#!/usr/bin/env python3
"""
Hardware test runner for the SparkFun IoT RedBoard - ESP32-P4 (WiFi/BLE) board file.

Builds and uploads RedBoardP4_WiFi_Test with arduino-cli, then runs the on-board test suite
across several boots: the first boot after upload, software restarts (ESP.restart) and hard
resets (USB Serial/JTAG RTS). Every boot has to bring the ESP32-C5 back up, which exercises
the C5 reset handling in the variant.

Usage:
    python3 run_tests.py --port /dev/cu.usbmodem1301 [--fqbn ...] [--no-upload] [--boot-button]

Requires pyserial and arduino-cli. Exit status is 0 only if every boot passes.
"""
import argparse
import os
import re
import subprocess
import sys
import time

import serial

HERE = os.path.dirname(os.path.abspath(__file__))
SKETCH = os.path.join(HERE, "RedBoardP4_WiFi_Test")
DEFAULT_FQBN = "esp32:esp32:sparkfun_esp32p4_iot_redboard_wifi"


def open_port(port, timeout=15.0):
    """Open the port, retrying while USB Serial/JTAG re-enumerates after a reset."""
    t0 = time.time()
    while True:
        try:
            # Set DTR/RTS before opening: toggling them on the USB Serial/JTAG port resets the chip.
            s = serial.Serial(None, 115200, timeout=0.2)
            s.port = port
            s.dtr = False
            s.rts = False
            s.open()
            return s
        except (serial.SerialException, OSError):
            if time.time() - t0 > timeout:
                raise
            time.sleep(0.2)


def hard_reset(port):
    s = open_port(port)
    s.rts = True  # EN low
    time.sleep(0.1)
    s.rts = False
    s.close()
    time.sleep(1.0)


def run_suite(port, timeout=90.0, boot_button=False, network=True):
    """Start the suite on a freshly booted board and collect its RESULT lines."""
    s = open_port(port)
    results, done, log = {}, None, []
    t0 = time.time()
    next_start = 0.0
    begun = False
    while time.time() - t0 < timeout:
        if not begun and time.time() > next_start:
            s.write(b"S" if network else b"N")  # the sketch also auto-starts (with network) after 10 s
            next_start = time.time() + 1.0
        try:
            raw = s.readline()
        except serial.SerialException:
            s.close()
            s = open_port(port)
            continue
        if not raw:
            continue
        line = raw.decode(errors="replace").rstrip()
        log.append(line)
        if line.startswith("BEGIN"):
            begun = True
            print("   ", line)
        m = re.match(r"RESULT (\S+) (PASS|FAIL|SKIP|MANUAL) ?(.*)", line)
        if m:
            results[m.group(1)] = (m.group(2), m.group(3))
            print(f"    {m.group(2):6s} {m.group(1):22s} {m.group(3)}")
        if any(s in line for s in ("Guru Meditation", "assert failed", "Identified slave", "Brownout")):
            print("    !!", line)
            results.setdefault("crash", ("FAIL", line))
        if begun and line.startswith("BEGIN") and len([l for l in log if l.startswith("BEGIN")]) > 1:
            print("    board rebooted during the suite")
            break
        if line.startswith("DONE"):
            done = dict(kv.split("=") for kv in line.split()[1:])
            break
    if done and boot_button:
        s.write(b"B")
        while time.time() - t0 < timeout + 20:
            line = s.readline().decode(errors="replace").rstrip()
            if line.startswith("PROMPT"):
                print("   >>>", line[7:])
            m = re.match(r"RESULT (boot_button) (PASS|FAIL) ?(.*)", line)
            if m:
                results[m.group(1)] = (m.group(2), m.group(3))
                print(f"    {m.group(2):6s} {m.group(1):22s} {m.group(3)}")
                break
    s.close()
    if done is None:
        print("    no DONE line - last output:")
        for line in log[-15:]:
            print("     |", line)
    return results, done


def soft_restart(port):
    s = open_port(port)
    s.write(b"R")
    s.flush()
    s.close()
    time.sleep(1.0)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", required=True)
    ap.add_argument("--fqbn", default=DEFAULT_FQBN)
    ap.add_argument("--no-upload", action="store_true")
    ap.add_argument("--soft", type=int, default=2, help="software restarts to test")
    ap.add_argument("--hard", type=int, default=2, help="hard (RTS) resets to test")
    ap.add_argument("--boot-button", action="store_true", help="also run the interactive BOOT button test")
    ap.add_argument("--no-network", action="store_true", help="skip the WiFi/BLE (ESP-Hosted) tests")
    args = ap.parse_args()

    if not args.no_upload:
        print(f"== Building and uploading {args.fqbn}")
        r = subprocess.run(["arduino-cli", "compile", "-u", "-p", args.port, "-b", args.fqbn, SKETCH])
        if r.returncode:
            sys.exit("upload failed")
        time.sleep(1.0)
    else:
        hard_reset(args.port)

    boots = [("after upload" if not args.no_upload else "hard reset", None)]
    boots += [("soft restart", soft_restart)] * args.soft
    boots += [("hard reset", hard_reset)] * args.hard

    summary = []
    for i, (kind, action) in enumerate(boots):
        if action:
            action(args.port)
        print(f"== Boot {i + 1}/{len(boots)} ({kind})")
        results, done = run_suite(args.port, boot_button=args.boot_button and i == 0, network=not args.no_network)
        fails = [k for k, (st, _) in results.items() if st == "FAIL"]
        ok = done is not None and not fails
        summary.append((kind, ok, fails if done else ["no DONE"]))

    print("\n== Summary")
    for i, (kind, ok, fails) in enumerate(summary):
        print(f"  boot {i + 1} {kind:13s} {'PASS' if ok else 'FAIL ' + ', '.join(fails)}")
    sys.exit(0 if all(ok for _, ok, _ in summary) else 1)


if __name__ == "__main__":
    main()
