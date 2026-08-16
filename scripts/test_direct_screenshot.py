#!/usr/bin/env python3
import os
import time
import zipfile
import uuid
import threading
import subprocess

from libpebble2.communication.transports.qemu import QemuTransport
from libpebble2.communication import PebbleConnection
from libpebble2.services.putbytes import PutBytes, PutBytesType
from libpebble2.services.screenshot import Screenshot
from libpebble2.services.appmessage import AppMessageService, Int32, Uint32
import libpebble2.protocol.apps as apps

PATH_ENV = os.environ.get("PATH", "") + ":/home/clark/.local/share/pebble-sdk/SDKs/4.17/toolchain/bin"
ENV = dict(os.environ, PATH=PATH_ENV, DISPLAY=":99", QEMU_AUDIO_DRV="none", SDL_AUDIODRIVER="none")
SCREENSHOT_DIR = "/home/clark/dev/PebbleAAPS/screenshots"

os.makedirs(SCREENSHOT_DIR, exist_ok=True)

subprocess.run("pkill -9 -f qemu-pebble; pkill -9 -f pypkjs; pkill -9 -f Xvfb; rm -f /tmp/.X99-lock /tmp/.X11-unix/X99 /tmp/.X*-lock; sleep 2", shell=True, env=ENV)

p_xvfb = subprocess.Popen(["Xvfb", ":99", "-screen", "0", "1280x800x24"], env=ENV)
time.sleep(2)

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
time.sleep(3)

transport = QemuTransport(host='127.0.0.1', port=12344)
pebble = PebbleConnection(transport)
pebble.connect()
pebble.run_async()
print("Connected to QEMU! Watch info:", pebble.watch_info)

zf = zipfile.ZipFile('build/PebbleAAPS.pbw')
app_uuid = uuid.UUID('54d3008f-e144-4712-b201-24bc515c40ba')

transfer_done = threading.Event()

def handle_fetch(packet):
    print("QEMU AppFetch requested!")
    pebble.send_packet(apps.AppFetchResponse(response=apps.AppFetchStatus.Start))
    
    def run_transfer():
        try:
            binary = zf.read('emery/pebble-app.bin')
            pb_bin = PutBytes(pebble, PutBytesType.Binary, binary, app_install_id=packet.app_id)
            pb_bin.send()
            
            resources = zf.read('emery/app_resources.pbpack')
            pb_res = PutBytes(pebble, PutBytesType.Resources, resources, app_install_id=packet.app_id)
            pb_res.send()
            print("PutBytes transfer completed successfully!")
        except Exception as e:
            import traceback
            print("PutBytes error:", e)
            traceback.print_exc()
        finally:
            transfer_done.set()

    t = threading.Thread(target=run_transfer)
    t.daemon = True
    t.start()

pebble.register_endpoint(apps.AppFetchRequest, handle_fetch)

print("Sending AppRunStateStart...")
pebble.send_packet(apps.AppRunState(data=apps.AppRunStateStart(uuid=app_uuid)))

print("Waiting for transfer...")
transfer_done.wait(timeout=20)
time.sleep(3)

print("Sending AppMessage payload...")
app_msg = AppMessageService(pebble)
NOW = int(time.time())
app_msg.send_message(app_uuid, {0: Int32(120), 1: Int32(5), 4: Uint32(NOW)})
time.sleep(3)

print("Grabbing screenshot via Screenshot service...")
scr = Screenshot(pebble)
img = scr.grab_image()
out_file = os.path.join(SCREENSHOT_DIR, "direct_render.png")
img.save(out_file)
print(f"SUCCESS! Saved {out_file} ({os.path.getsize(out_file)} bytes)")

p_qemu.terminate()
p_xvfb.terminate()
