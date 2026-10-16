// Copyright 2026 Vestigl Creations
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dopre_filter.h"
#include "dopre_defaults.h"

// Port of HONKS' DopreCalculateKeyState. A press can only push the period up, so:
//   - a reading below the baseline follows the baseline down (the resting level fell);
//   - a small rise creeps the baseline up (drift);
//   - a large rise is a press and freezes the baseline, so a held key never drags its own
//     trip point up to its pressed level.
void dopre_filter_update(dopre_key_t *key, uint32_t measured_period) {
    uint32_t period_fp = measured_period << 8;
    bool     was_on    = key->is_on;

    key->last_period = (uint16_t)measured_period;

    if (key->baseline_fp == 0) {
        // Not seeded, so take the first reading as the resting level.
        key->baseline_fp = period_fp;
    } else if (period_fp < key->baseline_fp) {
        key->baseline_fp -= (key->baseline_fp - period_fp) >> DOPRE_DECAY_DOWN_SHIFT;
    } else {
        uint32_t gap = period_fp - key->baseline_fp;
        if ((gap >> 8) < DOPRE_REST_BAND) {
            uint32_t step = gap >> DOPRE_DECAY_UP_SHIFT;
            key->baseline_fp += (step > 0) ? step : 1;
        }
    }

    uint32_t baseline = key->baseline_fp >> 8;

    // Tuning readout only, with no effect on the decision below: remember the largest rise above the
    // baseline. A host polling a few times a second would otherwise miss the peak of a short tap.
    if (measured_period > baseline) {
        uint32_t rise = measured_period - baseline;
        if (rise > key->peak_rise) {
            key->peak_rise = (uint16_t)((rise > UINT16_MAX) ? UINT16_MAX : rise);
        }
    }

    // Hysteresis: a different trip point depending on the current state.
    if (key->is_on) {
        key->is_on = (measured_period > baseline + DOPRE_THRESHOLD_OFF);
    } else {
        key->is_on = (measured_period > baseline + DOPRE_THRESHOLD_ON);
    }

    // The baseline is frozen while a key is held, so any drift during the hold goes unnoticed.
    // A release is the one moment the key is known to be at rest, so recover the drift then:
    // watch the next samples and snap the baseline up to the lowest (settled) level.
    if (was_on && !key->is_on) {
        key->reanchor_counter   = DOPRE_REANCHOR_SAMPLES;
        key->reanchor_min_fp    = period_fp;
    } else if (key->reanchor_counter > 0) {
        if (key->is_on) {
            // Pressed again before settling, so abandon the window.
            key->reanchor_counter = 0;
        } else {
            if (period_fp < key->reanchor_min_fp) {
                key->reanchor_min_fp = period_fp;
            }

            --key->reanchor_counter;
            if ((key->reanchor_counter == 0) && (key->reanchor_min_fp > key->baseline_fp)) {
                key->baseline_fp = key->reanchor_min_fp;
            }
        }
    }
}
