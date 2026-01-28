// Copyright 2024 QMK
// SPDX-License-Identifier: GPL-2.0-or-later

#include "quantum.h"
#include "uart.h"
#include "deferred_exec.h"
#include "common/smart_ble.h"
#include "al68.h"

yunzii_al68_active_state_t al68_state = {0};

static uint32_t debug_timer = 0;

void keyboard_pre_init_kb(void) {
    // boardInit() sets BKP->DR10 = RTC_BOOTLOADER_FLAG, which causes the bootloader
    // to stay in DFU mode on next boot. Clear it so normal reboot works.
    BKP->DR10 = 0;

    AFIO->MAPR = (AFIO->MAPR & ~AFIO_MAPR_SWJ_CFG_Msk);
    AFIO->MAPR |= AFIO_MAPR_SWJ_CFG_DISABLE; // disable JTAG

    // Enable USB Pull-up/Enable if implied by A8
    setPinOutput(A8);
    writePinHigh(A8);

    // Initialize Mode Switch Pins
    setPinInputHigh(BLE_MODE_PIN);
    setPinInputHigh(G24_MODE_PIN);
    setPinInputHigh(USB_PLUG_PIN);

    uart_init(460800);
    wait_ms(400);

    keyboard_pre_init_user();
}

void keyboard_post_init_kb(void) {
    // Initial check for mode based on pin states.
    // However, we do NOT call wireless_start() here immediately. 
    // We let the housekeeping task handle the transition after system stabilizes. 
    // This avoids race conditions where the module isn't ready.
    
    // Just enforce USB state initially (which stops wireless).
    al68_state.current_mode = 0; // KB_MODE_USB
    wireless_stop();

    keyboard_post_init_user();
}

// Deferred executor callbacks
uint32_t profile_timer_callback(uint32_t trigger_time, void *cb_arg) {
    al68_state.profile_timer = 0;
    return 0;
}

uint32_t pairing_timer_callback(uint32_t trigger_time, void *cb_arg) {
    al68_state.is_pairing = false;
    return 0;
}

void housekeeping_task_kb(void) {
    // Monitor UART
    smart_ble_task();

    // Poll Mode Switch
    uint8_t new_mode = 0; // KB_MODE_USB
    bool ble_pin = readPin(BLE_MODE_PIN);
    bool g24_pin = readPin(G24_MODE_PIN);

    if (!ble_pin) {
        if (al68_state.current_mode == 0 || al68_state.current_mode == 4) {
             new_mode = 1; // Default to BLE 1
        } else {
             new_mode = al68_state.current_mode;
        }
    } else if (!g24_pin) {
        new_mode = 4; // KB_MODE_24G
    }

    if (timer_elapsed32(debug_timer) > 1000) {
        debug_timer = timer_read32();
    }

    if (new_mode != al68_state.current_mode) {
#ifdef CONSOLE_ENABLE
        uprintf("Mode Change: %d -> %d\n", al68_state.current_mode, new_mode);
#endif
        if (new_mode == 0) {
            wireless_stop();
        } else {
            wireless_start(new_mode);
            al68_state.profile_timer = timer_read32();
            defer_exec(1500, profile_timer_callback, NULL);
        }
        al68_state.current_mode = new_mode;
        al68_state.is_pairing = false; 
    }
    
    al68_state.is_connected = is_wireless_connected();

    if (al68_state.is_pairing && al68_state.is_connected) {
        al68_state.is_pairing = false;
    }

    housekeeping_task_user();
}

bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
    if (record->event.pressed) {
        switch(keycode) {
            case KC_PAIR:
                if (al68_state.current_mode != 0) { // If wireless
                    wireless_pair(al68_state.current_mode);
                    al68_state.is_pairing = true;
                    defer_exec(60000, pairing_timer_callback, NULL);
                }
                return false;
            
            case KC_BLE1:
            case KC_BLE2:
            case KC_BLE3: {
                uint8_t target_profile = keycode - KC_BLE1 + 1;
                if (al68_state.current_mode != 0 && al68_state.current_mode != 4) { // BLE hardware mode
                    if (al68_state.current_mode != target_profile) {
                        wireless_start(target_profile);
                        al68_state.current_mode = target_profile;
                        al68_state.profile_timer = timer_read32();
                        defer_exec(1500, profile_timer_callback, NULL);
                    }
                }
                return false;
            }
            case KC_24G:
                return false;
        }
    }
    return process_record_user(keycode, record);
}

void bootloader_jump(void) {
    BKP->DR10 = RTC_BOOTLOADER_FLAG;
    NVIC_SystemReset();
}
