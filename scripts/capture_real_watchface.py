#!/usr/bin/env /home/clark/.local/share/uv/tools/pebble-tool/bin/python
"""
capture_real_watchface.py — Installs PebbleAAPS PBW onto QEMU Emery, sends full AppMessage payload, and captures true watchface screenshot.
"""
import os
import sys
import time
import zipfile
import uuid
import threading
import subprocess
from PIL import Image

PATH_ENV = os.environ.get("PATH", "") + ":/home/clark/.local/share/pebble-sdk/SDKs/4.17/toolchain/bin"
ENV = dict(os.environ, PATH=PATH_ENV, DISPLAY=":99", QEMU_AUDIO_DRV="none", SDL_AUDIODRIVER="none")
SCREENSHOT_DIR = "/home/clark/dev/PebbleAAPS/screenshots"
RESOURCE_SCREENSHOT_DIR = "/home/clark/dev/PebbleAAPS/resources/screenshots"
os.makedirs(SCREENSHOT_DIR, exist_ok=True)
os.makedirs(RESOURCE_SCREENSHOT_DIR, exist_ok=True)

from libpebble2.communication.transports.qemu import QemuTransport
from libpebble2.communication import PebbleConnection
from libpebble2.services.putbytes import PutBytes, PutBytesType
from libpebble2.services.appmessage import AppMessageService, Int32, Uint32, CString, ByteArray
import libpebble2.protocol.apps as apps

def run(cmd):
    res = subprocess.run(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, env=ENV)
    print(f"CMD: {cmd}\nEXIT: {res.returncode}")
    return res.returncode, res.stdout, res.stderr

def main():
    print("1. Cleaning old processes & locks...")
    run("pkill -9 -f qemu-pebble; pkill -9 -f pypkjs; pkill -9 -f Xvfb; rm -f /tmp/.X99-lock /tmp/.X11-unix/X99 /tmp/.X*-lock; sleep 2")

    print("2. Starting Xvfb...")
    p_xvfb = subprocess.Popen(["Xvfb", ":99", "-screen", "0", "1280x800x24"], env=ENV)
    time.sleep(2)

    print("3. Launching QEMU Emery on port 12344...")
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

    print("4. Connecting directly to QEMU transport...")
    transport = QemuTransport(host='127.0.0.1', port=12344)
    pebble = PebbleConnection(transport)
    pebble.connect()
    pebble.run_async()
    print("Connected to QEMU! Watch info:", pebble.watch_info)

    zf = zipfile.ZipFile('build/PebbleAAPS.pbw')
    app_uuid = uuid.UUID('54d3008f-e144-4712-b201-24bc515c40ba')

    installed_event = threading.Event()

    def handle_fetch(packet):
        print('QEMU FETCH REQUESTED! Transferring app binary and resources...')
        pebble.send_packet(apps.AppFetchResponse(response=apps.AppFetchStatus.Start))
        
        def run_transfer():
            try:
                binary = zf.read('emery/pebble-app.bin')
                pb_bin = PutBytes(pebble, PutBytesType.Binary, binary, app_install_id=packet.app_id)
                pb_bin.send()
                
                resources = zf.read('emery/app_resources.pbpack')
                pb_res = PutBytes(pebble, PutBytesType.Resources, resources, app_install_id=packet.app_id)
                pb_res.send()
                print('App installation complete!')
                installed_event.set()
            except Exception as err:
                print('PutBytes transfer error:', err)

        t = threading.Thread(target=run_transfer)
        t.daemon = True
        t.start()

    pebble.register_endpoint(apps.AppFetchRequest, handle_fetch)
    print("Launching PebbleAAPS app...")
    pebble.send_packet(apps.AppRunState(data=apps.AppRunStateStart(uuid=app_uuid)))
    
    print("Waiting for app installation & startup...")
    installed_event.wait(timeout=15)
    time.sleep(3)

    app_msg = AppMessageService(pebble)
    NOW = int(time.time())

    # Build a realistic 36-point history curve around 120 (BG/2 = 60 = 0x3C)
    history_bytes = bytes([
        55, 56, 57, 58, 59, 60, 61, 62, 63, 62, 61, 60,
        59, 58, 59, 60, 61, 62, 63, 64, 63, 62, 61, 60,
        59, 60, 61, 62, 61, 60, 59, 60, 61, 60, 60, 60
    ])

    full_payload = {
        0: Int32(120),               # BG: 120 mg/dL
        1: Int32(5),                 # TREND: Flat (5)
        2: CString("0.32 U"),        # IOB
        3: CString("15g"),           # COB
        4: Uint32(NOW),              # TIME (fresh: Age = 0')
        5: CString("0.90"),          # BASAL
        6: CString("(0.02|0.31)"),   # IOB_DETAIL
        7: CString("+3"),            # DELTA
        8: CString("+5"),            # AVG_DELTA
        9: ByteArray(history_bytes), # GLUCOSE_HISTORY (36 points)
        10: Int32(70),               # LOW_TARGET
        11: Int32(180),              # HIGH_TARGET
        12: Int32(0),                # UNITS: mg/dL
    }

    print("\nSending fully populated AppMessage payload with all 13 keys...")
    app_msg.send_message(app_uuid, full_payload)
    time.sleep(3)

    # Use pebble screenshot CLI command while QEMU is running on DISPLAY=:99
    out_path_1 = os.path.join(RESOURCE_SCREENSHOT_DIR, "emery_screenshot.png")
    out_path_2 = os.path.join(SCREENSHOT_DIR, "emery_screenshot.png")
    out_path_3 = os.path.join(SCREENSHOT_DIR, "in_range.png")
    
    print(f"Capturing screenshot via pebble tool...")
    subprocess.run(f"DISPLAY=:99 pebble screenshot --emulator emery {out_path_1}", shell=True, env=ENV)
    subprocess.run(f"cp {out_path_1} {out_path_2}", shell=True)
    subprocess.run(f"cp {out_path_1} {out_path_3}", shell=True)

    if os.path.exists(out_path_1):
        print(f"SUCCESS: Saved screenshot to {out_path_1} ({os.path.getsize(out_path_1)} bytes)!")

    if p_qemu: p_qemu.terminate()
    if p_xvfb: p_xvfb.terminate()

if __name__ == "__main__":
    main()
