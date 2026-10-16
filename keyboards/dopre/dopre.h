// Copyright 2026 Vestigl Creations
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "quantum.h"

enum dopre_keycodes {
    // Prints every key's period and baseline to the QMK console (needs CONSOLE_ENABLE).
    DP_DUMP = QK_KB_0,

    // Keypad hexadecimal digits A-F (HID keyboard-page usages 0xBC-0xC1). QMK has no stock keycode
    // for them, and HONKS keymaps use them on the keypad layer.
    KP_HXA,
    KP_HXB,
    KP_HXC,
    KP_HXD,
    KP_HXE,
    KP_HXF,
};

// ---- Raw HID tuning readout ---------------------------------------------------------------------
//
// With RAW_ENABLE, the keyboard answers 32-byte requests on the QMK raw HID interface (usage page
// 0xFF60, usage 0x61) with live sensor values, which tools/dopre_monitor.py displays. All multi-byte
// values are little endian. A request is [command, argument...]; every reply starts with
// [command, status] where status is 0 on success and 1 for a bad command or argument.
//
//   DOPRE_CMD_INFO       request: []
//                     reply:   [cmd, status, protocol_version, rows, cols,
//                               threshold_on (u16), threshold_off (u16), rest_band (u16),
//                               settle_timeouts (u32), scan_count (u32)]
//   DOPRE_CMD_PERIODS    request: [row]
//                     reply:   [cmd, status, row, on_mask, period[0..7] (u16 each)]
//   DOPRE_CMD_BASELINES  request: [row]
//                     reply:   [cmd, status, row, on_mask, baseline[0..7] (u16 each)]
//   DOPRE_CMD_PEAKS      request: [row]
//                     reply:   [cmd, status, row, on_mask, peak[0..7] (u16 each)]
//                     peak = largest rise of the period above the baseline since the previous
//                     DOPRE_CMD_PEAKS for that row; reading it clears it. Unlike a periodic read of
//                     DOPRE_CMD_PERIODS it cannot miss a short press, however slowly the host polls.
//
// scan_count is the number of complete scans since power-up; two readings a known time apart give
// the scan rate.
//
// "row" is the sense channel and "col" the mux state. Periods and baselines are in timer ticks, and
// on_mask has bit col set when that key is on. Columns at or beyond the matrix width read as 0.
#define DOPRE_PROTOCOL_VERSION 2

#define DOPRE_CMD_INFO 0x01
#define DOPRE_CMD_PERIODS 0x02
#define DOPRE_CMD_BASELINES 0x03
#define DOPRE_CMD_PEAKS 0x04
