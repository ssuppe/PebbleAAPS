#!/usr/bin/env python3
"""
generate_store_icon.py — Generates a crisp 144x144 Rebble Store App Icon PNG.
"""
import os
from PIL import Image, ImageDraw, ImageFont

def generate_store_icon():
    size = 144
    img = Image.new("RGBA", (size, size), (255, 255, 255, 255))
    draw = ImageDraw.Draw(img)

    # Rounded outer border (Dark Slate / Charcoal)
    draw.rounded_rectangle([4, 4, 140, 140], radius=24, fill=(245, 247, 250), outline=(20, 24, 33), width=6)

    # Dial perimeter ticks (Cardinals)
    draw.line([(72, 12), (72, 22)], fill=(20, 24, 33), width=4)
    draw.line([(72, 132), (72, 122)], fill=(20, 24, 33), width=4)
    draw.line([(132, 72), (122, 72)], fill=(20, 24, 33), width=4)
    draw.line([(12, 72), (22, 72)], fill=(20, 24, 33), width=4)

    # BG Number "120" (Islamic Green)
    try:
        font_bg = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 36)
        font_sub = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 16)
    except:
        font_bg = ImageFont.load_default()
        font_sub = ImageFont.load_default()

    # Draw "120"
    draw.text((36, 40), "120", font=font_bg, fill=(0, 168, 89))
    
    # Trend arrow ->
    draw.polygon([(104, 52), (118, 58), (104, 64)], fill=(0, 168, 89))
    draw.rectangle([(94, 55), (104, 61)], fill=(0, 168, 89))

    # AAPS Subtitle
    draw.text((28, 84), "PebbleAAPS", font=font_sub, fill=(20, 24, 33))

    # Sweeping hands overlay
    cx, cy = 72, 72
    # Hour hand (10 o'clock)
    draw.line([(cx, cy), (48, 52)], fill=(20, 24, 33), width=4)
    # Minute hand (2 o'clock)
    draw.line([(cx, cy), (102, 54)], fill=(20, 24, 33), width=3)
    # Hub
    draw.ellipse([cx-4, cy-4, cx+4, cy+4], fill=(255, 255, 255), outline=(20, 24, 33), width=2)

    out_path = "/home/clark/dev/PebbleAAPS/resources/store_icon.png"
    img.save(out_path, "PNG", optimize=True)
    print(f"Saved Rebble Store App Icon (144x144) to {out_path}")

if __name__ == "__main__":
    generate_store_icon()
