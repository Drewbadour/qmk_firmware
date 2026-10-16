# HMonolith

88-key ANSI-style board with a two-column cluster on the left, using the shared [dopre](../) capacitive scan system: an STM32G0B1KC with 11 sense channels of 8 keys each. How the scan works, how to tune it and the hardware notes are in [`../readme.md`](../readme.md).

* Keyboard Maintainer: Vestigl Creations
* Hardware Supported: STM32G0B1KC, 11 dopre sense channels

Make example for this keyboard (after setting up your build environment):

    make dopre/hmonolith:default

Flashing example for this keyboard:

    qmk flash -kb dopre/hmonolith -km default

## Status

| What | Where | Status |
|------|-------|--------|
| Layout and key wiring: 88 keys, each at `[Mux, Channel]` = `[sense channel, mux state]` | `keyboard.json` | **Real**, converted from `hmonolith.json` (a KLE file whose key labels are `Mux,Channel`) |
| Sense pins for Mux 0-10 | `dopre_config.h` | **Real**: PB0, PA3, PA2, PA1, PA0, PB9, PB8, PB7, PB4, PB6, PB5. The list originally given had PA2 twice; PA1 was used for Mux 3 and the board types correctly |
| Mux select lines | `dopre_config.h` | **Real**: PB1, PB2, PB3, with PB1 as bit 0 |
| Actuation thresholds | `dopre_config.h` | **Measured** on this board: ON 40, OFF 20. The basis is in the file; change them to move the actuation point |
| Keymap | `keymaps/default/keymap.c` | **Set** for the main block, the function row and the F13-F20 keys, with an HHKB-style Fn layer. The bottom-row modifiers and a few clusters are still `KC_NO`, see the comments in the file |
| USB product ID and name | `keyboard.json` | `0x0A9B`, HONKS' (the vendor ID and manufacturer come from `../info.json`) |
| Release lockout (`debounce`) | `keyboard.json` | 25 ms, HONKS' value |

The timer and alternate-function numbers were filled in from the STM32G0B1 pin tables (the same combinations Estacado uses, plus PB0 as TIM3 channel 3). Eleven of the twelve timer capture channels are in use; the free one is TIM3 channel 4.

## Notes

* Layer 0 follows the HHKB for the main block: Esc at `[0,3]`, Backspace at `[9,5]`, pipe/backslash at `[9,6]`, Delete at `[9,7]`, and Fn at `[10,5]`, the end of the shift row. `[0,2]` is the tilde key, `[1,2]` and `[0,1]` are F13 and F14, and the cluster `[1,0]`, `[0,0]`, `[1,1]`, `[1,6]`, `[1,3]`, `[1,5]` is F15-F20. Caps Lock stays at `[1,7]`, where the HHKB has Control.
* Layer 1 (Fn) is QMK's own HHKB Fn layer, position for position. Fn + `[1,2]` jumps to the DFU bootloader (`QK_BOOT`), and Fn + `[0,1]` is `DP_DUMP`, a console dump of every key's period and baseline.
* No Control, Alt or GUI keys are assigned yet: those positions (`[2,1]`, `[2,2]` and the five keys right of Space) are `KC_NO`.
* Measured on this board: a full scan takes about 0.73 ms (1,374 scans per second); a key at rest never rises more than 4 ticks above its baseline; presses peak at about 100 ticks, 71-79 on the weakest keys; and F4 (Mux 0, Channel 5) couples about 24 ticks when Mux 1, Channel 5 is pressed.
