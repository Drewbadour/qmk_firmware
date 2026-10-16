// Copyright 2026 Vestigl Creations
// SPDX-License-Identifier: GPL-2.0-or-later

// Keys are listed in reading order, row by row, as in the layout in keyboard.json. Coordinates in the
// comments are [Mux, Channel], the same pair used for the matrix.
//
// Layer 0
//   - The main block follows the HHKB: Esc at [0,3], the tilde key at [0,2] above it, Backspace at [9,5],
//     the pipe/backslash at [9,6] and Delete at [9,7] in the HHKB's \ and ` positions.
//   - Fn is [10,5], where the HHKB has it, at the end of the shift row.
//   - [1,2] and [0,1] are F13 and F14, and the cluster [1,0], [0,0], [1,1], [1,6], [1,3], [1,5] is F15-F20.
//   - KC_NO keys have no assigned legend yet: [2,5], [2,7], [2,6], [2,4], the bottom-row modifiers [2,1] and
//     [2,2], and [10,4], [10,2], [10,1], [10,0], [10,3].
//
// Layer 1 (Fn)
//   - The main block is QMK's own HHKB Fn layer (keyboards/hhkb/ansi), position for position: F1-F12 on the
//     number row, Power on Esc, Insert and Delete at the HHKB's \ and `, Caps on Tab, Print Screen, Scroll
//     Lock, Pause and Up on I O P [, volume on A S D, keypad * / + - on H J N M, Home PgUp End PgDn, the
//     arrows on ; ' / , and keypad Enter on Enter.
//   - KC_PWR is the system Power key, as on the HHKB; on macOS it can open the shutdown dialog.
//   - [1,2] is QK_BOOT: Fn + [1,2] jumps to the STM32 DFU bootloader. [0,1] is DP_DUMP (console dump of
//     every key's period and baseline).

#include QMK_KEYBOARD_H

// clang-format off
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = LAYOUT(
        KC_F13 , KC_F14 , KC_GRV , KC_F1  , KC_F2  , KC_F3  , KC_F4  , KC_F5  , KC_F6  , KC_F7  , KC_F8  , KC_F9  , KC_F10 , KC_F11 , KC_F12 ,
        KC_F15 , KC_F16 , KC_ESC , KC_1   , KC_2   , KC_3   , KC_4   , KC_5   , KC_6   , KC_7   , KC_8   , KC_9   , KC_0   , KC_MINS, KC_EQL , KC_BSLS, KC_DEL ,
        KC_F17 , KC_F18 , KC_TAB , KC_Q   , KC_W   , KC_E   , KC_R   , KC_T   , KC_Y   , KC_U   , KC_I   , KC_O   , KC_P   , KC_LBRC, KC_RBRC, KC_BSPC,
        KC_F19 , KC_F20 , KC_CAPS, KC_A   , KC_S   , KC_D   , KC_F   , KC_G   , KC_H   , KC_J   , KC_K   , KC_L   , KC_SCLN, KC_QUOT, KC_ENT ,
        KC_LSFT, KC_Z   , KC_X   , KC_C   , KC_V   , KC_B   , KC_N   , KC_M   , KC_COMM, KC_DOT , KC_SLSH, KC_RSFT, MO(1)  ,
        KC_NO  ,
        KC_NO  , KC_NO  , KC_SPC , KC_NO  , KC_NO  , KC_NO  , KC_NO  , KC_NO  ,
        KC_NO  , KC_NO  , KC_NO  
    ),
    [1] = LAYOUT(
        QK_BOOT, DP_DUMP, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_PWR , KC_F1  , KC_F2  , KC_F3  , KC_F4  , KC_F5  , KC_F6  , KC_F7  , KC_F8  , KC_F9  , KC_F10 , KC_F11 , KC_F12 , KC_INS , KC_DEL ,
        KC_TRNS, KC_TRNS, KC_CAPS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_PSCR, KC_SCRL, KC_PAUS, KC_UP  , KC_TRNS, KC_BSPC,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_VOLD, KC_VOLU, KC_MUTE, KC_TRNS, KC_TRNS, KC_PAST, KC_PSLS, KC_HOME, KC_PGUP, KC_LEFT, KC_RGHT, KC_PENT,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_PPLS, KC_PMNS, KC_END , KC_PGDN, KC_DOWN, KC_TRNS, KC_TRNS,
        KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        KC_TRNS, KC_TRNS, KC_TRNS
    )
};
