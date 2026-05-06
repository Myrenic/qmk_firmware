#pragma once

#include "quantum.h"

// Keyboard state
typedef struct {
    uint8_t  current_mode;      // 0=USB, 1=BLE1, 2=BLE2, 3=BLE3, 4=2.4G
    bool     is_pairing;        // True if currently in pairing mode
    bool     is_connected;      // True if wireless link is up
    uint32_t profile_timer;     // Timer for profile flash indication
    uint8_t  battery_percent;   // 0-100 battery level
    bool     battery_showing;   // True if battery display is active
    bool     low_battery;       // True if battery ≤ 10%
    bool     is_sleeping;       // True if in sleep preparation
    uint32_t idle_timer;        // Last keypress/activity timestamp
} yunzii_al68_state_t;

extern yunzii_al68_state_t al68_state;

// Wireless function prototypes
void wireless_pair(uint32_t mode);
void wireless_start(uint32_t mode);
void wireless_stop(void);
bool is_wireless_connected(void);

// Battery
uint8_t al68_battery_level(void);
void    al68_battery_init(void);

// Sleep
void al68_enter_sleep(void);

enum custom_keycodes {
    KC_USB = QK_KB_0,
    KC_BLE1,
    KC_BLE2,
    KC_BLE3,
    KC_24G,
    KC_PAIR,
    KC_BAT
};
