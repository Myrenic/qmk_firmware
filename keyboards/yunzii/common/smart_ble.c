// Copyright 2024 QMK
// SPDX-License-Identifier: GPL-2.0-or-later

#include "quantum.h"
#include "uart.h"
#include "ch.h"
#include "hal.h"
#include "host.h"
#include "host_driver.h"
#include "report.h"
#include <string.h>
#include "smart_ble.h"
#include "print.h"

#define PRODUCT_NAME "YUNZII AL68"
#define WIRELESS_MODULE_WAKE_UP_BYTES_NUM 60

static bool wireless_connected = false;
static uint8_t ble_led_state = 0;

// UART parsing state
static enum {
    UART_READY,
    UART_0X55_RECEIVED,
    UART_LENS_RECEIVED,
    UART_WORKMODE,
    UART_REPORT_ID_RECEIVED
} uart_state = UART_READY;

static uint8_t uart_command[40];
static uint8_t uart_lens = 0;
static uint8_t uart_buff_index = 0;

// Non-blocking report stashing
static uint32_t last_send_time = 0;
static uint32_t last_activity_time = 0;
static uint32_t active_wireless_mode = 0;

typedef enum {
    REPORT_NONE,
    REPORT_KEYBOARD,
    REPORT_MOUSE,
    REPORT_EXTRA,
    REPORT_NKRO
} report_type_t;

static report_type_t pending_report_type = REPORT_NONE;
static uint8_t pending_report_data[20]; // Big enough for all report types
static uint8_t pending_report_len = 0;

void smart_ble_task(void) {
    // 1. Process incoming UART
    while (uart_available()) {
        uint8_t c = uart_read();
        switch (uart_state) {
            case UART_READY:
                if (c == 0x55) {
                    uart_state = UART_0X55_RECEIVED;
                    uart_buff_index = 0;
                    uart_command[uart_buff_index++] = c;
                }
                break;
            case UART_0X55_RECEIVED:
                uart_lens = c;
                uart_command[uart_buff_index++] = c;
                uart_state = UART_LENS_RECEIVED;
                break;
            case UART_LENS_RECEIVED:
                uart_command[uart_buff_index++] = c;
                uart_state = UART_WORKMODE;
                break;
            case UART_WORKMODE:
                uart_command[uart_buff_index++] = c;
                uart_state = UART_REPORT_ID_RECEIVED;
                break;
            case UART_REPORT_ID_RECEIVED:
                uart_command[uart_buff_index++] = c;
                if (uart_buff_index >= uart_lens + 2) {
                    uint8_t report_id = uart_command[2];
                    uint8_t data = c;
                    switch (report_id) {
                        case 0: wireless_connected = (data == 0); break;
                        case 1: ble_led_state = data; break;
                    }
                    uart_state = UART_READY;
                }
                break;
            default: uart_state = UART_READY; break;
        }
    }

    // 2. Process outgoing Pending Reports (Non-blocking)
    if (pending_report_type != REPORT_NONE) {
        uint32_t delay = (active_wireless_mode == 4) ? 2 : 8;
        if (timer_elapsed32(last_send_time) >= delay) {
            // Wakeup module if needed (can be blocking since it's only every 10s)
            if (timer_elapsed32(last_activity_time) > 10000) {
                for (int i = 0; i < WIRELESS_MODULE_WAKE_UP_BYTES_NUM; i++) uart_write(0x00);
                wait_ms(10);
            }
            last_activity_time = timer_read32();

            // Send stored packet
            uart_write(0x55);
            if (pending_report_type == REPORT_KEYBOARD) {
                uart_write(0x09);
                uart_write(0x01);
            } else {
                uart_write(pending_report_len);
            }
            uart_transmit(pending_report_data, pending_report_len);
            
            last_send_time = timer_read32();
            pending_report_type = REPORT_NONE;
        }
    }
}

static uint8_t sc_ble_leds(void) {
    return ble_led_state; 
}

