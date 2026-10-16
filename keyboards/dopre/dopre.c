// Copyright 2026 Vestigl Creations
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dopre.h"
#include "dopre_scan.h"
#include "dopre_defaults.h"

#ifdef CONSOLE_ENABLE
#    include "print.h"
#endif

#ifdef RAW_ENABLE
#    include "raw_hid.h"
#endif

// ---- Start-up after a flash -----------------------------------------------------------------

// `dfu-util ... :leave`, which `qmk flash` uses, makes ST's ROM bootloader jump into the application
// instead of resetting the chip. The bootloader leaves its own vector table selected, so the application
// takes its first interrupts (SysTick, USB) in bootloader code and never comes up, until the keyboard is
// power cycled. ChibiOS does not set VTOR on STM32G0 (CRT0_VTOR_INIT is off there), and a jump does not
// reset it.
//
// Measured on HMonolith, on the first instruction of the application after such a jump:
//   VTOR = 0x1FFF0000 (the bootloader's table; it is 0 after any reset), the DMA1 channel 1 interrupt
//   enabled in the NVIC, the GPIO and RTC bus clocks running. Oscillators, SysTick and the USB clock
//   were back at their reset state.
//
// VTOR is 0 after every real reset, so a non-zero VTOR means we were jumped to. One system reset then
// gives the application the clean start it expects, and the host sees the USB pull-up drop and
// re-enumerates. After the reset VTOR is 0 again, so this cannot loop. QK_BOOT and cold boots are
// unaffected. Weak, so a board can replace it, but then it must keep this check.
__attribute__((weak)) void early_hardware_init_pre(void) {
    // QMK's own version of this hook does just this: honour a request to enter the DFU bootloader.
#if EARLY_INIT_PERFORM_BOOTLOADER_JUMP
    void enter_bootloader_mode_if_requested(void);
    enter_bootloader_mode_if_requested();
#endif

    if (SCB->VTOR != 0) {
        NVIC_SystemReset();
    }
}

// ---- Custom keycodes ------------------------------------------------------------------------

#ifdef CONSOLE_ENABLE
// One line per sense channel: "period/baseline" for each mux state, with a '*' on keys that are on.
static void dump_sensors(void) {
    uprintf("dopre: %u rows x %u cols, on>%u off>%u, settle timeouts %lu\n", MATRIX_ROWS, MATRIX_COLS, DOPRE_THRESHOLD_ON, DOPRE_THRESHOLD_OFF, (unsigned long)dopre_get_settle_timeouts());
    for (uint8_t row = 0; row < MATRIX_ROWS; ++row) {
        uprintf("r%u:", row);
        for (uint8_t col = 0; col < MATRIX_COLS; ++col) {
            uprintf(" %u/%u%c", dopre_get_period(row, col), dopre_get_baseline(row, col), dopre_get_is_on(row, col) ? '*' : ' ');
        }
        uprintf("\n");
    }
}
#endif

// The hex digits are outside the range QMK's register_code() accepts, so add them to the report
// directly.
static void send_hex_key(uint8_t usage, bool pressed) {
    if (pressed) {
        add_key(usage);
    } else {
        del_key(usage);
    }
    send_keyboard_report();
}

bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
    if (!process_record_user(keycode, record)) {
        return false;
    }

    switch (keycode) {
        case KP_HXA ... KP_HXF:
            send_hex_key((uint8_t)(0xBC + (keycode - KP_HXA)), record->event.pressed);
            return false;

#ifdef CONSOLE_ENABLE
        case DP_DUMP:
            if (record->event.pressed) {
                dump_sensors();
            }
            return false;
#endif
    }

    return true;
}

// ---- Raw HID tuning readout -----------------------------------------------------------------

#ifdef RAW_ENABLE
static void put_u16(uint8_t *out, uint16_t value) {
    out[0] = (uint8_t)(value & 0xFFU);
    out[1] = (uint8_t)(value >> 8);
}

static void put_u32(uint8_t *out, uint32_t value) {
    put_u16(out, (uint16_t)(value & 0xFFFFU));
    put_u16(out + 2, (uint16_t)(value >> 16));
}

// Weak, so a keymap (or VIA) that needs the raw HID interface for itself can replace it.
__attribute__((weak)) void raw_hid_receive(uint8_t *data, uint8_t length) {
    (void)length;

    uint8_t reply[32] = {0};
    uint8_t command   = data[0];

    reply[0] = command;
    reply[1] = 1; // Assume failure until a command is handled.

    switch (command) {
        case DOPRE_CMD_INFO:
            reply[1] = 0;
            reply[2] = DOPRE_PROTOCOL_VERSION;
            reply[3] = MATRIX_ROWS;
            reply[4] = MATRIX_COLS;
            put_u16(reply + 5, DOPRE_THRESHOLD_ON);
            put_u16(reply + 7, DOPRE_THRESHOLD_OFF);
            put_u16(reply + 9, DOPRE_REST_BAND);
            put_u32(reply + 11, dopre_get_settle_timeouts());
            put_u32(reply + 15, dopre_get_scan_count());
            break;

        case DOPRE_CMD_PERIODS:
        case DOPRE_CMD_BASELINES:
        case DOPRE_CMD_PEAKS: {
            uint8_t row = data[1];
            if (row >= MATRIX_ROWS) {
                break;
            }

            uint8_t on_mask = 0;
            for (uint8_t col = 0; col < MATRIX_COLS; ++col) {
                if (dopre_get_is_on(row, col)) {
                    on_mask |= (uint8_t)(1U << col);
                }
                uint16_t value;
                if (command == DOPRE_CMD_PERIODS) {
                    value = dopre_get_period(row, col);
                } else if (command == DOPRE_CMD_BASELINES) {
                    value = dopre_get_baseline(row, col);
                } else {
                    value = dopre_take_peak(row, col);
                }
                put_u16(reply + 4 + 2 * col, value);
            }

            reply[1] = 0;
            reply[2] = row;
            reply[3] = on_mask;
            break;
        }
    }

    raw_hid_send(reply, sizeof(reply));
}
#endif
