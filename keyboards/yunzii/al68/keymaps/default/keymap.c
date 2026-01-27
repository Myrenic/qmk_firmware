/* Copyright 2022 Jacky
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
#include QMK_KEYBOARD_H

#if defined(ENCODER_MAP_ENABLE)
const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][NUM_DIRECTIONS] = {
    [0] = { ENCODER_CCW_CW(LCTL(LGUI(KC_RIGHT)),LCTL(LGUI(KC_LEFT))) },
    [1] = { ENCODER_CCW_CW(_______, _______) }
};
#endif


const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    /* Layer 0: Default
     * ,----------------------------------------------------------------.
     * |Esc| 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 0 | - | = |Bksp |Mute|
     * |----------------------------------------------------------------|
     * |Tab  | Q | W | E | R | T | Y | U | I | O | P | [ | ] |  \  |Del |
     * |----------------------------------------------------------------|
     * |Caps  | A | S | D | F | G | H | J | K | L | ; | ' | Enter |PgUp|
     * |----------------------------------------------------------------|
     * |Shift   | Z | X | C | V | B | N | M | , | . | / |Shift| Up |PgDn|
     * |----------------------------------------------------------------|
     * |Ctrl|Gui |Alt |       Space        |Fn1 |Ctrl|   |Lft|Dwn|Rgt |
     * `----------------------------------------------------------------'
     */
    [0] = LAYOUT_65_ansi_blocker(
    KC_ESC,     KC_1,       KC_2,       KC_3,       KC_4,       KC_5,       KC_6,       KC_7,       KC_8,       KC_9,       KC_0,       KC_MINS,    KC_EQL,     KC_BSPC,    KC_MUTE,
    KC_TAB,     KC_Q,       KC_W,       KC_E,       KC_R,       KC_T,       KC_Y,       KC_U,       KC_I,       KC_O,       KC_P,       KC_LBRC,    KC_RBRC,    KC_BSLS,    KC_DELETE,
    KC_CAPS,    KC_A,       KC_S,       KC_D,       KC_F,       KC_G,       KC_H,       KC_J,       KC_K,       KC_L,       KC_SCLN,    KC_QUOT,                KC_ENT,     KC_PAGE_UP,
    KC_LSFT,    KC_Z,       KC_X,       KC_C,       KC_V,       KC_B,       KC_N,       KC_M,       KC_COMM,    KC_DOT,     KC_SLSH,    KC_RSFT,                KC_UP,      KC_PAGE_DOWN,
    KC_LCTL,    KC_LGUI,    KC_LALT,                            KC_SPC,                             MO(1),      KC_RCTL,                            KC_LEFT,    KC_DOWN,    KC_RIGHT    
    ),
    /* Layer 1: FN
     * ,----------------------------------------------------------------.
     * | ` |Br-|Br+|   |MyC|Mal|Who|Prv|Ply|Nxt|   |Vl-|Vl+|     |    |
     * |----------------------------------------------------------------|
     * |     |BT1|BT2|BT3|24G|   |   |   |   |   |Par|   |   |Nxt |    |
     * |----------------------------------------------------------------|
     * |      |   |   |   |   |   |   |   |   |   |   |   |     |Hue+ |
     * |----------------------------------------------------------------|
     * |        |   |   |   |   |   |   |   |   |   |   |     |Val+ |    |
     * |----------------------------------------------------------------|
     * |    |    |    |        RM_TOGG        |    |    |   |Sp-|Val-|Sp+|
     * `----------------------------------------------------------------'
     */
    [1] = LAYOUT_65_ansi_blocker(
    KC_GRV,     KC_BRID,    KC_BRIU,    _______,    KC_MYCM,    KC_MAIL,    KC_WHOM,    KC_MPRV,    KC_MPLY,    KC_MNXT,    _______,    KC_VOLD,    KC_VOLU,    _______,    _______,
    _______,    KC_BLE1,    KC_BLE2,    KC_BLE3,    KC_24G,     _______,    _______,    _______,    _______,    _______,    KC_PAIR,    _______,    _______,    RM_NEXT,    _______,
    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,                RM_HUEU,    _______,
    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,                RM_VALU,    _______,
    _______,    _______,     _______,                           RM_TOGG,                           _______,    _______,                            RM_SPDD,    RM_VALD,    RM_SPDU
    )
};

#ifdef RGB_MATRIX_ENABLE
bool rgb_matrix_indicators_user(void) {
    if (host_keyboard_led_state().caps_lock) {
        rgb_matrix_set_color(25, 255, 0, 0); // Caps Lock red
    }

    if (al68_state.current_mode != 0) { // Wireless mode
        if (al68_state.is_pairing) {
            // Flash 'P' key (Index 43)
             if ((timer_read32() / 250) % 2) {
                rgb_matrix_set_color(43, 0, 0, 255); // Blue flash
            } else {
                rgb_matrix_set_color(43, 0, 0, 0);
            }
        }
        
        // Mode specific background override
        // Only override if we are in the "profile view" timeout
        if (al68_state.profile_timer > 0) {
            if (al68_state.current_mode == 4) {
                 rgb_matrix_sethsv_noeeprom(HSV_GREEN);
            } else {
                switch(al68_state.current_mode) {
                    case 1: rgb_matrix_sethsv_noeeprom(HSV_BLUE); break;
                    case 2: rgb_matrix_sethsv_noeeprom(HSV_CYAN); break;
                    case 3: rgb_matrix_sethsv_noeeprom(HSV_PURPLE); break;
                }
            }
        }
    }
    return false;
}
#endif
