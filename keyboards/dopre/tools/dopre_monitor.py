#!/usr/bin/env python3
# Copyright 2026 Vestigl Creations
# SPDX-License-Identifier: GPL-2.0-or-later
"""Live view of the key sensors of a dopre board, read over the QMK raw HID interface.

Shows, for every key, how far its oscillator period is above its tracked baseline (in timer ticks).
Press and release keys to see how large the signal is, and compare it with DOPRE_THRESHOLD_ON and
DOPRE_THRESHOLD_OFF in dopre_config.h. Rows are sense channels, columns are mux states, matching the
matrix coordinates in keyboard.json.

The firmware keeps each key's peak itself, so a press is never missed because this tool polls too
slowly; polling speed only limits how finely the live table and the CSV follow a press. The header also
shows the scan rate of the firmware, measured from its scan counter.

    # Needs the 'hid' module. QMK's bundled Python has it; with Homebrew, run this as
    #   "$(brew --prefix qmk)/libexec/bin/python" dopre_monitor.py
    # or install hidapi into a virtual environment ('pip install' into Homebrew's python3 is refused).
    ./dopre_monitor.py              # live table, Ctrl-C to stop and print a summary
    ./dopre_monitor.py --once       # one snapshot (handy for scripting)
    ./dopre_monitor.py --duration 60 --csv press.csv   # record for 60 s, then print the summary

The wire format is documented in dopre.h. The defaults for --vid and --pid are HMonolith's;
use the values of your board otherwise.
"""

import argparse
import csv
import struct
import sys
import time

try:
    import hid
except ImportError:
    sys.exit("This tool needs the 'hid' module (pip package 'hidapi'). With Homebrew, run it with \"$(brew --prefix qmk)/libexec/bin/python\", which has it; Homebrew's python3 refuses pip installs.")

VENDOR_ID = 0x16D0
PRODUCT_ID = 0x0A9B
RAW_USAGE_PAGE = 0xFF60
RAW_USAGE = 0x61

DOPRE_PROTOCOL_VERSION = 2
DOPRE_CMD_INFO = 0x01
DOPRE_CMD_PERIODS = 0x02
DOPRE_CMD_BASELINES = 0x03
DOPRE_CMD_PEAKS = 0x04

REPORT_SIZE = 32
TIMEOUT_MS = 500


class MonitorError(Exception):
    pass


# Two unrelated PyPI packages are both imported as "hid". QMK bundles "hid" (class hid.Device); the
# "hidapi" package has hid.device(). Both are supported.
HID_ERRORS = (MonitorError, OSError, getattr(hid, "HIDException", OSError))


def open_device(path):
    if hasattr(hid, "Device"):
        return hid.Device(path=path)
    device = hid.device()
    device.open_path(path)
    return device


def find_path(vid, pid):
    for info in hid.enumerate(vid, pid):
        if info["usage_page"] == RAW_USAGE_PAGE and info["usage"] == RAW_USAGE:
            return info["path"]
    raise MonitorError(
        f"No raw HID interface found for {vid:04X}:{pid:04X}. Is the keyboard plugged in, and is the "
        "firmware built with RAW_ENABLE? (macOS also needs Input Monitoring permission for the terminal.)"
    )


class Keyboard:
    def __init__(self, device):
        self.device = device

    def request(self, command, *args):
        # hidapi wants a leading report ID byte; QMK's raw HID interface has none, so it is 0.
        packet = bytes([0, command, *args]).ljust(REPORT_SIZE + 1, b"\0")
        if self.device.write(packet) < 0:  # (hid.Device raises instead of returning -1)
            raise MonitorError("write to the keyboard failed")
        reply = bytes(self.device.read(REPORT_SIZE, TIMEOUT_MS))
        if len(reply) < REPORT_SIZE:
            raise MonitorError("no reply from the keyboard (timed out)")
        if reply[0] != command or reply[1] != 0:
            raise MonitorError(f"keyboard rejected command {command:#04x}")
        return reply

    def info(self):
        reply = self.request(DOPRE_CMD_INFO)
        version, rows, cols, on, off, rest = struct.unpack_from("<BBBHHH", reply, 2)
        timeouts, scans = struct.unpack_from("<II", reply, 11)
        if version != DOPRE_PROTOCOL_VERSION:
            raise MonitorError(
                f"firmware speaks protocol {version}, this tool speaks {DOPRE_PROTOCOL_VERSION}. "
                "Rebuild and flash the current firmware, or use a matching copy of this tool."
            )
        return {
            "rows": rows, "cols": cols, "threshold_on": on, "threshold_off": off, "rest_band": rest,
            "settle_timeouts": timeouts, "scan_count": scans,
        }

    def snapshot(self, rows, cols):
        """Returns (periods, baselines, is_on, peaks), each indexed [row][col].

        peaks is the largest rise above the baseline since the previous snapshot, kept by the firmware.
        """
        periods, baselines, is_on, peaks = [], [], [], []
        for row in range(rows):
            p = self.request(DOPRE_CMD_PERIODS, row)
            b = self.request(DOPRE_CMD_BASELINES, row)
            k = self.request(DOPRE_CMD_PEAKS, row)
            on_mask = p[3]
            periods.append(list(struct.unpack_from("<8H", p, 4))[:cols])
            baselines.append(list(struct.unpack_from("<8H", b, 4))[:cols])
            peaks.append(list(struct.unpack_from("<8H", k, 4))[:cols])
            is_on.append([bool(on_mask >> col & 1) for col in range(cols)])
        return periods, baselines, is_on, peaks


