#!/usr/bin/env python3
"""
generate_clean_screenshots.py — High-contrast Light Mode Emery (200x228) watchface pixel renderer.
Renders all 13 data fields according to docs/design/pebble_aaps_design_spec.md:
- Top: Delta (+3), Age (0')
- Middle: BG (120), Trend Arrow
- Left Column: IOB (0.32 U), Basal (0.90)
- Right Column: COB (15g), Date (16 Aug)
- Full Width Row 3: Detailed IOB (0.02|0.31)
- Bottom: High Target (190px gray dashed line), Low Target (212px red dashed line), 36-point history curve
- Clock: 12, 3, 6, 9 ticks + analog hour & minute hands
"""
import os
import sys
import math
from PIL import Image, ImageDraw, ImageFont

SCREENSHOT_DIR = "/home/clark/dev/PebbleAAPS/screenshots"
RESOURCE_SCREENSHOT_DIR = "/home/clark/dev/PebbleAAPS/resources/screenshots"
os.makedirs(SCREENSHOT_DIR, exist_ok=True)
os.makedirs(RESOURCE_SCREENSHOT_DIR, exist_ok=True)

FONT_PATH = "/home/clark/dev/PebbleAAPS/resources/fonts/LilitaOne-Regular.ttf"
FONT_48 = ImageFont.truetype(FONT_PATH, 42) # BG
FONT_32 = ImageFont.truetype(FONT_PATH, 24) # IOB, COB
FONT_22 = ImageFont.truetype(FONT_PATH, 16) # Delta, Age, Basal, Detail, Date

COLOR_GREEN = (0, 168, 80)      # GColorIslamicGreen (#00A850)
COLOR_ORANGE = (255, 140, 0)    # GColorOrange (#FF8C00)
COLOR_RED = (200, 0, 0)         # GColorBulgarianRose (#C80000)
COLOR_GRAY = (85, 85, 85)       # GColorDarkGray (#555555)
COLOR_WHITE = (255, 255, 255)
COLOR_BLACK = (0, 0, 0)

ARROW_MAP = {
    "flat": "/home/clark/dev/PebbleAAPS/resources/images/arrow_flat.png",
    "forty_five_up": "/home/clark/dev/PebbleAAPS/resources/images/arrow_forty_five_up.png",
    "forty_five_down": "/home/clark/dev/PebbleAAPS/resources/images/arrow_forty_five_down.png",
    "single_up": "/home/clark/dev/PebbleAAPS/resources/images/arrow_single_up.png",
    "single_down": "/home/clark/dev/PebbleAAPS/resources/images/arrow_single_down.png",
    "double_up": "/home/clark/dev/PebbleAAPS/resources/images/arrow_double_up.png",
    "double_down": "/home/clark/dev/PebbleAAPS/resources/images/arrow_double_down.png",
}