static void sc_ble_mouse(report_mouse_t *report) {
    if (!wireless_connected) return;
    pending_report_type = REPORT_MOUSE;
    pending_report_len = sizeof(report_mouse_t);
    memcpy(pending_report_data, report, pending_report_len);
}

static void sc_ble_extra(report_extra_t *report) {
    if (!wireless_connected) return;
    pending_report_type = REPORT_EXTRA;
    pending_report_len = sizeof(report_extra_t);
    memcpy(pending_report_data, report, pending_report_len);
}

static void sc_ble_keyboard(report_keyboard_t *report) {
    if (!wireless_connected) return;
    pending_report_type = REPORT_KEYBOARD;
    pending_report_len = KEYBOARD_REPORT_SIZE;
    memcpy(pending_report_data, report, pending_report_len);
}

static void sc_send_nkro(report_nkro_t *report) {
    if (!wireless_connected) return;
    pending_report_type = REPORT_NKRO;
    pending_report_len = 0x12; // NKRO report size for this module
    memcpy(pending_report_data, report, pending_report_len);
}

static host_driver_t *last_host_driver = NULL;
static host_driver_t  sc_ble_driver    = {sc_ble_leds, sc_ble_keyboard, sc_send_nkro, sc_ble_mouse, sc_ble_extra};

void smart_ble_startup(void) {
    if (host_get_driver() == &sc_ble_driver) return;
    clear_keyboard();
    last_host_driver = host_get_driver();
    host_set_driver(&sc_ble_driver);
}

void smart_ble_disconnect(void) {
    if (host_get_driver() != &sc_ble_driver) return;
    clear_keyboard();
    host_set_driver(last_host_driver);
    wireless_connected = false;
    pending_report_type = REPORT_NONE;
}

void sc_ble_battary(uint8_t batt_level) {
    if (wireless_connected) {
        uart_write(0x55);
        uart_write(0x02);
        uart_write(0x09);
        uart_write(batt_level);
        // Batterty isn't critical, we can wait or just skip the wait if we assume low frequency
    }
}

void wireless_pair(uint32_t mode) {
    smart_ble_startup();
    wireless_connected = false;
    for (int i = 0; i < WIRELESS_MODULE_WAKE_UP_BYTES_NUM; i++) uart_write(0x00);
    wait_ms(350);
    for (int i = 0; i < 2; i++) {
        uart_write(0x55);
        uart_write(0x03);
        uart_write(0x00);
        uart_write(mode);
        uart_write(0x01);
        wait_ms(10);
    }
}

void wireless_start(uint32_t mode) {
    uint8_t ble_command[30];
    smart_ble_startup();
    wireless_connected = false;
    last_activity_time = timer_read32();
    if (mode < 1 || mode > 4) mode = 1;
    active_wireless_mode = mode;
    for (int i = 0; i < WIRELESS_MODULE_WAKE_UP_BYTES_NUM; i++) uart_write(0x00);
    wait_ms(350);
    memset(ble_command, 0, sizeof(ble_command));
    ble_command[0] = 0x55;
    ble_command[1] = 20;
    ble_command[2] = 0;
    ble_command[3] = mode;
    strcpy((char *)(ble_command + 4), PRODUCT_NAME);
    size_t name_len = strlen(PRODUCT_NAME);
    ble_command[4 + name_len] = '-';
    ble_command[4 + name_len + 1] = '0' + mode;
    ble_command[4 + name_len + 2] = 0;
    uart_transmit(ble_command, 22);
    wait_ms(10);
    uart_transmit(ble_command, 22);
}

void wireless_stop(void) {
    uint8_t ble_command[4];
    wireless_connected = false;
    for (int i = 0; i < WIRELESS_MODULE_WAKE_UP_BYTES_NUM; i++) uart_write(0x00);
    wait_ms(100);
    smart_ble_disconnect();
    wait_ms(20);
    ble_command[0] = 0x55;
    ble_command[1] = 2;
    ble_command[2] = 0;
    ble_command[3] = 0;
    uart_transmit(ble_command, 4);
}

bool is_wireless_connected(void) {
    return wireless_connected;
}
