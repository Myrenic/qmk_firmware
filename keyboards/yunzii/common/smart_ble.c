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
#define UART_MAX_FRAME_LEN 0x20
#define UART_STATUS_FRAME_LEN 0x03
#define WIRELESS_NKRO_REPORT_LEN 0x15

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

#define BLE_QUEUE_SIZE 32

typedef struct {
    report_type_t type;
    uint8_t data[32];
    uint8_t len;
} ble_report_entry_t;

static ble_report_entry_t ble_queue[BLE_QUEUE_SIZE];
static uint8_t ble_queue_head = 0;
static uint8_t ble_queue_tail = 0;

static void ble_queue_push(report_type_t type, const void *data, uint8_t len) {
    uint8_t next = (ble_queue_head + 1) % BLE_QUEUE_SIZE;
    if (next == ble_queue_tail) {
        // Queue full, drop oldest or ignore newest? 
        // For keyboard reports, dropping oldest is usually better, but here we just ignore to keep it simple and avoid complex sync.
        return;
    }
    ble_queue[ble_queue_head].type = type;
    ble_queue[ble_queue_head].len = len;
    memcpy(ble_queue[ble_queue_head].data, data, len);
    ble_queue_head = next;
}

void smart_ble_task(void) {
    // 1. Process incoming UART
    while (uart_available()) {
        uint8_t c = uart_read();

        /* Prevent buffer overflow on malformed streams. */
        if (uart_buff_index >= sizeof(uart_command)) {
            uart_state = UART_READY;
            uart_buff_index = 0;
            uart_lens = 0;
        }

        switch (uart_state) {
            case UART_READY:
                if (c == 0x55) {
                    uart_state = UART_0X55_RECEIVED;
                    uart_buff_index = 0;
                    uart_command[uart_buff_index++] = c;
                }
                break;
            case UART_0X55_RECEIVED:
                /* Ignore extra sync bytes. */
                if (c == 0x55) {
                    break;
                }

                uart_lens = c;
                if (uart_lens < 0x02 || uart_lens > UART_MAX_FRAME_LEN || (uart_lens + 2) > sizeof(uart_command)) {
                    uart_state = UART_READY;
                    uart_buff_index = 0;
                    uart_lens = 0;
                    break;
                }

                uart_command[uart_buff_index++] = c;
                uart_state = UART_LENS_RECEIVED;
                break;
            case UART_LENS_RECEIVED:
                /* Command ID (0..2 in vendor firmware). */
                if (c > 2) {
                    uart_state = UART_READY;
                    uart_buff_index = 0;
                    uart_lens = 0;
                    break;
                }

                uart_command[uart_buff_index++] = c;
                uart_state = UART_WORKMODE;
                break;
            case UART_WORKMODE:
                /* Workmode (0..4). */
                if (c > 4) {
                    uart_state = UART_READY;
                    uart_buff_index = 0;
                    uart_lens = 0;
                    break;
                }

                uart_command[uart_buff_index++] = c;
                uart_state = UART_REPORT_ID_RECEIVED;
                break;
            case UART_REPORT_ID_RECEIVED:
                uart_command[uart_buff_index++] = c;
                if (uart_buff_index >= uart_lens + 2) {
                    /* Vendor status frames are 0x55 0x03 <cmd> <workmode> <data>. */
                    if (uart_lens == UART_STATUS_FRAME_LEN) {
                        uint8_t cmd = uart_command[2];
                        uint8_t workmode = uart_command[3];
                        uint8_t data = uart_command[4];

                        /* Ignore stale updates for other profiles. */
                        if (workmode == active_wireless_mode) {
                            switch (cmd) {
                                case 0:
                                    wireless_connected = (data == 0);
                                    break;
                                case 1:
                                    ble_led_state = data;
                                    break;
                                default:
                                    break;
                            }
                        }
                    }

                    uart_state = UART_READY;
                    uart_buff_index = 0;
                    uart_lens = 0;
                }
                break;
            default: uart_state = UART_READY; break;
        }
    }

    // 2. Process outgoing Pending Reports (Non-blocking)
    if (ble_queue_head != ble_queue_tail) {
        uint32_t delay = (active_wireless_mode == 4) ? 2 : 8;
        if (timer_elapsed32(last_send_time) >= delay) {
            ble_report_entry_t *entry = &ble_queue[ble_queue_tail];
            
            // Wakeup module if needed (can be blocking since it's only every 10s)
            if (timer_elapsed32(last_activity_time) > 10000) {
                for (int i = 0; i < WIRELESS_MODULE_WAKE_UP_BYTES_NUM; i++) uart_write(0x00);
                wait_ms(10);
            }
            last_activity_time = timer_read32();

            // Send stored packet
            uart_write(0x55);
            if (entry->type == REPORT_KEYBOARD) {
                uart_write(0x09);
                uart_write(0x01);
            } else {
                uart_write(entry->len);
            }
            uart_transmit(entry->data, entry->len);
            
            last_send_time = timer_read32();
            ble_queue_tail = (ble_queue_tail + 1) % BLE_QUEUE_SIZE;
        }
    }
}


static uint8_t sc_ble_leds(void) {
    return ble_led_state; 
}

static void sc_ble_mouse(report_mouse_t *report) {
    if (!wireless_connected) return;
    ble_queue_push(REPORT_MOUSE, report, sizeof(report_mouse_t));
}

static void sc_ble_extra(report_extra_t *report) {
    if (!wireless_connected) return;
    ble_queue_push(REPORT_EXTRA, report, sizeof(report_extra_t));
}

static void sc_ble_keyboard(report_keyboard_t *report) {
    if (!wireless_connected) return;
    ble_queue_push(REPORT_KEYBOARD, report, KEYBOARD_REPORT_SIZE);
}

static void sc_send_nkro(report_nkro_t *report) {
    if (!wireless_connected) return;
    uint8_t len = WIRELESS_NKRO_REPORT_LEN;
    if (sizeof(report_nkro_t) < len) {
        len = sizeof(report_nkro_t);
    }
    ble_queue_push(REPORT_NKRO, report, len);
}

static host_driver_t *last_host_driver = NULL;
static host_driver_t  sc_ble_driver    = {sc_ble_leds, sc_ble_keyboard, sc_send_nkro, sc_ble_mouse, sc_ble_extra};

void smart_ble_startup(void) {
    if (host_get_driver() == &sc_ble_driver) return;
    clear_keyboard();
    ble_queue_head = 0;
    ble_queue_tail = 0;
    last_host_driver = host_get_driver();
    host_set_driver(&sc_ble_driver);
}

void smart_ble_disconnect(void) {
    if (host_get_driver() != &sc_ble_driver) return;
    clear_keyboard();
    host_set_driver(last_host_driver);
    wireless_connected = false;
    ble_queue_head = 0;
    ble_queue_tail = 0;
}

void sc_ble_battery(uint8_t batt_level) {
    if (wireless_connected) {
        uart_write(0x55);
        uart_write(0x02);
        uart_write(0x09);
        uart_write(batt_level);
        // Battery isn't critical; low-frequency report.
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
    active_wireless_mode = 0;
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
