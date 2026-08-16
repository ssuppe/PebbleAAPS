#!/usr/bin/env python3
"""
capture_real_screenshots.py — Direct QEMU execution, app installation, AppMessage injection, and screenshot generation.
"""
import os
import sys
import time
import zipfile
import uuid
import threading
import subprocess

PATH_ENV = os.environ.get("PATH", "") + ":/home/clark/.local/share/pebble-sdk/SDKs/4.17/toolchain/bin"
ENV = dict(os.environ, PATH=PATH_ENV, DISPLAY=":99", QEMU_AUDIO_DRV="none", SDL_AUDIODRIVER="none")
SCREENSHOT_DIR = "/home/clark/dev/PebbleAAPS/screenshots"

os.makedirs(SCREENSHOT_DIR, exist_ok=True)

from libpebble2.communication.transports.qemu import QemuTransport
from libpebble2.communication import PebbleConnection
from libpebble2.services.putbytes import PutBytes, PutBytesType
from libpebble2.services.screenshot import Screenshot
from libpebble2.services.appmessage import AppMessageService, Int32, Uint32
import libpebble2.protocol.apps as apps

def run(cmd):
    res = subprocess.run(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, env=ENV)
    print(f"CMD: {cmd}\nEXIT: {res.returncode}\nOUT: {res.stdout.strip()}\nERR: {res.stderr.strip()}\n")
    return res.returncode, res.stdout, res.stderr

def main():
    print("1. Clean old processes & locks...")
    run("pkill -9 -f qemu-pebble; pkill -9 -f pypkjs; pkill -9 -f Xvfb; rm -f /tmp/.X99-lock /tmp/.X11-unix/X99 /tmp/.X*-lock; sleep 2")

    print("2. Starting Xvfb...")
    p_xvfb = subprocess.Popen(["Xvfb", ":99", "-screen", "0", "1280x800x24"], env=ENV)
    time.sleep(2)

    print("3. Launching QEMU standalone on port 12344...")
    qemu_cmd = [
        "/home/clark/.local/share/pebble-sdk/SDKs/4.17/toolchain/bin/qemu-pebble",
        "-rtc", "base=localtime",
        "-serial", "null",
        "-serial", "tcp::12344,server=on,wait=off",
        "-kernel", "/home/clark/.local/share/pebble-sdk/SDKs/4.17/sdk-core/pebble/emery/qemu/qemu_micro_flash.bin",
        "-machine", "pebble-emery",
        "-cpu", "cortex-m33",
        "-drive", "if=mtd,format=raw,file=/home/clark/.local/share/pebble-sdk/4.17/emery/qemu_spi_flash.bin",
        "-audio", "driver=none,id=audio0"
    ]
    p_qemu = subprocess.Popen(qemu_cmd, env=ENV)
    time.sleep(4)

    print("4. Connecting directly to QEMU serial transport (port 12344)...")
    transport = QemuTransport(host='127.0.0.1', port=12344)
    pebble = PebbleConnection(transport)
    pebble.connect()
    pebble.run_async()
    print("Connected to QEMU! Watch info:", pebble.watch_info)

    zf = zipfile.ZipFile('build/PebbleAAPS.pbw')
    app_uuid = uuid.UUID('54d3008f-e144-4712-b201-24bc515c40ba')

    def handle_fetch(packet):
        print('QEMU FETCH REQUESTED! Spawning PutBytes transfer thread...')
        pebble.send_packet(apps.AppFetchResponse(response=apps.AppFetchStatus.Start))
        
        def run_transfer():
            try:
                binary = zf.read('emery/pebble-app.bin')
                pb_bin = PutBytes(pebble, PutBytesType.Binary, binary, app_install_id=packet.app_id)
                pb_bin.send()
                
                resources = zf.read('emery/app_resources.pbpack')
                pb_res = PutBytes(pebble, PutBytesType.Resources, resources, app_install_id=packet.app_id)
                pb_res.send()
                print('PUTBYTES TRANSFER SUCCEEDED!')
            except Exception as err:
                print('PutBytes transfer exception:', err)

        t = threading.Thread(target=run_transfer)
        t.daemon = True
        t.start()

    pebble.register_endpoint(apps.AppFetchRequest, handle_fetch)
    print("Sending AppRunStateStart...")
    pebble.send_packet(apps.AppRunState(data=apps.AppRunStateStart(uuid=app_uuid)))
    print("Waiting 15s for PutBytes transfer & app boot...")
    time.sleep(15)

    app_msg = AppMessageService(pebble)
    NOW = int(time.time())
    STALE_TIME = NOW - 1000

    states = [
        ("in_range", {0: Int32(120), 1: Int32(5), 4: Uint32(NOW)}),
        ("high", {0: Int32(250), 1: Int32(3), 4: Uint32(NOW)}),
        ("low", {0: Int32(55), 1: Int32(7), 4: Uint32(NOW)}),
        ("arrow_forty_five_up", {0: Int32(150), 1: Int32(4), 4: Uint32(NOW)}),
        ("arrow_forty_five_down", {0: Int32(100), 1: Int32(6), 4: Uint32(NOW)}),
        ("stale", {0: Int32(140), 1: Int32(5), 4: Uint32(STALE_TIME)}),
    ]

    for name, payload in states:
        print(f"\n---> Triggering state: {name}")
        app_msg.send_message(app_uuid, payload)
        time.sleep(2)
        
        scr_path = os.path.join(SCREENSHOT_DIR, f"{name}.png")
        print(f"Capturing screenshot: {scr_path}...")
        try:
            scr = Screenshot(pebble)
            img = scr.grab_image()
            img.save(scr_path)
            print(f"SAVED {name}.png ({os.path.getsize(scr_path)} bytes)")
        except Exception as e:
            import traceback
            print(f"Screenshot error for {name}: {e}")
            traceback.print_exc()

    print("\n================ CAPTURE COMPLETED ================")
    for f in sorted(os.listdir(SCREENSHOT_DIR)):
        path = os.path.join(SCREENSHOT_DIR, f)
        print(f" - {f} ({os.path.getsize(path)} bytes)")

    if p_qemu: p_qemu.terminate()
    if p_xvfb: p_xvfb.terminate()

if __name__ == "__main__":
    main()