def render_watchface(bg_val, delta_str, age_str, color_rgb, arrow_type, iob_str="0.32 U", cob_str="15g", basal_str="0.90", detail_str="(0.02|0.31)", date_str="16 Aug", show_strike=False):
    # Base 200x228 canvas — High-Contrast Light Mode (White background)
    img = Image.new("RGB", (200, 228), COLOR_WHITE)
    draw = ImageDraw.Draw(img)

    # 1. Perimeter Ticks (All 12 hours)
    # Cardinal ticks (12, 3, 6, 9 o'clock) — Black, 4px stroke (thickest)
    draw.line([(100, 0), (100, 10)], fill=COLOR_BLACK, width=4)
    draw.line([(100, 228), (100, 218)], fill=COLOR_BLACK, width=4)
    draw.line([(200, 114), (190, 114)], fill=COLOR_BLACK, width=4)
    draw.line([(0, 114), (10, 114)], fill=COLOR_BLACK, width=4)

    # Diagonal corner ticks (1, 2, 4, 5, 7, 8, 10, 11 o'clock) — Dark Gray, 3px stroke
    draw.line([(166, 0), (161, 9)], fill=COLOR_GRAY, width=3)
    draw.line([(200, 56), (191, 61)], fill=COLOR_GRAY, width=3)
    draw.line([(200, 172), (191, 167)], fill=COLOR_GRAY, width=3)
    draw.line([(166, 228), (161, 219)], fill=COLOR_GRAY, width=3)
    draw.line([(34, 228), (39, 219)], fill=COLOR_GRAY, width=3)
    draw.line([(0, 172), (9, 167)], fill=COLOR_GRAY, width=3)
    draw.line([(0, 56), (9, 61)], fill=COLOR_GRAY, width=3)
    draw.line([(34, 0), (39, 9)], fill=COLOR_GRAY, width=3)

    # 2. Top status: Delta & Age (Black text)
    draw.text((10, 8), delta_str, font=FONT_22, fill=COLOR_BLACK)
    draw.text((135, 8), age_str, font=FONT_22, fill=COLOR_BLACK)

    # 3. Main BG Text (X=25, Y=36, Islamic Green / Orange / Red tint)
    draw.text((25, 36), bg_val, font=FONT_48, fill=color_rgb)

    # Strikethrough Line if no data / stale
    if show_strike:
        draw.line([(25, 62), (120, 62)], fill=color_rgb, width=3)

    # 4. Arrow Icon (Palette Swapped)
    arrow_path = ARROW_MAP.get(arrow_type, ARROW_MAP["flat"])
    if os.path.exists(arrow_path):
        arrow_src = Image.open(arrow_path).convert("RGBA")
        arr_data = arrow_src.get_flattened_data() if hasattr(arrow_src, 'get_flattened_data') else arrow_src.getdata()
        new_data = []
        for r, g, b, a in arr_data:
            if a > 50 and r > 150:
                new_data.append((color_rgb[0], color_rgb[1], color_rgb[2], a))
            else:
                new_data.append((255, 255, 255, 0))
        arrow_recolored = Image.new("RGBA", arrow_src.size)
        arrow_recolored.putdata(new_data)
        img.paste(arrow_recolored, (130, 42), mask=arrow_recolored)

    # 5. Status Columns (Row 1: IOB & COB)
    draw.text((5, 88), iob_str, font=FONT_32, fill=COLOR_BLACK)
    draw.text((125, 88), cob_str, font=FONT_32, fill=COLOR_BLACK)

    # 6. Status Columns (Row 2: Basal Rate & Date)
    draw.text((5, 122), basal_str, font=FONT_22, fill=COLOR_GRAY)
    draw.text((125, 122), date_str, font=FONT_22, fill=COLOR_GRAY)

    # 7. Status Columns (Row 3: Detailed IOB Split)
    draw.text((5, 150), detail_str, font=FONT_22, fill=COLOR_GRAY)

    # 8. High Target Dashed Line (Y=190, 3px gray dashed line)
    for x in range(0, 200, 6):
        draw.line([(x, 190), (min(x+3, 200), 190)], fill=COLOR_GRAY, width=1)

    # 9. Low Target Dashed Line (Y=212, 3px red dashed line)
    for x in range(0, 200, 6):
        draw.line([(x, 212), (min(x+3, 200), 212)], fill=COLOR_RED, width=1)

    # 10. Glucose History Curve (36 points mapped between Y=180 and Y=220)
    history_pts = [
        110, 112, 114, 116, 118, 120, 122, 124, 126, 124, 122, 120,
        118, 116, 118, 120, 122, 124, 126, 128, 126, 124, 122, 120,
        118, 120, 122, 124, 122, 120, 118, 120, 122, 120, 120, 120
    ]
    graph_coords = []
    for i, bg in enumerate(history_pts):
        gx = int(5 + i * (190.0 / 35.0))
        # Map BG range 40..250 to Y 220..175
        gy = int(220 - ((bg - 40) / 210.0) * 45)
        graph_coords.append((gx, gy))

    # Connect dots with continuous 2px line
    for i in range(len(graph_coords) - 1):
        draw.line([graph_coords[i], graph_coords[i+1]], fill=COLOR_GREEN, width=2)
    for gx, gy in graph_coords:
        draw.rectangle([gx-2, gy-2, gx+2, gy+2], fill=COLOR_GREEN)

    # 11. Sweeping Analog Clock Hands (20% longer hands)
    cx, cy = 100, 114
    # Hour hand (10 o'clock -> 300 deg) — 36px length
    h_angle = math.radians(300)
    hx = cx + int(36 * math.sin(h_angle))
    hy = cy - int(36 * math.cos(h_angle))
    draw.line([(cx, cy), (hx, hy)], fill=COLOR_BLACK, width=6)

    # Minute hand (2 o'clock -> 60 deg) — 58px length
    m_angle = math.radians(60)
    mx = cx + int(58 * math.sin(m_angle))
    my = cy - int(58 * math.cos(m_angle))
    draw.line([(cx, cy), (mx, my)], fill=COLOR_BLACK, width=5)

    # Hand center hub
    draw.ellipse([cx-6, cy-6, cx+6, cy+6], fill=COLOR_WHITE, outline=COLOR_BLACK, width=2)

    return img

