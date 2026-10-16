# dopre

Shared QMK support for keyboards that sense keys capacitively ("dopre"): every key is a relaxation oscillator whose period grows when the key is pressed. It is a port of the HONKS firmware (Estacado, Teacup) to QMK, for boards built around an STM32G0B1 with timer input capture, DMA and a 3-bit analog mux per sense channel.

Every board under this folder shares the scan system. A board only supplies what differs: its sense and mux pins, its actuation thresholds, its layout and its keymap.

Boards:

* [`hmonolith`](hmonolith/)

Make example (after setting up your build environment):

    make dopre/hmonolith:default

Flashing example:

    qmk flash -kb dopre/hmonolith -km default

## Adding a board

Create `keyboards/dopre/<board>/` with:

1. `keyboard.json`: `keyboard_name`, `usb.pid` (a product ID of its own), `usb.device_version`, `debounce` (the release lockout in ms, 25 on HMonolith), `matrix_size` (`rows` = number of sense channels, `cols` = mux states, at most 8) and the layout. Give every key `"matrix": [row, col]` = `[sense channel, mux state]`. In HONKS' `keyboard_spec.h` the body of the `KEYMAP` macro lists, for each sense channel, the key at each mux state, which is exactly that pair.
2. `dopre_config.h`: `DOPRE_SENSE_CHANNELS`, `DOPRE_MUX_PINS`, `DOPRE_THRESHOLD_ON` and `DOPRE_THRESHOLD_OFF`. The format, and the other settings a board can override, are documented in [`dopre_defaults.h`](dopre_defaults.h).
3. `readme.md` and `keymaps/default/keymap.c`.

The build stops with a message if the rows of `keyboard.json` and the sense channels disagree, if a limit is exceeded (at most 12 sense channels, because TIM2, TIM3 and TIM4 have four capture channels each), if a required setting is missing, or if a pin is used twice, including a sense pin that is also a mux line.

Everything shared lives in this folder and is picked up through QMK's per-folder rules: `info.json` (manufacturer, MCU, bootloader, features, debounce, USB vendor ID), `rules.mk` (sources), `chconf.h` and `dopre.h`/`dopre.c` (the vendor-wide header and source). QMK merges the files with the most general one applied last, so a value in this folder's `info.json` applies to every board and a board cannot override it. That file therefore holds only what all boards share (manufacturer, MCU, bootloader, features, USB vendor ID, EEPROM driver and debounce type); a board's `keyboard.json` holds what can differ, including `debounce`, `usb.pid` and `usb.device_version`. Only STM32G0 boards with this timer and DMA arrangement are supported, because the scan driver programs those peripherals directly.

## How it works

* **Sensing** (`dopre_scan.c`): each sense pin feeds a timer input-capture channel. The timer free-runs at 64 MHz, a circular DMA channel moves every capture into a 16-entry ring buffer, and the period is the difference of the two newest captures. The mux then moves to the next state, and a reading is only trusted once three captures have passed since the switch. One `dopre_scan()` measures every mux state, which takes about three oscillator periods per state. QMK's matrix row is the sense channel and its column is the mux state.
* **Debounce layer 1** (`dopre_filter.c`): each key has an adaptive baseline that follows drift but freezes while the key is pressed, a two-threshold (hysteresis) on/off decision, and a re-anchor step after release. Ported from HONKS' `DopreCalculateKeyState`.
* **Debounce layer 2** (`lockout_debounce.c`): presses are reported at once, and a release is held back until the key has been down for `DEBOUNCE` ms, measured from the press. Ported from HONKS' `KeyboardDebounceMatrix`. `DEBOUNCE` is 25 ms, the value HONKS' firmware uses (`LOCKOUT_THRESHOLD` in its `info.h`; its debounce write-up says 20).
* **USB**: NKRO is on by default (HONKS only sent NKRO reports) and QMK falls back to a 6-key boot report when the host asks for it. Media keys use QMK's extrakey.

## Tuning

The thresholds are in raw timer ticks of one oscillator period and depend on the hardware, so measure them for each board. The firmware keeps each key's largest rise above its baseline itself, so a short tap is never missed by a slow reader.

