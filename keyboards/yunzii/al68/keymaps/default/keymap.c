#include QMK_KEYBOARD_H

#if defined(ENCODER_MAP_ENABLE)
const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][NUM_DIRECTIONS] = {
    // Layer 0: Normal rotation (Desktop switching)
    [0] = { ENCODER_CCW_CW(LCTL(LGUI(KC_LEFT)), LCTL(LGUI(KC_RIGHT))) },
    // Layer 1: Rotation while holding the dial (Volume control)
    [1] = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU) }
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
    KC_ESC,     KC_1,       KC_2,       KC_3,       KC_4,       KC_5,       KC_6,       KC_7,       KC_8,       KC_9,       KC_0,       KC_MINS,    KC_EQL,     KC_BSPC,    LT(1, KC_MUTE),
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

void keyboard_post_init_user(void) {
    // Set default RGB state: Wide Reactive and slow fade for a "premium" feel
    rgb_matrix_mode_noeeprom(RGB_MATRIX_SOLID_REACTIVE_MULTIWIDE);
    rgb_matrix_sethsv_noeeprom(HSV_AZURE); 
    rgb_matrix_set_speed_noeeprom(16); // Slower fade
    rgb_matrix_enable_noeeprom();
}

#ifdef RGB_MATRIX_ENABLE
bool rgb_matrix_indicators_user(void) {
    uint8_t r = 0, g = 0, b = 0;
    bool is_mode_active = false;
    const uint8_t dim_val = 51; // 20% brightness

    // Determine Mode Color
    if (al68_state.current_mode == 0) {
        // Wired: Off
        r = 0; g = 0; b = 0;
        is_mode_active = false;
    } else {
        is_mode_active = true;
        switch (al68_state.current_mode) {
            case 1: // BLE 1: White
                r = dim_val; g = dim_val; b = dim_val;
                break;
            case 2: // BLE 2: Light Blue (Cyan-ish)
                r = 0; g = dim_val; b = dim_val; 
                break;
            case 3: // BLE 3: Dark Blue
                r = 0; g = 0; b = dim_val;
                break;
            case 4: // Dongle: Yellow
                r = dim_val; g = dim_val; b = 0;
                break;
        }
    }

    // Pairing Blink Logic
    if (al68_state.is_pairing && is_mode_active) {
        if ((timer_read32() / 250) % 2) {
            // Blink Off
             r = 0; g = 0; b = 0;
        }
    }

    // Caps Lock Override
    if (host_keyboard_led_state().caps_lock) {
        // Caps Lock Active: Red
        r = dim_val; g = 0; b = 0;
    }

    // Apply to dedicated LEDs (3 and 4)
    rgb_matrix_set_color(3, r, g, b);
    rgb_matrix_set_color(4, r, g, b);

    return false;
}
#endif
