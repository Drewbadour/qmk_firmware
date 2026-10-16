// Copyright 2026 Vestigl Creations
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// Settings of the shared "dopre" capacitive key sensing: the board's own dopre_config.h, plus defaults
// for everything the board does not set. Every file of the scan system includes this one.
//
// Every key is a relaxation oscillator whose period grows when the key is pressed. One oscillator
// feeds one timer input-capture pin and is shared by MATRIX_COLS keys through an analog mux.
//   - QMK matrix row    = sense channel (one entry of DOPRE_SENSE_CHANNELS)
//   - QMK matrix column = mux state     (0 .. MATRIX_COLS-1, MATRIX_COLS <= 8)
//
// A board provides dopre_config.h, next to its keyboard.json, with at least:
//
//   DOPRE_SENSE_CHANNELS(X)  one X(pin, alternate_function, timer, timer_channel) per sense channel.
//                            The Nth entry is matrix row N, and it also uses DMA/DMAMUX channel N:
//                            entries 0-6 land on DMA1 channels 1-7 and entries 7-11 on DMA2 channels
//                            1-5, so at most 12. The timer is 2, 3 or 4 (TIM2/TIM3/TIM4), the timer
//                            channel 1-4, and the alternate function must be the one that routes the
//                            pin to that timer channel.
//   DOPRE_MUX_PINS(Y)        one Y(pin) per mux select line, least significant bit first. Mux state N
//                            drives bit k of N onto line k.
//   DOPRE_THRESHOLD_ON/OFF   the actuation thresholds (below); they depend on the board's sensor pads.
//
// No pin may be used twice, including between the two lists. The build checks that.
//
// Any other DOPRE_* setting below can be overridden by defining it in the board's dopre_config.h.

#include "dopre_config.h"

#ifndef DOPRE_SENSE_CHANNELS
#    error "This board's dopre_config.h must define DOPRE_SENSE_CHANNELS (see dopre_defaults.h)"
#endif
#ifndef DOPRE_MUX_PINS
#    error "This board's dopre_config.h must define DOPRE_MUX_PINS (see dopre_defaults.h)"
#endif
#ifndef DOPRE_THRESHOLD_ON
#    error "This board's dopre_config.h must define DOPRE_THRESHOLD_ON (see dopre_defaults.h)"
#endif
#ifndef DOPRE_THRESHOLD_OFF
#    error "This board's dopre_config.h must define DOPRE_THRESHOLD_OFF (see dopre_defaults.h)"
#endif

// ---------------------------------------------------------------------------------------------
// ACTUATION
//
// DOPRE_THRESHOLD_ON: how far above its baseline a released key's period must rise to count as
// pressed. DOPRE_THRESHOLD_OFF: how close to its baseline a pressed key's period must fall to count as
// released; it must be lower than ON, and the gap is the hysteresis. Both are in timer ticks of one
// oscillator period (the timers run at SYSCLK, 64 MHz). Measure them with tools/dopre_monitor.py.
// ---------------------------------------------------------------------------------------------

// ---------------------------------------------------------------------------------------------
// BASELINE TRACKING
// ---------------------------------------------------------------------------------------------

// Largest excursion above the baseline still treated as drift. Anything wider is a press and
// freezes the baseline. A real press must exceed DOPRE_THRESHOLD_ON + DOPRE_REST_BAND.
#ifndef DOPRE_REST_BAND
#    define DOPRE_REST_BAND 10
#endif

// Samples watched after a release to recover the resting level. Must exceed the settle time of a
// released key (normally 1-2 samples).
#ifndef DOPRE_REANCHOR_SAMPLES
#    define DOPRE_REANCHOR_SAMPLES 128
#endif

// Baseline IIR speed: the baseline moves by (difference >> shift) per sample. Higher is slower.
#ifndef DOPRE_DECAY_DOWN_SHIFT
#    define DOPRE_DECAY_DOWN_SHIFT 11
#endif
#ifndef DOPRE_DECAY_UP_SHIFT
#    define DOPRE_DECAY_UP_SHIFT 11
#endif

// ---------------------------------------------------------------------------------------------
// SCANNING
// ---------------------------------------------------------------------------------------------

// Captures to let through after switching the mux before a reading is trusted (Estacado uses 3).
#ifndef DOPRE_MUX_SETTLE_CAPTURES
#    define DOPRE_MUX_SETTLE_CAPTURES 3
#endif

// Give up waiting for a channel to settle after this many timer ticks (64 MHz, so 32000 is 0.5 ms).
// This only matters if an oscillator has stopped; the stale reading is then reused. Must be < 65536.
#ifndef DOPRE_SETTLE_TIMEOUT_TICKS
#    define DOPRE_SETTLE_TIMEOUT_TICKS 32000
#endif