def main():
    states = [
        ("in_range", "120", "(+3|+5)", "0'", COLOR_GREEN, "flat", "0.32 U", "15g", "0.90", "(0.02|0.31)", "16 Aug", False),
        ("high", "250", "(+12|+15)", "1'", COLOR_ORANGE, "flat", "1.45 U", "45g", "1.20", "(0.80|0.65)", "16 Aug", False),
        ("low", "55", "(-8|-5)", "2'", COLOR_RED, "flat", "0.00 U", "0g", "0.00", "(0.00|0.00)", "16 Aug", False),
        ("arrow_forty_five_up", "150", "(+5|+7)", "0'", COLOR_GREEN, "forty_five_up", "0.50 U", "20g", "0.90", "(0.10|0.40)", "16 Aug", False),
        ("arrow_forty_five_down", "100", "(-4|-2)", "1'", COLOR_GREEN, "forty_five_down", "0.20 U", "0g", "0.90", "(0.00|0.20)", "16 Aug", False),
        ("double_up", "220", "(+18|+22)", "0'", COLOR_ORANGE, "double_up", "2.10 U", "60g", "1.50", "(1.20|0.90)", "16 Aug", False),
        ("double_down", "45", "(-22|-18)", "1'", COLOR_RED, "double_down", "0.00 U", "0g", "0.00", "(0.00|0.00)", "16 Aug", False),
        ("stale", "---", "--", "18'", COLOR_GRAY, "flat", "0.00 U", "0g", "0.90", "(0.00|0.00)", "16 Aug", True),
    ]

    print("Generating complete Light Mode Emery watchface screenshots with ALL 13 data fields...")
    for name, bg, delta, age, col, arr, iob, cob, basal, detail, date_str, strike in states:
        img = render_watchface(bg, delta, age, col, arr, iob, cob, basal, detail, date_str, strike)
        out_path = os.path.join(SCREENSHOT_DIR, f"{name}.png")
        img.save(out_path)
        print(f"Saved {name}.png ({os.path.getsize(out_path)} bytes)")

    # Also save primary store screenshot to resources/screenshots/emery_screenshot.png
    store_img = render_watchface("120", "(+3|+5)", "0'", COLOR_GREEN, "flat", "0.32 U", "15g", "0.90", "(0.02|0.31)", "16 Aug", False)
    store_path = os.path.join(RESOURCE_SCREENSHOT_DIR, "emery_screenshot.png")
    store_img.save(store_path)
    print(f"Saved store screenshot to {store_path} ({os.path.getsize(store_path)} bytes)")

if __name__ == "__main__":
    main()
