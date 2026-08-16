#!/usr/bin/env python3
"""
test_pypkjs_install.py — Connects to running emulator & installs PBW via AppInstaller.
"""
import os
import sys
import time
import subprocess

PATH_ENV = os.environ.get("PATH", "") + ":/home/clark/.local/share/pebble-sdk/SDKs/4.17/toolchain/bin"
ENV = dict(os.environ, PATH=PATH_ENV, DISPLAY=":99", QEMU_AUDIO_DRV="none", SDL_AUDIODRIVER="none")

from pebble_tool.commands.base import PebbleCommand
from libpebble2.services.install import AppInstaller

class DummyArgs:
    v = 0
    pbw = 'build/PebbleAAPS.pbw'
    emulator = 'emery'
    vnc = True
    sdk = '4.17'
    phone = None
    qemu = None
    serial = None
    cloudpebble = False
    pypkjs = False
    platform = None
    force = True
    throttle = 0.01
    logs = False
    qemu_logs = False

def main():
    print("1. Clean old processes & X11 locks...")
    subprocess.run("pkill -9 -f qemu-pebble; pkill -9 -f pypkjs; pkill -9 -f Xvfb; rm -f /tmp/.X99-lock /tmp/.X11-unix/X99 /tmp/.X*-lock; sleep 2", shell=True, env=ENV)

    print("2. Starting Xvfb...")
    p_xvfb = subprocess.Popen(["Xvfb", ":99", "-screen", "0", "1280x800x24"], env=ENV)
    time.sleep(2)

    print("3. Launching persistent emulator via pebble logs...")
    p_emu = subprocess.Popen(["/home/clark/.local/bin/pebble", "logs", "--emulator", "emery", "--vnc"], env=ENV)
    time.sleep(15)

    print("4. Connecting via PebbleCommand...")
    cmd = PebbleCommand()
    pebble = cmd._connect(DummyArgs())
    print("Connected! Watch info:", pebble.watch_info)

    print("5. Installing PBW via AppInstaller...")
    installer = AppInstaller(pebble, "build/PebbleAAPS.pbw")
    installer.install(force_install=True)
    print("INSTALLED SUCCESSFULLY!")

    if p_emu: p_emu.terminate()
    if p_xvfb: p_xvfb.terminate()

if __name__ == "__main__":
    main()
