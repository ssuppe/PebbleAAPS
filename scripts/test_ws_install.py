#!/usr/bin/env python3
"""
test_ws_install.py — Installs PBW via WebSocketInstallBundle packet over PebbleCommand transport.
"""
import os
import sys
import time
import subprocess

PATH_ENV = os.environ.get("PATH", "") + ":/home/clark/.local/share/pebble-sdk/SDKs/4.17/toolchain/bin"
ENV = dict(os.environ, PATH=PATH_ENV, DISPLAY=":99", QEMU_AUDIO_DRV="none", SDL_AUDIODRIVER="none")

from pebble_tool.commands.base import PebbleCommand
from libpebble2.communication.transports.websocket import MessageTargetPhone
from libpebble2.communication.transports.websocket.protocol import WebSocketInstallBundle, WebSocketInstallStatus

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

    print("5. Sending WebSocketInstallBundle packet...")
    queue = pebble.get_endpoint_queue(WebSocketInstallStatus)
    with open('build/PebbleAAPS.pbw', 'rb') as f:
        pebble.transport.send_packet(WebSocketInstallBundle(pbw=f.read()), target=MessageTargetPhone())
    
    print("6. Waiting for WebSocketInstallStatus...")
    status = queue.get(timeout=30)
    print("Install status result:", status.result)

    if p_emu: p_emu.terminate()
    if p_xvfb: p_xvfb.terminate()

if __name__ == "__main__":
    main()
