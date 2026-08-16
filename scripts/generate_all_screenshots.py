#!/usr/bin/env python3
"""
generate_all_screenshots.py — Master automated screenshot generator for PebbleAAPS watchface.
"""
import os
import sys
import time
import subprocess

PATH_ENV = os.environ.get("PATH", "") + ":/home/clark/.local/share/pebble-sdk/SDKs/4.17/toolchain/bin"
ENV = dict(os.environ, PATH=PATH_ENV, DISPLAY=":99", QEMU_AUDIO_DRV="none", SDL_AUDIODRIVER="none")
PEBBLE_CLI = "/home/clark/.local/bin/pebble"
SCREENSHOT_DIR = "/home/clark/dev/PebbleAAPS/screenshots"

os.makedirs(SCREENSHOT_DIR, exist_ok=True)

def run(cmd):
    res = subprocess.run(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, env=ENV)
    print(f"CMD: {cmd}\nEXIT: {res.returncode}\nOUT: {res.stdout.strip()}\nERR: {res.stderr.strip()}\n")
    return res.returncode, res.stdout, res.stderr

def main():
    print("1. Cleaning old processes & X11 locks...")
    run("pkill -9 -f qemu-pebble; pkill -9 -f pypkjs; pkill -9 -f Xvfb; rm -f /tmp/.X99-lock /tmp/.X11-unix/X99 /tmp/.X*-lock; sleep 2")

    print("2. Starting Xvfb on display :99...")
    p_xvfb = subprocess.Popen(["Xvfb", ":99", "-screen", "0", "1280x800x24"], env=ENV)
    time.sleep(2)

    print("3. Launching persistent emulator via pebble logs...")
    p_emu = subprocess.Popen([PEBBLE_CLI, "logs", "--emulator", "emery", "--vnc"], env=ENV)
    time.sleep(14)

    print("4. Installing watchface PBW...")
    run(f"{PEBBLE_CLI} install build/PebbleAAPS.pbw --emulator emery")
    time.sleep(4)

    NOW = int(time.time())
    STALE_TIME = NOW - 1000

    states = [
        ("in_range", f"--int 0=120 1=5 4={NOW}"),
        ("high", f"--int 0=250 1=3 4={NOW}"),
        ("low", f"--int 0=55 1=7 4={NOW}"),
        ("arrow_forty_five_up", f"--int 0=150 1=4 4={NOW}"),
        ("arrow_forty_five_down", f"--int 0=100 1=6 4={NOW}"),
        ("stale", f"--int 0=140 1=5 4={STALE_TIME}"),
    ]

    for name, payload in states:
        print(f"\n=================== GENERATING SCREENSHOT FOR: {name} ===================")
        run(f"{PEBBLE_CLI} send-app-message --emulator emery {payload}")
        time.sleep(2)
        
        out_png = os.path.join(SCREENSHOT_DIR, f"{name}.png")
        run(f"{PEBBLE_CLI} screenshot --emulator emery --no-open '{out_png}'")
        time.sleep(1)

    print("\n================ FINAL GENERATED SCREENSHOTS ================")
    for f in sorted(os.listdir(SCREENSHOT_DIR)):
        path = os.path.join(SCREENSHOT_DIR, f)
        print(f" - {f} ({os.path.getsize(path)} bytes)")

    if p_emu: p_emu.terminate()
    if p_xvfb: p_xvfb.terminate()

if __name__ == "__main__":
    main()
