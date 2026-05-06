// Copyright 2024 QMK
// SPDX-License-Identifier: GPL-2.0-or-later

#include "quantum.h"
#include "uart.h"
#include "deferred_exec.h"
#include "common/smart_ble.h"
#include "al68.h"
#include "hal.h"
#include "print.h"

yunzii_al68_state_t al68_state = {0};

// ─── Battery ADC (raw register access, no ChibiOS ADC driver) ───────────────

static uint32_t battery_test_timer = 0;
static uint16_t battery_mv = 0;
static bool adc_initialized = false;

void al68_battery_init(void) {
    palSetLineMode(BATTERY_ADC_PIN, PAL_MODE_INPUT_ANALOG);

    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    // ADC prescaler: PCLK2/8 = 9MHz (must be ≤14MHz)
    RCC->CFGR = (RCC->CFGR & ~(3u << 14)) | (3u << 14);

    ADC1->CR1 = 0; // Independent mode, no scan, no interrupts
    ADC1->CR2 = ADC_CR2_ADON | ADC_CR2_EXTTRIG | (7u << 17); // SW trigger, right-aligned
    ADC1->SQR1 = 0; // 1 conversion in sequence
    // Channel 9 (B1) sample time = 239.5 cycles for stability
    ADC1->SMPR2 |= (7u << (3 * BATTERY_ADC_CHANNEL));
    // Channel 17 (VREFINT) sample time
    ADC1->SMPR1 |= (7u << (3 * (17 - 10)));
    // Enable internal VREFINT
    ADC1->CR2 |= (1u << 23); // TSVREFE

    // Calibrate
    ADC1->CR2 |= ADC_CR2_RSTCAL;
    for (volatile int i = 0; i < 10000 && (ADC1->CR2 & ADC_CR2_RSTCAL); i++);
    ADC1->CR2 |= ADC_CR2_CAL;
    for (volatile int i = 0; i < 10000 && (ADC1->CR2 & ADC_CR2_CAL); i++);

    adc_initialized = true;
}

static uint16_t al68_adc_read(uint8_t channel) {
    if (!adc_initialized) return 0;
    ADC1->SQR3 = channel;
    ADC1->SR = 0; // Clear flags
    ADC1->CR2 |= ADC_CR2_SWSTART;
    // Wait with timeout (~5ms at worst)
    for (volatile int timeout = 0; timeout < 50000; timeout++) {
        if (ADC1->SR & ADC_SR_EOC) {
            return (uint16_t)ADC1->DR;
        }
    }
    return 0; // Timeout — return safe value
}

static uint16_t al68_read_battery_mv(void) {
    uint16_t vbat_raw = al68_adc_read(BATTERY_ADC_CHANNEL);
    uint16_t vref_raw = al68_adc_read(17);
    if (vref_raw == 0) return battery_mv; // Keep last known value on failure
    // Vendor formula: battery_value = (adc_value * 1764 / adc_vref)
    return (uint16_t)((uint32_t)vbat_raw * 1764 / vref_raw);
}

uint8_t al68_battery_level(void) {
    if (battery_mv <= BATT_MV_OFF) return 0;
    if (battery_mv <= BATT_MV_5)   return 5;
    if (battery_mv <= BATT_MV_10)  return 10;
    if (battery_mv <= BATT_MV_40)  return 40;
    if (battery_mv <= BATT_MV_60)  return 60;
    if (battery_mv <= BATT_MV_80)  return 80;
    if (battery_mv <= BATT_MV_85)  return 85;
    if (battery_mv >= BATT_MV_100) return 100;
    return 85 + (uint8_t)((uint32_t)(battery_mv - BATT_MV_85) * 15 / (BATT_MV_100 - BATT_MV_85));
}

static void al68_battery_task(void) {
    if (timer_elapsed32(battery_test_timer) < 30000) return;
    battery_test_timer = timer_read32();

    battery_mv = al68_read_battery_mv();
    uint8_t new_level = al68_battery_level();

    // Smooth: allow max 1% change per reading
    if (new_level > al68_state.battery_percent + 1) {
        al68_state.battery_percent++;
    } else if (new_level + 1 < al68_state.battery_percent && al68_state.battery_percent > 0) {
        al68_state.battery_percent--;
    } else {
        al68_state.battery_percent = new_level;
    }

    al68_state.low_battery = (al68_state.battery_percent <= 10);

    // Report to BLE MCU
    if (al68_state.current_mode >= 1 && al68_state.current_mode <= 3 && al68_state.is_connected) {
        sc_ble_battery(al68_state.battery_percent);
    }
}

// ─── Sleep Mode ─────────────────────────────────────────────────────────────

