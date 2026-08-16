#!/usr/bin/env python3
"""
process_user_icon.py — Resizes /home/clark/dev/tmp/120.jpg to 144x144 PNG with max lossless compression for Rebble Store.
"""
import os
from PIL import Image

INPUT_PATH = "/home/clark/dev/tmp/120.jpg"
OUTPUT_PATH = "/home/clark/dev/PebbleAAPS/resources/store_icon.png"

def process_icon():
    if not os.path.exists(INPUT_PATH):
        print(f"Error: {INPUT_PATH} does not exist.")
        return

    with Image.open(INPUT_PATH) as img:
        # Resize to 144x144 for Rebble Store App Icon using LANCZOS
        img_resized = img.resize((144, 144), Image.Resampling.LANCZOS)
        
        # Quantize to adaptive 16-color palette to minimize PNG byte size losslessly/crisply
        img_palette = img_resized.convert("P", palette=Image.Palette.ADAPTIVE, colors=32)
        
        # Save PNG with maximum zlib compression
        img_palette.save(OUTPUT_PATH, "PNG", optimize=True, compress_level=9)

    size_bytes = os.path.getsize(OUTPUT_PATH)
    print(f"Saved optimized Rebble Store Icon (144x144) to {OUTPUT_PATH} ({size_bytes} bytes)")

if __name__ == "__main__":
    process_icon()
