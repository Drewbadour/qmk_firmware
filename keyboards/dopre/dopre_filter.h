// Copyright 2026 Vestigl Creations
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stdint.h>
#include <stdbool.h>

// Layer 1 of the HONKS debouncing: turns one noisy, slowly drifting oscillator period into a clean
// on/off state per key (adaptive baseline plus hysteresis). Layer 2, the press lockout, is in
// lockout_debounce.c. This file has no hardware dependencies.

typedef struct {
    uint32_t baseline_fp;        // Adaptive resting period, with 8 fractional bits. 0 = not seeded yet.
    uint32_t reanchor_min_fp;    // Lowest period seen so far in the post-release re-anchor window.
    uint32_t reanchor_counter;   // Samples left in the re-anchor window, 0 when idle.
    uint16_t last_period;        // Most recent raw reading, for the tuning readout.
    uint16_t peak_rise;          // Largest rise above the baseline since the host last read it (readout only).
    bool     is_on;              // The clean output.
} dopre_key_t;

// Feeds one new period reading (timer ticks) into a key and updates key->is_on.
void dopre_filter_update(dopre_key_t *key, uint32_t measured_period);