static bool rgb_is_off_for_sleep       = false;
static bool rgb_restore_after_idle     = false;
static bool rgb_restore_after_suspend  = false;

static bool al68_matrix_has_activity(void) {
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        if (matrix_get_row(row) != 0) {
            return true;
        }
    }
    return false;
}

static void al68_restore_idle_rgb(void) {
    if (!rgb_is_off_for_sleep) {
        return;
    }

    rgb_is_off_for_sleep = false;
    if (rgb_restore_after_idle) {
        rgb_matrix_enable_noeeprom();
        rgb_restore_after_idle = false;
    }
}

static void al68_note_activity(void) {
    al68_state.idle_timer  = timer_read32();
    al68_state.is_sleeping = false;
    al68_restore_idle_rgb();
}

static void al68_apply_rgb_profile(void) {
    if (al68_state.current_mode == 0) {
        rgb_matrix_mode_noeeprom(RGB_MATRIX_CYCLE_LEFT_RIGHT);
        rgb_matrix_sethsv_noeeprom(0, 255, 255);
        rgb_matrix_set_speed_noeeprom(32);
    } else {
        rgb_matrix_mode_noeeprom(RGB_MATRIX_SOLID_REACTIVE_MULTIWIDE);
        rgb_matrix_sethsv_noeeprom(HSV_AZURE);
        rgb_matrix_set_speed_noeeprom(16);
    }
}

static uint32_t al68_configure_stop_wake_sources(void) {
    // Configure matrix for wake detection (ROW2COL: cols output low, rows input high)
    const pin_t col_pins[] = MATRIX_COL_PINS;
    for (uint8_t x = 0; x < MATRIX_COLS; x++) {
        setPinOutput(col_pins[x]);
        writePinLow(col_pins[x]);
    }

    const pin_t row_pins[] = MATRIX_ROW_PINS;
    for (uint8_t x = 0; x < MATRIX_ROWS; x++) {
        setPinInputHigh(row_pins[x]);
    }

    // Row pins: A0-A4, mode switches: PC14/PC15, encoder: A6/A7, USB plug: B9.
    AFIO->EXTICR[0] = (AFIO->EXTICR[0] & 0x0000) | 0x0000; // PA0-PA3
    AFIO->EXTICR[1] = (AFIO->EXTICR[1] & 0xFFF0) | 0x0000; // PA4

    setPinInput(BLE_MODE_PIN);
    setPinInput(G24_MODE_PIN);
    AFIO->EXTICR[3] = (AFIO->EXTICR[3] & 0x00FF) | 0x2200; // PC14, PC15

    setPinInput(A6);
    setPinInput(A7);
    AFIO->EXTICR[1] = (AFIO->EXTICR[1] & 0x00FF) | 0x0000; // PA6, PA7

    setPinInput(USB_PLUG_PIN);
    AFIO->EXTICR[2] = (AFIO->EXTICR[2] & 0xF0FF) | 0x0100; // PB9

    uint32_t wake_lines = (1u << 0) | (1u << 1) | (1u << 2) | (1u << 3) | (1u << 4)
                        | (1u << 6) | (1u << 7)
                        | (1u << 9)
                        | (1u << 14) | (1u << 15);

    EXTI->FTSR |= wake_lines;
    EXTI->RTSR |= (1u << 6) | (1u << 7) | (1u << 9) | (1u << 14) | (1u << 15);
    EXTI->IMR |= wake_lines;
    EXTI->PR = wake_lines;

    return wake_lines;
}

static void al68_restore_after_stop(uint32_t wake_lines) {
    SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
    EXTI->PR = wake_lines;
    wait_ms(10);

    stm32_clock_init();

    AFIO->MAPR = (AFIO->MAPR & ~AFIO_MAPR_SWJ_CFG_Msk);
    AFIO->MAPR |= AFIO_MAPR_SWJ_CFG_DISABLE;

    al68_battery_init();
    matrix_init();
    uart_init(460800);
    wait_ms(50);

    al68_state.idle_timer  = timer_read32();
    al68_state.is_sleeping = false;
}