* **Raw HID monitor** (live, all keys). With the board plugged in:

      "$(brew --prefix qmk)/libexec/bin/python" keyboards/dopre/tools/dopre_monitor.py

  That is the Python bundled with the Homebrew `qmk` package, which already has the `hid` module. Homebrew's own `python3` refuses `pip install` ("externally managed"), so elsewhere use a virtual environment: `python3 -m venv .venv && .venv/bin/pip install hidapi`.

  It shows `period - baseline` for every key (reverse video when the key is on), the peak rise of each key since it started, and the scan rate of the firmware. `--csv log.csv` records every reading, `--duration 60` stops by itself after a minute, and `--vid`/`--pid` select another board (the defaults are HMonolith's). On macOS the terminal needs Input Monitoring permission.
* **Console dump** (one snapshot). Bind `DP_DUMP` in a keymap and read it with `qmk console`.

A good `DOPRE_THRESHOLD_ON` sits well above the resting noise and above any coupling between keys, and well below the weakest press. `DOPRE_THRESHOLD_OFF` is lower; the gap between them is the hysteresis. To measure: record the board at rest, then one firm and one light press of every key into separate CSV files, and compare the peaks with the noise.

## Differences from HONKS

* Gone: the vendor HID protocol (stats, heatmap, sense commands), the USB updater and its host tools (`estacado_update.py`, `torture.py`), key-pair chatter tracking, and the dual-bank A/B update. Firmware updates use the STM32 system DFU bootloader instead. Put `QK_BOOT` in a keymap to enter it from software.
* New: the Raw HID readout above, `DP_DUMP`, and keycodes `KP_HXA`-`KP_HXF` for the keypad hex digits (HID usages 0xBC-0xC1), which QMK does not provide.
* Settings that QMK keeps in EEPROM (for example the NKRO toggle and the default layer) are not saved across power cycles. See below.

## Hardware notes

* **System tick.** QMK's ChibiOS configuration times its tick off TIM2 on STM32G0, but the sensing needs TIM2, TIM3 and TIM4. `chconf.h` switches the tick to SysTick (1 kHz), which leaves every general-purpose timer free. The ChibiOS GPT, PWM, ICU, SPI, I2C, UART and ADC drivers must stay disabled (they are by default), because they would claim the same timers and DMA channels.
* **EEPROM.** `info.json` selects the `transient` EEPROM driver. QMK's default stores settings in flash through ChibiOS' embedded-flash driver, and for STM32G0 that driver only has flash descriptors for 64 KB and 128 KB parts, so on a 256 KB STM32G0B1KC it finds none at startup (from reading the ChibiOS source; not tried on hardware). The image uses well under 128 KB, so nothing else is affected. Using real EEPROM emulation would need that driver taught about 256 KB devices.
* **Option bytes.** QMK's STM32G0B1 board support (`platforms/chibios/boards/GENERIC_STM32_G0B1XB/board/extra.c`) rewrites the option bytes on first boot, if needed, so the BOOT0 pin and NRST pin work like other STM32 parts (`nBOOT_SEL = 0`, `NRST_MODE = 0b11`), then resets. HONKS never changed those bits, and the change survives reflashing.
* **Flashing.** `qmk flash` ends with `dfu-util ... :leave`, which makes ST's ROM bootloader jump into the firmware instead of resetting the chip. The bootloader leaves its own vector table selected (`VTOR = 0x1FFF0000`, where it is 0 after any reset), and ChibiOS does not set `VTOR` on STM32G0, so without a fix the firmware's first interrupts run bootloader code and the keyboard stays dead until it is unplugged. `early_hardware_init_pre()` in `dopre.c` does one system reset when `VTOR` is not 0, and the keyboard re-enumerates about a second after the flash. The reset cannot repeat, because it makes `VTOR` 0; cold boots and `QK_BOOT` are unaffected.
* **Flash size.** The linker script is the 128 KB one. The STM32G0B1KC has 256 KB, and an image is about 30 KB.
* **Toolchain.** The build needs an ARM compiler that ships a C library. If it fails with `stdint.h: No such file or directory`, the compiler on the PATH is a bare one (for example Homebrew's `arm-none-eabi-gcc`); use the Arm GNU Toolchain, or the QMK tap's `arm-none-eabi-gcc@8` together with its `arm-none-eabi-binutils`.