class ScanRate:
    """Scans per second of the firmware, from its scan counter, averaged over windows of about a second."""

    def __init__(self, count, now):
        self.start_count, self.start_time = count, now
        self.ref_count, self.ref_time = count, now
        self.last_count, self.last_time = count, now
        self.rate = None

    def update(self, count, now):
        if now - self.ref_time >= 1.0:
            self.rate = ((count - self.ref_count) & 0xFFFFFFFF) / (now - self.ref_time)
            self.ref_count, self.ref_time = count, now
        self.last_count, self.last_time = count, now

    def average(self):
        elapsed = self.last_time - self.start_time
        if elapsed <= 0:
            return None
        return ((self.last_count - self.start_count) & 0xFFFFFFFF) / elapsed


def render(info, snap, peaks, scan_rate):
    periods, baselines, is_on, _ = snap
    rate = f"{scan_rate:,.0f}/s" if scan_rate is not None else "measuring"
    lines = [
        f"dopre sensors  threshold ON > {info['threshold_on']}  OFF > {info['threshold_off']}  "
        f"rest band {info['rest_band']}  settle timeouts {info['settle_timeouts']}  firmware scan rate {rate}",
        "delta = period - baseline (ticks); [reverse video] = key is on; Ctrl-C to stop",
        "",
        "      " + "".join(f"  mux{c:<3}" for c in range(info["cols"])),
    ]
    for row in range(info["rows"]):
        cells = []
        for col in range(info["cols"]):
            delta = periods[row][col] - baselines[row][col]
            text = f"{delta:+7d}"
            cells.append(f"\x1b[7m{text}\x1b[0m" if is_on[row][col] else text)
        lines.append(f"ch{row:<3} " + "".join(cells))
    lines += ["", "peak rise since start (kept by the firmware, so short presses are included):"]
    for row in range(info["rows"]):
        lines.append(f"ch{row:<3} " + "".join(f"{peaks[row][col]:+7d}" for col in range(info["cols"])))
    return "\n".join(lines)


def summarize(info, peaks, reached, scan_rate):
    keys = [(r, c) for r in range(info["rows"]) for c in range(info["cols"])]
    hit = [peaks[r][c] for r, c in keys if reached[r][c]]
    miss = [peaks[r][c] for r, c in keys if not reached[r][c]]
    print()
    print(f"Keys whose rise reached THRESHOLD_ON ({info['threshold_on']}): {len(hit)} of {len(keys)}")
    if hit:
        print(f"  weakest peak among them: {min(hit):+d}   strongest: {max(hit):+d}")
    if miss:
        print(f"  strongest peak among the others: {max(miss):+d}   (these never reached it)")
        weak = sorted(((peaks[r][c], r, c) for r, c in keys if not reached[r][c]), reverse=True)[:5]
        print("  closest to it: " + ", ".join(f"[{r},{c}] {v:+d}" for v, r, c in weak))
    average = scan_rate.average() if scan_rate else None
    if average is not None:
        print(f"Firmware scan rate over the run: {average:,.0f} scans/s")
    print("A good THRESHOLD_ON sits well above the resting noise and well below the weakest press; THRESHOLD_OFF is lower.")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--vid", type=lambda s: int(s, 0), default=VENDOR_ID)
    parser.add_argument("--pid", type=lambda s: int(s, 0), default=PRODUCT_ID)
    parser.add_argument("--interval", type=float, default=0.0, help="extra seconds to wait between refreshes (default 0: as fast as the USB link allows)")
    parser.add_argument("--csv", help="also append every reading to this CSV file")
    parser.add_argument("--once", action="store_true", help="print one snapshot and exit")
    parser.add_argument("--duration", type=float, help="stop after this many seconds and print the summary")
    args = parser.parse_args()

    try:
        device = open_device(find_path(args.vid, args.pid))
        keyboard = Keyboard(device)
        info = keyboard.info()
    except HID_ERRORS as error:
        sys.exit(str(error))

    rows, cols = info["rows"], info["cols"]
    peaks = [[0] * cols for _ in range(rows)]
    reached = [[False] * cols for _ in range(rows)]
    scan_rate = ScanRate(info["scan_count"], time.time())
    csv_file = open(args.csv, "a", newline="") if args.csv else None
    writer = csv.writer(csv_file) if csv_file else None
    if writer and csv_file.tell() == 0:
        writer.writerow(["time", "row", "col", "period", "baseline", "is_on", "peak"])

    try:
        if not args.once:
            keyboard.snapshot(rows, cols)  # Reading clears the firmware's peaks, so "since start" means since now.
        started = time.time()
        while True:
            snap = keyboard.snapshot(rows, cols)
            periods, baselines, is_on, key_peaks = snap
            info = keyboard.info()
            now = time.time()
            scan_rate.update(info["scan_count"], now)
            for row in range(rows):
                for col in range(cols):
                    peaks[row][col] = max(peaks[row][col], key_peaks[row][col], periods[row][col] - baselines[row][col])
                    reached[row][col] |= is_on[row][col] or key_peaks[row][col] > info["threshold_on"]
                    if writer:
                        writer.writerow([f"{now:.3f}", row, col, periods[row][col], baselines[row][col], int(is_on[row][col]), key_peaks[row][col]])

            frame = render(info, snap, peaks, scan_rate.rate)
            if args.once:
                print(frame)
                break
            sys.stdout.write("\x1b[H\x1b[J" + frame + "\n")
            sys.stdout.flush()
            if args.duration is not None and time.time() - started >= args.duration:
                break
            if args.interval > 0:
                time.sleep(args.interval)
    except KeyboardInterrupt:
        pass
    except HID_ERRORS as error:
        print(f"\n{error}", file=sys.stderr)
    finally:
        if csv_file:
            csv_file.close()
        device.close()

    summarize(info, peaks, reached, scan_rate)


if __name__ == "__main__":
    main()
