#!/usr/bin/env python3
"""
generate_all_4_screenshots.py — Generates full-info Light Mode screenshots for all 4 Pebble platforms:
- emery   (Pebble Time 2, 200x228 Color)
- basalt  (Pebble Time / Time Steel, 144x168 Color)
- chalk   (Pebble Time Round, 180x180 Color Circular)
- diorite (Pebble 2 / 2 SE, 144x168 B&W High-Contrast)
"""
import os
import sys
import math
from PIL import Image, ImageDraw, ImageFont

RESOURCE_SCREENSHOT_DIR = "/home/clark/dev/PebbleAAPS/resources/screenshots"
os.makedirs(RESOURCE_SCREENSHOT_DIR, exist_ok=True)

FONT_PATH = "/home/clark/dev/PebbleAAPS/resources/fonts/LilitaOne-Regular.ttf"

def render_emery_base():
    """Renders the base 200x228 Emery full-info Light Mode watchface."""
    img = Image.new("RGB", (200, 228), (255, 255, 255))
    draw = ImageDraw.Draw(img)

    font_48 = ImageFont.truetype(FONT_PATH, 42)
    font_32 = ImageFont.truetype(FONT_PATH, 24)
    font_22 = ImageFont.truetype(FONT_PATH, 16)

    # 1. Perimeter Ticks
    draw.line([(100, 0), (100, 10)], fill=(0,0,0), width=4)
    draw.line([(100, 228), (100, 218)], fill=(0,0,0), width=4)
    draw.line([(200, 114), (190, 114)], fill=(0,0,0), width=4)
    draw.line([(0, 114), (10, 114)], fill=(0,0,0), width=4)

    draw.line([(166, 0), (161, 9)], fill=(85,85,85), width=3)
    draw.line([(200, 56), (191, 61)], fill=(85,85,85), width=3)
    draw.line([(200, 172), (191, 167)], fill=(85,85,85), width=3)
    draw.line([(166, 228), (161, 219)], fill=(85,85,85), width=3)
    draw.line([(34, 228), (39, 219)], fill=(85,85,85), width=3)
    draw.line([(0, 172), (9, 167)], fill=(85,85,85), width=3)
    draw.line([(0, 56), (9, 61)], fill=(85,85,85), width=3)
    draw.line([(34, 0), (39, 9)], fill=(85,85,85), width=3)

    # 2. Top status: Delta & Age
    draw.text((10, 8), "(+3|+5)", font=font_22, fill=(0,0,0))
    draw.text((135, 8), "0'", font=font_22, fill=(0,0,0))

    # 3. BG Reading & Trend Arrow
    draw.text((25, 42), "120", font=font_48, fill=(0, 168, 80))
    
    # Arrow
    arrow_path = "/home/clark/dev/PebbleAAPS/resources/images/arrow_flat.png"
    if os.path.exists(arrow_path):
        with Image.open(arrow_path) as arrow_img:
            arrow_rgb = Image.new("RGB", arrow_img.size, (0, 168, 80))
            if arrow_img.mode in ('RGBA', 'LA') or (arrow_img.mode == 'P' and 'transparency' in arrow_img.info):
                alpha = arrow_img.convert('RGBA').split()[-1]
                img.paste(arrow_rgb, (130, 52), alpha)

    # 4. Status Column 1: IOB & Basal Rate
    draw.text((5, 92), "0.32 U", font=font_32, fill=(0,0,0))
    draw.text((5, 122), "0.90", font=font_22, fill=(85,85,85))

    # 5. Status Column 2: COB & Date
    draw.text((128, 92), "15g", font=font_32, fill=(0,0,0))
    draw.text((125, 122), "16 Aug", font=font_22, fill=(85,85,85))

    # 6. Detailed IOB
    draw.text((5, 150), "(0.02|0.31)", font=font_22, fill=(85,85,85))

    # 7. High & Low Target Lines
    for x in range(0, 200, 6):
        draw.line([(x, 190), (x+3, 190)], fill=(85,85,85), width=1)
        draw.line([(x, 212), (x+3, 212)], fill=(200,0,0), width=1)

    # 8. Glucose History Points
    graph_coords = [
        (10, 205), (15, 204), (20, 203), (25, 203), (30, 202), (35, 202),
        (40, 201), (45, 201), (50, 200), (55, 199), (60, 200), (65, 201),
        (70, 202), (75, 202), (80, 201), (85, 201), (90, 200), (95, 200),
        (100, 199), (105, 198), (110, 199), (115, 200), (120, 201), (125, 201),
        (130, 200), (135, 200), (140, 199), (145, 200), (150, 201), (155, 201),
        (160, 200), (165, 200), (170, 199), (175, 199), (180, 200), (185, 200)
    ]
    for gx, gy in graph_coords:
        draw.rectangle([gx-2, gy-2, gx+2, gy+2], fill=(0, 168, 80))

    # 9. Sweeping Analog Clock Hands
    cx, cy = 100, 114
    h_angle = math.radians(300)
    hx = cx + int(36 * math.sin(h_angle))
    hy = cy - int(36 * math.cos(h_angle))
    draw.line([(cx, cy), (hx, hy)], fill=(0,0,0), width=6)

    m_angle = math.radians(60)
    mx = cx + int(58 * math.sin(m_angle))
    my = cy - int(58 * math.cos(m_angle))
    draw.line([(cx, cy), (mx, my)], fill=(0,0,0), width=5)

    draw.ellipse([cx-6, cy-6, cx+6, cy+6], fill=(255,255,255), outline=(0,0,0), width=2)

    return img

def main():
    print("Generating screenshots for all 4 Pebble platforms...")
    emery_img = render_emery_base()

    # 1. Emery (200x228 Color)
    emery_path = os.path.join(RESOURCE_SCREENSHOT_DIR, "emery_screenshot.png")
    emery_img.save(emery_path, "PNG")
    print(f"Saved Emery screenshot (200x228): {emery_path}")

    # 2. Basalt (144x168 Color)
    basalt_img = emery_img.resize((144, 168), Image.Resampling.LANCZOS)
    basalt_path = os.path.join(RESOURCE_SCREENSHOT_DIR, "basalt_screenshot.png")
    basalt_img.save(basalt_path, "PNG")
    print(f"Saved Basalt screenshot (144x168): {basalt_path}")

    # 3. Chalk (180x180 Color Circular)
    chalk_square = emery_img.resize((180, 180), Image.Resampling.LANCZOS)
    chalk_img = Image.new("RGBA", (180, 180), (0, 0, 0, 0))
    mask = Image.new("L", (180, 180), 0)
    mask_draw = ImageDraw.Draw(mask)
    mask_draw.ellipse([0, 0, 180, 180], fill=255)
    chalk_img.paste(chalk_square, (0, 0), mask)
    chalk_path = os.path.join(RESOURCE_SCREENSHOT_DIR, "chalk_screenshot.png")
    chalk_img.save(chalk_path, "PNG")
    print(f"Saved Chalk screenshot (180x180 Round): {chalk_path}")

    # 4. Diorite (144x168 B&W High-Contrast)
    diorite_img = basalt_img.convert("L").point(lambda p: 255 if p > 160 else 0).convert("1")
    diorite_path = os.path.join(RESOURCE_SCREENSHOT_DIR, "diorite_screenshot.png")
    diorite_img.save(diorite_path, "PNG")
    print(f"Saved Diorite screenshot (144x168 B&W): {diorite_path}")

if __name__ == "__main__":
    main()
