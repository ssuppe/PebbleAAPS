#!/usr/bin/env python3
"""
capture_screenshots.py — Automate watchface state testing and screenshot capture.
Uses xwd to capture the exact Xvfb display output of QEMU for each state.
"""
import os
import sys
import time
import subprocess

PEBBLE_CLI = "/home/clark/.local/bin/pebble"
SCREENSHOT_DIR = "/home/clark/dev/PebbleAAPS/screenshots"

os.makedirs(SCREENSHOT_DIR, exist_ok=True)

def run(cmd):
    res = subprocess.run(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    print(f"CMD: {cmd}\nEXIT: {res.returncode}\nOUT: {res.stdout.strip()}\nERR: {res.stderr.strip()}\n")
    return res.returncode, res.stdout, res.stderr

def capture(name):
    display = os.environ.get("DISPLAY", ":99")
    raw_xwd = f"/tmp/{name}.xwd"
    out_png = os.path.join(SCREENSHOT_DIR, f"{name}.png")
    # Capture root window of Xvfb display
    run(f"xwd -root -display {display} -out {raw_xwd}")
    # Convert and crop/trim to emulator window
    run(f"convert {raw_xwd} -trim {out_png}")
    if os.path.exists(raw_xwd):
        os.remove(raw_xwd)

def main():
    print("Starting screenshot generation script...")
    
    # 1. Start QEMU in background via pebble tool
    print("Launching pebble emulator...")
    p_emu = subprocess.Popen([PEBBLE_CLI, "install", "--emulator", "emery", "--vnc"])
    time.sleep(12)  # Allow boot
    
    # 2. Re-install watchface bundle
    print("Installing watchface bundle...")
    run(f"{PEBBLE_CLI} install --emulator emery build/PebbleAAPS.pbw")
    time.sleep(3)

    NOW = int(time.time())
    STALE_TIME = NOW - 1000  # 16 minutes ago (stale threshold >= 15m)

    # List of test payloads to send and screenshot filename
    states = [
        ("no_data", None),                                  # default state
        ("in_range", f"--int 0=120 1=5 4={NOW}"),          # 120 mg/dL, Flat Right (Green)
        ("high", f"--int 0=250 1=3 4={NOW}"),              # 250 mg/dL, Single Up (Orange)
        ("low", f"--int 0=55 1=7 4={NOW}"),                # 55 mg/dL, Single Down (Red)
        ("arrow_forty_five_up", f"--int 0=150 1=4 4={NOW}"),# 150 mg/dL, 45° Up
        ("arrow_forty_five_down", f"--int 0=100 1=6 4={NOW}"),# 100 mg/dL, 45° Down
        ("stale", f"--int 0=140 1=5 4={STALE_TIME}"),      # 140 mg/dL, 16m ago (stale)
    ]

    for name, payload in states:
        print(f"\n--- Testing state: {name} ---")
        if payload:
            run(f"{PEBBLE_CLI} send-app-message --emulator emery {payload}")
            time.sleep(1)
        
        capture(name)
        time.sleep(1)

    print("\nFinished capture sequence. Files in screenshots/:")
    for f in sorted(os.listdir(SCREENSHOT_DIR)):
        print(f" - {f}")

    if p_emu:
        p_emu.terminate()

if __name__ == "__main__":
    main()
