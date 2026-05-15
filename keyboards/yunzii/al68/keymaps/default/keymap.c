#include QMK_KEYBOARD_H

#if defined(ENCODER_MAP_ENABLE)
const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][NUM_DIRECTIONS] = {
    [0] = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU) },
    [1] = { ENCODER_CCW_CW(LCTL(LGUI(KC_LEFT)), LCTL(LGUI(KC_RIGHT))) }
};
#endif

#define STATUS_LED_LEFT  3
#define STATUS_LED_RIGHT 4
#define STATUS_LED_HOLD_MS 2800

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    /* Layer 0: Default */
    [0] = LAYOUT_65_ansi_blocker(
    KC_ESC,     KC_1,       KC_2,       KC_3,       KC_4,       KC_5,       KC_6,       KC_7,       KC_8,       KC_9,       KC_0,       KC_MINS,    KC_EQL,     KC_BSPC,    LT(1, KC_MUTE),
    KC_TAB,     KC_Q,       KC_W,       KC_E,       KC_R,       KC_T,       KC_Y,       KC_U,       KC_I,       KC_O,       KC_P,       KC_LBRC,    KC_RBRC,    KC_BSLS,    KC_DELETE,
    KC_CAPS,    KC_A,       KC_S,       KC_D,       KC_F,       KC_G,       KC_H,       KC_J,       KC_K,       KC_L,       KC_SCLN,    KC_QUOT,                KC_ENT,     KC_PAGE_UP,
    KC_LSFT,    KC_Z,       KC_X,       KC_C,       KC_V,       KC_B,       KC_N,       KC_M,       KC_COMM,    KC_DOT,     KC_SLSH,    KC_RSFT,                KC_UP,      KC_PAGE_DOWN,
    KC_LCTL,    KC_LGUI,    KC_LALT,                            KC_SPC,                             MO(1),      KC_NO,                              KC_LEFT,    KC_DOWN,    KC_RIGHT
    ),
    /* Layer 1: FN */
    [1] = LAYOUT_65_ansi_blocker(
    KC_GRV,     KC_F1,      KC_F2,      KC_F3,      KC_F4,      KC_F5,      KC_F6,      KC_F7,      KC_F8,      KC_F9,      KC_F10,     KC_F11,     KC_F12,     _______,    _______,
    _______,    KC_BLE1,    KC_BLE2,    KC_BLE3,    KC_24G,     _______,    _______,    _______,    _______,    _______,    KC_PAIR,    _______,    _______,    RM_NEXT,    _______,
    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,                RM_HUEU,    KC_BAT,
    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,                RM_VALU,    _______,
    _______,    _______,    _______,                            RM_TOGG,                            _______,    _______,                            RM_SPDD,    RM_VALD,    RM_SPDU
    )
};

void keyboard_post_init_user(void) {
    rgb_matrix_enable_noeeprom();
}

#ifdef RGB_MATRIX_ENABLE
bool rgb_matrix_indicators_user(void) {
    static uint8_t  last_mode      = 0;
    static bool     last_connected = false;
    static bool     last_pairing   = false;
    static uint32_t status_timer   = 0;

    if (al68_state.current_mode != last_mode || al68_state.is_connected != last_connected || al68_state.is_pairing != last_pairing) {
        last_mode      = al68_state.current_mode;
        last_connected = al68_state.is_connected;
        last_pairing   = al68_state.is_pairing;
        status_timer   = timer_read32();
    }

    // Battery display mode (hold KC_BAT)
    if (al68_state.battery_showing) {
        rgb_matrix_set_color_all(0, 0, 0);
        uint8_t bars = al68_state.battery_percent / 10;
        if (bars > 10) bars = 10;
        for (uint8_t i = 0; i < bars; i++) {
            uint8_t r, g, b;
            if (al68_state.battery_percent <= 30) {
                r = 200; g = 0; b = 0;   // Red
            } else if (al68_state.battery_percent <= 70) {
                r = 200; g = 200; b = 0;  // Yellow
            } else {
                r = 0; g = 200; b = 0;    // Green
            }
            // Light up number row keys (indices 55-64 in the LED layout)
            // These map to the 0-9 keys in the matrix
            rgb_matrix_set_color(55 + i, r, g, b);
        }
        return false;
    }

    uint8_t r = 0, g = 0, b = 0;
    bool show_status = false;
    const uint8_t dim_val = 51;

    if (al68_state.current_mode != 0) {
        show_status = al68_state.is_pairing || !al68_state.is_connected || timer_elapsed32(status_timer) <= STATUS_LED_HOLD_MS;
        switch (al68_state.current_mode) {
            case 1: r = dim_val; g = dim_val; b = dim_val; break; // BLE1: White
            case 2: r = 0; g = dim_val; b = dim_val; break;       // BLE2: Cyan
            case 3: r = 0; g = 0; b = dim_val; break;             // BLE3: Blue
            case 4: r = dim_val; g = dim_val; b = 0; break;       // 2.4G: Yellow
        }
    }

    if (!show_status) {
        r = 0;
        g = 0;
        b = 0;
    }

    // Pairing blink
    if (al68_state.is_pairing && show_status) {
        if ((timer_read32() / 250) % 2) {
            r = 0; g = 0; b = 0;
        }
    }

    // Not connected blink (slower)
    if (show_status && !al68_state.is_connected && !al68_state.is_pairing) {
        if ((timer_read32() / 500) % 2) {
            r = 0; g = 0; b = 0;
        }
    }

    // Low battery warning: override with red blink
    if (al68_state.low_battery && al68_state.current_mode != 0) {
        if ((timer_read32() / 1000) % 2) {
            r = dim_val; g = 0; b = 0;
        } else {
            r = 0; g = 0; b = 0;
        }
    }

    // Caps Lock override
    if (host_keyboard_led_state().caps_lock) {
        r = dim_val; g = 0; b = 0;
    }

    // Apply to indicator LEDs (indices 3 and 4)
    rgb_matrix_set_color(STATUS_LED_LEFT, r, g, b);
    rgb_matrix_set_color(STATUS_LED_RIGHT, r, g, b);

    return false;
}
#endif