void al68_enter_sleep(void) {
    al68_state.is_sleeping = true;

    // Turn off RGB to save power
    rgb_matrix_disable_noeeprom();

    // Turn off LED power pin
    setPinOutput(A5);
    writePinLow(A5);

    // Send sleep command to wireless MCU
    if (al68_state.current_mode != 0) {
        uint8_t cmd[4] = {0x55, 0x02, 0x00, 0x00};
        for (int i = 0; i < 60; i++) uart_write(0x00);
        wait_ms(10);
        uart_transmit(cmd, 4);
        wait_ms(20);
    }

    // Disable ADC
    ADC1->CR2 &= ~ADC_CR2_ADON;
    adc_initialized = false;

    // Disable USB pull-up
    writePinLow(A8);

    uint32_t wake_lines = al68_configure_stop_wake_sources();

    // Enter STOP mode
    PWR->CR = (PWR->CR & ~PWR_CR_PDDS) | PWR_CR_LPDS;
    PWR->CR |= PWR_CR_CWUF | PWR_CR_CSBF;
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
    __WFI();

    al68_restore_after_stop(wake_lines);

    // Re-enable USB
    setPinOutput(A8);
    writePinHigh(A8);

    // Re-enable LED power + RGB
    setPinOutput(A5);
    writePinHigh(A5);
    if (rgb_restore_after_idle) {
        al68_apply_rgb_profile();
        rgb_matrix_enable_noeeprom();
        rgb_restore_after_idle = false;
    }

    // Determine mode and restart wireless
    bool ble_pin = readPin(BLE_MODE_PIN);
    bool g24_pin = readPin(G24_MODE_PIN);
    if (!ble_pin) {
        uint8_t profile = al68_state.current_mode;
        if (profile < 1 || profile > 3) profile = 1;
        wireless_start(profile);
        al68_state.current_mode = profile;
    } else if (!g24_pin) {
        wireless_start(4);
        al68_state.current_mode = 4;
    } else {
        wireless_stop();
        al68_state.current_mode = 0;
    }

    al68_apply_rgb_profile();

    // Reset state
    rgb_is_off_for_sleep   = false;
}

static void al68_enter_usb_suspend_sleep(void) {
    uint32_t wake_lines;

    al68_state.is_sleeping = true;

    writePinLow(A5);
    ADC1->CR2 &= ~ADC_CR2_ADON;
    adc_initialized = false;

    wake_lines = al68_configure_stop_wake_sources();

    PWR->CR = (PWR->CR & ~PWR_CR_PDDS) | PWR_CR_LPDS;
    PWR->CR |= PWR_CR_CWUF | PWR_CR_CSBF;
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
    __WFI();

    al68_restore_after_stop(wake_lines);

    setPinOutput(A5);
    writePinHigh(A5);
    if (rgb_restore_after_suspend) {
        al68_apply_rgb_profile();
        rgb_matrix_enable_noeeprom();
        rgb_restore_after_suspend = false;
    }
}

static void al68_sleep_task(void) {
    // Only sleep in wireless modes when not plugged in
    if (al68_state.current_mode == 0) return;
    if (readPin(USB_PLUG_PIN)) return; // USB plugged in, don't sleep
    if (al68_state.is_pairing || al68_state.profile_timer) return;

    // Never sleep while keys are still physically held.
    if (al68_matrix_has_activity()) {
        al68_note_activity();
        return;
    }

    uint32_t idle_elapsed = timer_elapsed32(al68_state.idle_timer);

    // Stage 1: Turn off RGB after idle timeout
    if (!rgb_is_off_for_sleep && idle_elapsed > SLEEP_RGB_OFF_TIMEOUT) {
        rgb_restore_after_idle = rgb_matrix_is_enabled();
        if (rgb_restore_after_idle) {
            rgb_matrix_disable_noeeprom();
        }
        rgb_is_off_for_sleep = true;
    }

    // If wireless mode is selected but not connected, sleep earlier to avoid draining the battery.
    if (!al68_state.is_connected && idle_elapsed > SLEEP_DISCONNECTED_TIMEOUT) {
        al68_enter_sleep();
        return;
    }

    // Stage 2: Deep sleep after extended idle while connected.
    if (idle_elapsed > SLEEP_IDLE_TIMEOUT) {
        al68_enter_sleep();
    }
}

// ─── Deferred Callbacks ─────────────────────────────────────────────────────

uint32_t profile_timer_callback(uint32_t trigger_time, void *cb_arg) {
    al68_state.profile_timer = 0;
    return 0;
}

uint32_t pairing_timer_callback(uint32_t trigger_time, void *cb_arg) {
    al68_state.is_pairing = false;
    return 0;
}

// ─── QMK Hooks ──────────────────────────────────────────────────────────────

