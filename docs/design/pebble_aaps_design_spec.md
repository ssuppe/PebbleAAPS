# PebbleAAPS Watchface Design Specification

This document details the design specifications, coordinate layout, and design rationale for the high-legibility **PebbleAAPS** watchface for the Pebble Time 2 (Emery, 200x228px).

---

## 1. Design Goal

The core objective is to create a **highly legible, glanceable, and trustworthy dashboard** for diabetes management on retro e-paper hardware. It replaces dense, low-contrast WearOS-style layouts with a high-contrast, structurally symmetric design optimized for physical e-paper (MIP) screens under all lighting conditions.

---

## 2. Visual Layout Blueprint

```text
+-------------------------------------------------------------+
| [12:00 Tick]                                                |
| [Delta] (+3|+5)                                  [Age] (3') |
| (18px White)                                   (18px White) |
|                                                             |
|                   [Glucose]      [Trend]                    |
|                     6.2            →                        |
|                 (38px LECO)    (20x20 Green)                |
|                                                             |
| [9:00 Tick]           (Analog Center)           [3:00 Tick] |
|                                                             |
|     INSULIN COLUMN (LEFT)          CARBS & DATE (RIGHT)     |
|                                                             |
|       [IOB]                          [COB]                  |
|       0.32 U                         0g                     |
|    (24px BoldPixels)              (24px BoldPixels)         |
|                                                             |
|       [IOB Detail]                   [Date]                 |
|       (0.02|0.31)                    9 Aug                  |
|    (18px Regular)                 (18px Regular)            |
|                                                             |
|       [Basal Rate]                                          |
|       0.90                                                  |
|    (18px Regular)                                           |
|                                                             |
| ------------[High Target: 170px (Dashed Gray)]------------- |
| ....................[Glucose dots (4x4)].................... |
| ------------[Low Target:  195px (Dashed Red)]-------------- |
|                                                             |
|                      [6:00 Tick]                            |
+-------------------------------------------------------------+
```

---

## 3. Position and Coordinate Table

| Layer ID | Name | Type | X | Y | Width | Height | Font / Size | Color | Description |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `bg_layer` | **BG** | Text | 5 | 36 | 110 | 52 | `Lilita One 48` (scl font 2) | `#00aa00` (Islamic Green) | Blood glucose reading value. Left-aligned at X=5px (scl_x(25)) matching the info column below it. |
| `arrow_layer` | **Arrow** | Bitmap | Dynamic | 42 | 36 | 36 | N/A | Dynamic (Trend-based) | Dilated 36x36px trend arrow, dynamically positioned after the BG text with a 5px gap. |
| `delta_layer` | **Delta** | Text | 10 | 10 | 125 | 27 | `Gothic 18 Bold` (scl font 4) | `#000000` (Black) | Rate of change trend `(+3\|+5)`. |
| `age_layer` | **Age** | Text | 130 | 10 | 60 | 27 | `Gothic 18 Bold` (scl font 4) | `#000000` (Black) | Reading sync age `(3')`. |
| `iob_layer` | **IOB** | Text | 5 | 88 | 115 | 40 | `Lilita One 32` (scl font 1) | `#000000` (Black) | Total active insulin on board. |
| `right_iob_detail` | **IOB Detail** | Text | 5 | 160 | 190 | 30 | `Lilita One 22` (scl font 3) | `#555555` (Gray) | Detailed bolus/basal split (full width row 3). |
| `left_val_top` | **Basal Rate** | Text | 5 | 128 | 115 | 32 | `Lilita One 22` (scl font 3) | `#555555` (Gray) | Active pump basal profile rate (row 2). |
| `cob_layer` | **COB** | Text | 125 | 88 | 70 | 40 | `Lilita One 32` (scl font 1) | `#000000` (Black) | Active carbs on board. |
| `date_layer` | **Date** | Text | 125 | 128 | 70 | 32 | `Lilita One 22` (scl font 3) | `#555555` (Gray) | Calendar date. |
| `graph_high_target` | **High Target** | Custom | 0 | 190 | 200 | 1 | N/A | `#555555` (Gray) | Programmatic 3px dashed line. |
| `graph_low_target` | **Low Target** | Custom | 0 | 212 | 200 | 1 | N/A | `#aa0000` (Red) | Programmatic 3px dashed line. |
| `gpath_hour_hand` | **Hour Hand** | GPath | 100 | 114 | 32 | 45 | N/A | `#ffffff` (White) | 5px thick analog hour hand. |
| `gpath_minute_hand` | **Minute Hand** | GPath | 100 | 114 | 20 | 85 | N/A | `#ffffff` (White) | 3px thick analog minute hand. |

