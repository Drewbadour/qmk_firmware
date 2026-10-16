// Copyright 2026 Vestigl Creations
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// HMonolith's pin map and actuation thresholds for the shared dopre scan system (keyboards/dopre/).
// What each setting means, and the other settings that can be overridden here, are in
// keyboards/dopre/dopre_defaults.h.

// ---------------------------------------------------------------------------------------------
// PIN MAP
// ---------------------------------------------------------------------------------------------

// X(pin, alternate_function, timer, timer_channel). Row N is "Mux N" in hmonolith.json:
//   Mux  0   1   2   3   4   5   6   7   8   9  10
//       PB0 PA3 PA2 PA1 PA0 PB9 PB8 PB7 PB4 PB6 PB5
// The pin list originally given had PA2 twice. PA1, the only TIM2 pin missing, was used for Mux 3, and
// the board types correctly with it. Every entry other than PB0 is the same pin/AF/timer combination
// Estacado uses. PB0 is TIM3_CH3 on AF1, which is also how QMK's WeAct G0B1 board drives its LED strip
// from PB0. Eleven of the twelve timer capture channels are in use; TIM3 channel 4 is the free one.
#define DOPRE_SENSE_CHANNELS(X) \
    X(B0, 1, 3, 3)              \
    X(A3, 2, 2, 4)              \
    X(A2, 2, 2, 3)              \
    X(A1, 2, 2, 2)              \
    X(A0, 2, 2, 1)              \
    X(B9, 9, 4, 4)              \
    X(B8, 9, 4, 3)              \
    X(B7, 9, 4, 2)              \
    X(B4, 1, 3, 1)              \
    X(B6, 9, 4, 1)              \
    X(B5, 1, 3, 2)

// Y(pin): mux select lines, least significant bit first. HMonolith: PB1, PB2, PB3 in that order, so
// PB1 is bit 0.
#define DOPRE_MUX_PINS(Y) \
    Y(B1)                 \
    Y(B2)                 \
    Y(B3)

// ---------------------------------------------------------------------------------------------
// ACTUATION TUNING
//
// Measured on the HMonolith board with tools/dopre_monitor.py (firmware peak-hold, one press of every
// key in a firm run and a light run, plus 127 s at rest):
//   - at rest a key never rises more than 4 ticks above its baseline;
//   - the one coupling found: F4 (Mux 0, Ch 5) rises by about 24 whenever Mux 1, Ch 5 is pressed;
//   - the lightest tap measured peaked at 53, and the weakest keys reach only 71-79 when pressed;
//   - the typical press peaks at about 100, and a light press is only about 8% weaker than a firm one.
// So ON has to sit above 24 and well below 53; Estacado's 80 was above the light-press signal of six
// keys. This is an actuation point you can move: lower is shallower and more sensitive to noise and
// coupling, higher is deeper.
// ---------------------------------------------------------------------------------------------

#define DOPRE_THRESHOLD_ON 40
#define DOPRE_THRESHOLD_OFF 20