void keyboard_pre_init_kb(void) {
    // Clear bootloader flag
    BKP->DR10 = 0;

    // Disable JTAG+SWD to free PA14, PA15, PB3 for matrix use
    // Must be two separate writes (STM32F1 SWJ_CFG bits are write-only)
    AFIO->MAPR = (AFIO->MAPR & ~AFIO_MAPR_SWJ_CFG_Msk);
    AFIO->MAPR |= AFIO_MAPR_SWJ_CFG_DISABLE;

    // USB pull-up
    setPinOutput(A8);
    writePinHigh(A8);

    // Mode switch pins
    setPinInputHigh(BLE_MODE_PIN);
    setPinInputHigh(G24_MODE_PIN);
    setPinInputHigh(USB_PLUG_PIN);

    // LED power enable
    setPinOutput(A5);
    writePinHigh(A5);

    uart_init(460800);
    wait_ms(400);

    keyboard_pre_init_user();
}

void keyboard_post_init_kb(void) {
    // Initialize battery ADC
    al68_battery_init();
    battery_mv = al68_read_battery_mv();
    al68_state.battery_percent = al68_battery_level();
    al68_state.low_battery = (al68_state.battery_percent <= 10);

    // Start in USB mode
    al68_state.current_mode = 0;
    wireless_stop();
    al68_apply_rgb_profile();

    // Initialize idle timer
    al68_state.idle_timer = timer_read32();
    battery_test_timer = timer_read32();

    keyboard_post_init_user();
}

void housekeeping_task_kb(void) {
    if (al68_matrix_has_activity()) {
        al68_note_activity();
    }

    // Process UART from wireless MCU
    smart_ble_task();

    // Poll mode switch
    uint8_t new_mode = 0;
    bool ble_pin = readPin(BLE_MODE_PIN);
    bool g24_pin = readPin(G24_MODE_PIN);

    if (!ble_pin) {
        new_mode = (al68_state.current_mode >= 1 && al68_state.current_mode <= 3)
                   ? al68_state.current_mode : 1;
    } else if (!g24_pin) {
        new_mode = 4;
    }

    // Handle mode change
    if (new_mode != al68_state.current_mode) {
        if (new_mode == 0) {
            wireless_stop();
        } else {
            wireless_start(new_mode);
            al68_state.profile_timer = timer_read32();
            defer_exec(1500, profile_timer_callback, NULL);
        }
        al68_state.current_mode = new_mode;
        al68_state.is_pairing = false;
        al68_apply_rgb_profile();

        // Wake RGB if it was off
        al68_restore_idle_rgb();
    }

    // Track connection state
    bool was_connected = al68_state.is_connected;
    al68_state.is_connected = is_wireless_connected();

    // On new connection: send clear to prevent stuck keys
    if (!was_connected && al68_state.is_connected) {
        smart_ble_send_clear();
    }

    if (al68_state.is_pairing && al68_state.is_connected) {
        al68_state.is_pairing = false;
    }

    // Battery monitoring
    al68_battery_task();

    // Sleep management
    al68_sleep_task();

    housekeeping_task_user();
}

bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
    if (record->event.pressed) {
        // Reset idle timer on any keypress and restore auto-dimmed RGB.
        al68_note_activity();

        // Poke wireless MCU on keypress when not connected (helps reconnect)
        if (al68_state.current_mode != 0) {
            smart_ble_keepalive();
        }

        switch (keycode) {
            case KC_PAIR:
                if (al68_state.current_mode != 0) {
                    wireless_pair(al68_state.current_mode);
                    al68_state.is_pairing = true;
                    defer_exec(60000, pairing_timer_callback, NULL);
                }
                return false;

            case KC_BLE1:
            case KC_BLE2:
            case KC_BLE3: {
                uint8_t target = keycode - KC_BLE1 + 1;
                if (al68_state.current_mode >= 1 && al68_state.current_mode <= 3) {
                    if (al68_state.current_mode != target) {
                        wireless_start(target);
                        al68_state.current_mode = target;
                        al68_state.profile_timer = timer_read32();
                        defer_exec(1500, profile_timer_callback, NULL);
                    }
                }
                return false;
            }

            case KC_24G:
                return false;

            case KC_BAT:
                al68_state.battery_showing = true;
                return false;

        }
    } else {
        if (keycode == KC_BAT) {
            al68_state.battery_showing = false;
            return false;
        }
    }
    return process_record_user(keycode, record);
}

void suspend_power_down_kb(void) {
    suspend_power_down_user();

    if (al68_state.current_mode == 0) {
        rgb_restore_after_suspend = rgb_matrix_is_enabled();
        if (rgb_restore_after_suspend) {
            rgb_matrix_disable_noeeprom();
        }
        al68_enter_usb_suspend_sleep();
    }
}

void suspend_wakeup_init_kb(void) {
    if (al68_state.current_mode == 0) {
        al68_apply_rgb_profile();
    }
    suspend_wakeup_init_user();
}

void bootloader_jump(void) {
    BKP->DR10 = RTC_BOOTLOADER_FLAG;
    NVIC_SystemReset();
}
