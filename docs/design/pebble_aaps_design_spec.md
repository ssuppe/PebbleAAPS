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
| `bg_layer` | **BG** | Text | 60 | 35 | 80 | 40 | `LECO 1976` (38px Bold) | `#00ff00` (Green) | Blood glucose reading value. |
| `arrow_layer` | **Arrow** | Bitmap | 134 | 45 | 20 | 20 | N/A | `#00ff00` (Green) | Aligned vertically with BG text center. |
| `delta_layer` | **Delta** | Text | 10 | 10 | 75 | 24 | `Medodica` (18px Regular) | `#ffffff` (White) | Rate of change trend `(+3\|+5)`. |
| `age_layer` | **Age** | Text | 145 | 10 | 45 | 24 | `Medodica` (18px Regular) | `#ffffff` (White) | Reading sync age `(3')`. |
| `iob_layer` | **IOB** | Text | 10 | 92 | 80 | 28 | `BoldPixels` (24px Bold) | `#ffffff` (White) | Total active insulin on board. |
| `right_iob_detail` | **IOB Detail** | Text | 10 | 120 | 80 | 24 | `Medodica` (18px Regular) | `#ffffff` (White) | Detailed bolus/basal split. |
| `left_val_top` | **Basal Rate** | Text | 10 | 144 | 80 | 24 | `Medodica` (18px Regular) | `#ffffff` (White) | Active pump basal profile rate. |
| `cob_layer` | **COB** | Text | 115 | 92 | 75 | 28 | `BoldPixels` (24px Bold) | `#ffffff` (White) | Active carbs on board. |
| `date_layer` | **Date** | Text | 115 | 120 | 75 | 24 | `Medodica` (18px Regular) | `#ffffff` (White) | Calendar date. |
| `graph_high_target` | **High Target** | Bitmap | 0 | 170 | 200 | 1 | N/A | `#555555` (Gray) | Dashed 3px target boundary. |
| `graph_low_target` | **Low Target** | Bitmap | 0 | 195 | 200 | 1 | N/A | `#aa0000` (Red) | Dashed 3px target boundary. |
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

### 3. Maximum Contrast for E-Paper Hardware
Pebble's memory-in-pixel (MIP) reflective screens suffer from reduced contrast in low light. To counter this:
* We converted all text elements from gray (`#aaaaaa`) to **solid white (`#ffffff`)**. Hierarchy is now established cleanly through font size and positioning, while ensuring every letter pops under any lighting conditions.
* The trend arrow matches the **glucose green (`#00ff00`)** and is aligned with the vertical text center of the BG value.

### 4. 1-Dimensional Dithered Lines (Dashed Targets)
Rather than solid bright bars, the graph target ranges are rendered using pixel-perfect repeating **`3px color / 3px transparent` dashed lines**. This visually dims the lines so they act as a secondary grid background, preventing them from competing with the white watch hands sweeping on top.

### 5. Bold Glucose Trend Curve
Because scattered 2x2 dots look like visual noise and are hard to see on a watch, we connected the dots with a **continuous `2px` line** and increased the point sizes to **`4x4` pixels**, making the shape of your glucose curve instantly recognizable.
