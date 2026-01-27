// Copyright 2024 QMK
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once


#define WS2812_PWM_DRIVER PWMD4
#define WS2812_PWM_CHANNEL 3
#define WS2812_PWM_PAL_MODE 2
#define WS2812_DMA_STREAM STM32_DMA1_STREAM7
#define WS2812_DMA_CHANNEL 7

/* UART Config for Wireless */
#define SERIAL_DRIVER SD1
#define SD1_TX_PAL_MODE PAL_MODE_ALTERNATE_PUSHPULL
#define UART_DRIVER_REQUIRED yes

/* Mode Switch Pins */
#define BLE_MODE_PIN C15
#define G24_MODE_PIN C14
#define USB_PLUG_PIN B9

