#pragma once

#include "quantum.h"

// Shared state for Wireless status
typedef struct {
    uint8_t current_mode;       // 0=USB, 1=BLE1, 2=BLE2, 3=BLE3, 4=2.4G
    bool is_pairing;            // True if currently in pairing mode
    bool is_connected;          // True if wireless link is up
    uint32_t profile_timer;     // Timer for profile flash indication
} yunzii_al68_active_state_t;

extern yunzii_al68_active_state_t al68_state;

// Wireless function prototypes
void wireless_pair(uint32_t mode);
void wireless_start(uint32_t mode);
void wireless_stop(void);
bool is_wireless_connected(void);

enum custom_keycodes {
    KC_USB = QK_KB_0,
    KC_BLE1,
    KC_BLE2,
    KC_BLE3,
    KC_24G,
    KC_PAIR
};
