#!/usr/bin/env python3
"""
optimize_png_assets.py — Losslessly strip metadata and optimize compression for PNG assets.
"""
import os
from PIL import Image

IMAGE_DIR = "/home/clark/dev/PebbleAAPS/resources/images"

def optimize_png(file_path):
    orig_size = os.path.getsize(file_path)
    with Image.open(file_path) as img:
        # Convert to 1-bit or P mode (palette) losslessly
        img_converted = img.convert("RGBA") if img.mode != "RGBA" else img
        
        # Save with maximum zlib compression and zero info metadata
        img_converted.save(file_path, "PNG", optimize=True, compress_level=9)
    new_size = os.path.getsize(file_path)
    print(f"Optimized {os.path.basename(file_path)}: {orig_size}B -> {new_size}B")

def main():
    print("Optimizing PNG assets in resources/images...")
    for f in sorted(os.listdir(IMAGE_DIR)):
        if f.endswith(".png"):
            optimize_png(os.path.join(IMAGE_DIR, f))

if __name__ == "__main__":
    main()
