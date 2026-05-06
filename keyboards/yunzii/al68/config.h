// Copyright 2024 QMK
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

/* WS2812 LED driver */
#define WS2812_PWM_DRIVER PWMD4
#define WS2812_PWM_CHANNEL 3
#define WS2812_PWM_PAL_MODE 2
#define WS2812_DMA_STREAM STM32_DMA1_STREAM7
#define WS2812_DMA_CHANNEL 7

/* Encoder */
#define ENCODER_RESOLUTION 4
#define ENCODER_DEFAULT_POS 0x3

/* UART Config for Wireless MCU */
#define SERIAL_DRIVER SD1
#define SD1_TX_PAL_MODE PAL_MODE_ALTERNATE_PUSHPULL
#define UART_DRIVER_REQUIRED yes

/* Mode Switch Pins */
#define BLE_MODE_PIN C15
#define G24_MODE_PIN C14
#define USB_PLUG_PIN B9

/* Battery ADC - pin B1, ADC channel 9 */
#define BATTERY_ADC_PIN B1
#define BATTERY_ADC_CHANNEL 9

/* Battery voltage thresholds (mV) */
#define BATT_MV_OFF   3000
#define BATT_MV_5     3130
#define BATT_MV_10    3180
#define BATT_MV_40    3630
#define BATT_MV_60    3760
#define BATT_MV_80    3930
#define BATT_MV_85    3980
#define BATT_MV_100   4150

/* Sleep timeouts (ms) */
#define SLEEP_RGB_OFF_TIMEOUT          300000  // 5 minutes: turn off RGB first
#define SLEEP_IDLE_TIMEOUT             900000  // 15 minutes: enter deep sleep when connected
#define SLEEP_DISCONNECTED_TIMEOUT      60000  // 1 minute: sleep sooner if wireless mode is idle/disconnected

/* RGB Matrix */
#define ENABLE_RGB_MATRIX_CYCLE_ALL
#define ENABLE_RGB_MATRIX_CYCLE_LEFT_RIGHT
#define ENABLE_RGB_MATRIX_SOLID_REACTIVE_MULTIWIDE
#define RGB_MATRIX_KEYPRESSES
#define RGB_MATRIX_MAXIMUM_BRIGHTNESS 200
#define RGB_MATRIX_DEFAULT_MODE RGB_MATRIX_SOLID_REACTIVE_MULTIWIDE
#define RGB_MATRIX_DEFAULT_SPD 16

/* Debounce */
#define DEBOUNCE 6
