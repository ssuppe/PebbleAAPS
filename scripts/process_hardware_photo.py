#!/usr/bin/env python3
"""
process_hardware_photo.py — Resizes wristwatch photo to 1024px height, completely strips EXIF data, and saves to assets/.
"""
import os
from PIL import Image

INPUT_PATH = "/home/clark/dev/tmp/pebbless.jpg"
ASSETS_DIR = "/home/clark/dev/PebbleAAPS/assets"
OUTPUT_PATH = os.path.join(ASSETS_DIR, "pebble_hardware_photo.jpg")

def process_photo():
    os.makedirs(ASSETS_DIR, exist_ok=True)
    if not os.path.exists(INPUT_PATH):
        print(f"Error: {INPUT_PATH} not found.")
        return

    with Image.open(INPUT_PATH) as img:
        # Calculate aspect ratio scaling to target height 1024px
        target_height = 1024
        w, h = img.size
        aspect = w / h
        target_width = int(target_height * aspect)

        # Resize using LANCZOS
        img_resized = img.resize((target_width, target_height), Image.Resampling.LANCZOS)

        # Create a fresh image to guarantee EXIF and metadata stripping
        clean_img = Image.new(img_resized.mode, img_resized.size)
        clean_img.putdata(list(img_resized.getdata()))

        # Save to assets/ without any EXIF dictionary
        clean_img.save(OUTPUT_PATH, "JPEG", quality=92, optimize=True)

    size_bytes = os.path.getsize(OUTPUT_PATH)
    print(f"Processed photo: {w}x{h} -> {target_width}x{target_height}, stripped EXIF, saved to {OUTPUT_PATH} ({size_bytes} bytes)")

if __name__ == "__main__":
    process_photo()
