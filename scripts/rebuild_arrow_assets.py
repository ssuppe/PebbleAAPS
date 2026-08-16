#!/usr/bin/env python3
"""
rebuild_arrow_assets.py — Generates solid white-on-transparent 36x36 PNG arrow assets from arrow-right.png source.
"""
import os
from PIL import Image

SOURCE_PATH = "/home/clark/dev/tmp/arrow-right.png"
TARGET_DIR = "/home/clark/dev/PebbleAAPS/resources/images"

os.makedirs(TARGET_DIR, exist_ok=True)

def process_and_resize(img, rotation_angle):
    # 1. Convert to RGBA
    rgba = img.convert("RGBA")
    
    # 2. Threshold alpha channel: solid white (255,255,255,255) vs fully transparent (255,255,255,0)
    w, h = rgba.size
    solid = Image.new("RGBA", (w, h), (255, 255, 255, 0))
    for y in range(h):
        for x in range(w):
            r, g, b, a = rgba.getpixel((x, y))
            if a > 100:
                solid.putpixel((x, y), (255, 255, 255, 255))
            else:
                solid.putpixel((x, y), (255, 255, 255, 0))

    # 3. Rotate if angle specified (expand=False, resample=Image.NEAREST for sharp pixel edges)
    if rotation_angle != 0:
        rotated = solid.rotate(rotation_angle, resample=Image.NEAREST, expand=False)
    else:
        rotated = solid

    # 4. Resize to 36x36 using NEAREST resampling to preserve sharp 1-bit alpha edge
    resized = rotated.resize((36, 36), Image.NEAREST)

    # 5. Clean up any interpolation artifacts to maintain strict 2-color (Solid White / Transparent)
    final_img = Image.new("RGBA", (36, 36), (255, 255, 255, 0))
    rw, rh = resized.size
    for y in range(rh):
        for x in range(rw):
            r, g, b, a = resized.getpixel((x, y))
            if a > 100:
                final_img.putpixel((x, y), (255, 255, 255, 255))
            else:
                final_img.putpixel((x, y), (255, 255, 255, 0))

    return final_img

def process_double_arrow(src_path):
    img = Image.open(src_path).convert("RGBA")
    w, h = img.size
    solid = Image.new("RGBA", (w, h), (255, 255, 255, 0))
    for y in range(h):
        for x in range(w):
            r, g, b, a = img.getpixel((x, y))
            if a > 100 and (r < 100 or g < 100 or b < 100):
                solid.putpixel((x, y), (255, 255, 255, 255))

    aspect = w / h
    max_size = 36
    new_w = max_size
    new_h = int(round(max_size / aspect))

    resized = solid.resize((new_w, new_h), Image.LANCZOS)
    canvas = Image.new("RGBA", (36, 36), (255, 255, 255, 0))
    offset_x = (36 - new_w) // 2
    offset_y = (36 - new_h) // 2
    canvas.paste(resized, (offset_x, offset_y), mask=resized)

    final_up = Image.new("RGBA", (36, 36), (255, 255, 255, 0))
    for y in range(36):
        for x in range(36):
            r, g, b, a = canvas.getpixel((x, y))
            if a > 100:
                final_up.putpixel((x, y), (255, 255, 255, 255))
    
    final_down = final_up.rotate(180)
    return final_up, final_down

def main():
    src = Image.open(SOURCE_PATH)

    # Rotations from source arrow-right.png (points right = 0 deg)
    arrow_configs = {
        "arrow_flat.png": 0,
        "arrow_forty_five_up.png": 45,        # 45 deg counter-clockwise (up-right)
        "arrow_forty_five_down.png": -45,     # 45 deg clockwise (down-right)
        "arrow_single_up.png": 90,            # 90 deg counter-clockwise (straight up)
        "arrow_single_down.png": -90,         # 90 deg clockwise (straight down)
    }

    print("Rebuilding solid white 36x36 PNG arrow assets...")
    for filename, angle in arrow_configs.items():
        out_path = os.path.join(TARGET_DIR, filename)
        processed = process_and_resize(src, angle)
        processed.save(out_path, format="PNG")
        colors = processed.getcolors()
        print(f"Generated {filename} ({os.path.getsize(out_path)} bytes) — Colors: {colors}")

    double_src = "/home/clark/dev/tmp/doubleuporiginal.png"
    if os.path.exists(double_src):
        dup, ddown = process_double_arrow(double_src)
        p_up = os.path.join(TARGET_DIR, "arrow_double_up.png")
        p_down = os.path.join(TARGET_DIR, "arrow_double_down.png")
        dup.save(p_up, format="PNG")
        ddown.save(p_down, format="PNG")
        print(f"Generated arrow_double_up.png ({os.path.getsize(p_up)} bytes) — Colors: {dup.getcolors()}")
        print(f"Generated arrow_double_down.png ({os.path.getsize(p_down)} bytes) — Colors: {ddown.getcolors()}")

if __name__ == "__main__":
    main()
