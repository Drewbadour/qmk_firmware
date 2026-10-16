// Copyright 2026 Vestigl Creations
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "matrix.h"
#include "dopre_defaults.h"

// Number of sense channels in DOPRE_SENSE_CHANNELS (equals MATRIX_ROWS).
#define DOPRE_X_COUNT(pin, af, tim, ch) +1
#define DOPRE_NUM_CHANNELS (0 DOPRE_SENSE_CHANNELS(DOPRE_X_COUNT))

// Starts the capture timers, DMA and mux. Call once, before the first scan.
void dopre_init(void);

// Measures every key once (a full pass over the mux states), runs the baseline tracker and writes
// the resulting on/off states into current_matrix. Returns true if any key changed state.
bool dopre_scan(matrix_row_t current_matrix[]);

// Tuning readout. period is the latest raw reading and baseline the tracked resting level, both
// in timer ticks; is_on is the filtered key state. Out-of-range keys read as 0 / false.
uint16_t dopre_get_period(uint8_t row, uint8_t col);
uint16_t dopre_get_baseline(uint8_t row, uint8_t col);
bool     dopre_get_is_on(uint8_t row, uint8_t col);

// Largest rise of the key's period above its baseline since this was last called for it, in timer
// ticks. Reading clears it. Unlike dopre_get_period() this cannot miss a short press, however rarely
// it is polled. Call it from the main loop (QMK's raw HID handler is), as the scan runs there too.
uint16_t dopre_take_peak(uint8_t row, uint8_t col);

// Number of complete scans (every key measured once) since power-up. Two readings a known time apart
// give the scan rate.
uint32_t dopre_get_scan_count(void);

// Count of times a channel failed to settle in time (stale reading reused). Non-zero usually means
// an oscillator has stopped.
uint32_t dopre_get_settle_timeouts(void);
