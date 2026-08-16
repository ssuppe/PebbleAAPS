#!/usr/bin/env python3
"""
activate_and_capture.py — Navigates Pebble OS menu to select AAPS Watchface and captures the watch screen via ImageMagick.
"""
import os
import sys
import time
import subprocess

PATH_ENV = os.environ.get("PATH", "") + ":/home/clark/.local/share/pebble-sdk/SDKs/4.17/toolchain/bin"
ENV = dict(os.environ, PATH=PATH_ENV, DISPLAY=":99", QEMU_AUDIO_DRV="none", SDL_AUDIODRIVER="none")
SCREENSHOT_DIR = "/home/clark/dev/PebbleAAPS/screenshots"

os.makedirs(SCREENSHOT_DIR, exist_ok=True)

def run(cmd):
    res = subprocess.run(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, env=ENV)
    print(f"CMD: {cmd}\nEXIT: {res.returncode}\nOUT: {res.stdout.strip()}\nERR: {res.stderr.strip()}\n")
    return res.returncode, res.stdout, res.stderr

def press_button(btn, delay=2.0):
    run(f"/home/clark/.local/bin/pebble emu-button --emulator emery click {btn}")
    time.sleep(delay)

def main():
    print("1. Clean old processes & X11 locks...")
    run("pkill -9 -f qemu-pebble; pkill -9 -f pypkjs; pkill -9 -f Xvfb; rm -f /tmp/.X99-lock /tmp/.X11-unix/X99 /tmp/.X*-lock; sleep 2")

    print("2. Starting Xvfb on display :99...")
    p_xvfb = subprocess.Popen(["Xvfb", ":99", "-screen", "0", "1280x800x24"], env=ENV)
    time.sleep(2)

    print("3. Launching persistent emulator via pebble logs...")
    p_emu = subprocess.Popen(["/home/clark/.local/bin/pebble", "logs", "--emulator", "emery", "--vnc"], env=ENV)
    print("Waiting 22s for Pebble OS firmware boot...")
    time.sleep(22)

    print("4. Installing watchface PBW...")
    run("/home/clark/.local/bin/pebble install build/PebbleAAPS.pbw --emulator emery")
    time.sleep(4)

    print("5. Navigating Pebble OS menu to select AAPS Watchface...")
    press_button("select")
    press_button("select")
    press_button("down")
    press_button("select")
    time.sleep(3)

    NOW = int(time.time())
    run(f"/home/clark/.local/bin/pebble send-app-message --emulator emery --int 0=120 1=5 4={NOW}")
    time.sleep(2)

    raw_png = os.path.join(SCREENSHOT_DIR, "activated_raw.png")
    final_png = os.path.join(SCREENSHOT_DIR, "activated_watchface.png")

    print(f"Capturing watchface render to {final_png}...")
    run(f"import -window root '{raw_png}'")
    run(f"convert '{raw_png}' -crop 200x228+540+286 +repage '{final_png}'")

    print(f"RESULT FILE SIZE: {os.path.getsize(final_png)} bytes")

    if p_emu: p_emu.terminate()
    if p_xvfb: p_xvfb.terminate()

if __name__ == "__main__":
    main()
