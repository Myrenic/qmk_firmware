/* Copyright 2024 Jacky
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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
