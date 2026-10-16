// Copyright 2026 Vestigl Creations
// SPDX-License-Identifier: GPL-2.0-or-later

#include "debounce.h"
#include "timer.h"

// Layer 2 of the HONKS debouncing (KeyboardDebounceMatrix): a press lockout.
//
// Presses are reported at once. A release is held back until the key has been down for at least
// DEBOUNCE milliseconds, measured from the press and never restarted by chatter. A contact that
// bounces (make, break, make) therefore produces exactly one press and one release.
//
// This is not one of QMK's stock debounce types. The nearest, asym_eager_defer_pk, also reports
// presses at once but delays every release by DEBOUNCE ms after the release edge. Here a key held
// for at least DEBOUNCE ms is released with no delay at all. Select it with DEBOUNCE_TYPE = custom.

#ifndef DEBOUNCE
#    define DEBOUNCE 25
#endif

static uint32_t press_time[MATRIX_ROWS][MATRIX_COLS];

void debounce_init(void) {}

// Must run on every scan, not only when the raw matrix changed: a deferred release becomes due
// purely because time passes.
bool debounce(matrix_row_t raw[], matrix_row_t cooked[], bool changed) {
    (void)changed;

    bool     cooked_changed = false;
    uint32_t now            = timer_read32();

    for (uint8_t row = 0; row < MATRIX_ROWS; ++row) {
        matrix_row_t pending = raw[row] ^ cooked[row];
        if (pending == 0) {
            continue;
        }

        for (uint8_t col = 0; col < MATRIX_COLS; ++col) {
            matrix_row_t mask = (matrix_row_t)(1U << col);
            if (!(pending & mask)) {
                continue;
            }

            if (raw[row] & mask) {
                cooked[row] |= mask;
                press_time[row][col] = now;
                cooked_changed       = true;
            } else if ((uint32_t)(now - press_time[row][col]) >= DEBOUNCE) {
                cooked[row] &= (matrix_row_t)~mask;
                cooked_changed = true;
            }
        }
    }

    return cooked_changed;
}