---

## 4. Key Design Choices & Rationale

### 1. Glanceability over High-Density Noise
We dropped the **theoretical prediction rates** (Insulin Activity and Blood Glucose Impact (BGI)) in favor of keeping strictly **empirical, concrete data** on the screen. Removing them cleared up visual real estate, allowing us to bump secondary font sizes up from an unreadable 14px to a clear 18px.

### 2. Elimination of Text Labels
Instead of printing cluttering labels like `"Carb"`, `"IOB"`, or `"Date"`, we rely on screen position and units (`U` and `g`) to tell them apart:
* **The Left Column** is dedicated entirely to **Insulin** (IOB, Detailed split, Basal rate).
* **The Right Column** is dedicated entirely to **Intake and System state** (COB, Calendar Date).

### 3. Maximum Contrast for E-Paper Hardware (High-Contrast Light Mode)
Pebble's memory-in-pixel (MIP) reflective screens suffer from reduced contrast under indoor or ambient lighting. To counter this:
* We configured **High-Contrast Light Mode** (`window_set_background_color(s_main_window, GColorWhite)`). Hands, ticks, date, and status headers are rendered in dark high-contrast shades (`GColorBlack`, `GColorCobaltBlue`, `GColorDarkGray`).
* The **Blood Glucose Reading** and **Trend Arrow** use dynamic color-coded state tinting (`BgColorState`): `GColorIslamicGreen` (In Range), `GColorOrange` (High), `GColorBulgarianRose` (Low), and `GColorDarkGray` with a 3px horizontal strikethrough layer (No Data / Stale $\ge$ 15m).
* All 7 trend arrow icons (`arrow_flat.png`, `arrow_single_up.png`, `arrow_single_down.png`, `arrow_forty_five_up.png`, `arrow_forty_five_down.png`, `arrow_double_up.png`, `arrow_double_down.png`) are 36×36 1-bit crisp binary PNGs (solid white `#FFFFFF` shape fill on 100% transparent background).
* At runtime, `gbitmap_get_palette(s_arrow_bitmap)[1] = target_color` dynamically color-tints the solid arrow shape in memory without requiring separate color image files on disk.

### 4. 1-Dimensional Dashed Lines (Dashed Targets)
Rather than solid bright bars, the graph target ranges are rendered using pixel-perfect repeating **`3px color / 3px transparent` dashed lines** (`GColorDarkGray` for High Target, `GColorBulgarianRose` for Low Target). This visually dims the lines so they act as a secondary grid background, preventing them from competing with the watch hands sweeping on top.

### 5. Bold Glucose Trend Curve
Because scattered 2x2 dots look like visual noise and are hard to see on a watch, we connected the dots with a **continuous `2px` green line** and increased the point sizes to **`4x4` pixels** (`GColorIslamicGreen` in-range, `GColorOrange` high, `GColorBulgarianRose` low), making the shape of your glucose curve instantly recognizable.

---

## 5. Memory & Resource Optimization Rationale

To maintain the high-visibility, visually premium typography of the Lilita One typeface without causing heap memory exhaustion or long Bluetooth transmission delays, we employ several design-level optimizations:

### 1. Programmatic Layouts over Static Assets
Dashed targets, dial ticks, and the stale strikethrough line are drawn dynamically in update callbacks using basic drawing primitives (`graphics_draw_pixel` and `graphics_draw_line`), avoiding static background image assets.

### 2. On-Demand Bitmap Swapping & Palette Index Tinting
Instead of caching all trend direction arrows in memory concurrently, only the active trend arrow bitmap is loaded when a data update is received. The old bitmap is immediately freed back to the heap. Runtime palette tinting (`palette[1] = target_color`) dynamically changes arrow color without re-allocating assets.

### 3. Glyph Character-Set Pruning (`characterRegex`)
TrueType font compilation is strictly restricted in `package.json` to only compile character ranges that are actually displayed on the UI. The large 48px font only generates numbers and punctuation, while letters are only generated for the smaller 22px/32px fonts. This keeps the bundle size at ~24KB and slashes active font memory usage by over 80% (~3.8 KB runtime heap RAM).
