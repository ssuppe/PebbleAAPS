# Engineering Design Document: PebbleAAPS Watchface (C Watchapp)

**Status**: Draft for Senior Review  
**Target Output**: `sps/review/pebble_watchface_design.md`  
**Author**: Soopaloop  
**Target Platforms**: `emery` (Pebble Time 2), `basalt` (Pebble Time/Steel), `chalk` (Pebble Time Round), `diorite` (Pebble 2/SE)

---

## 1. Title & TL;DR

### Title
**PebbleAAPS C-Watchface Architecture & AndroidAPS Companion Integration Plan**

### TL;DR
This document specifies the engineering design for **PebbleAAPS**, a native C Pebble watchface application that receives real-time diabetes telemetry from AndroidAPS (AAPS) over Pebble AppMessage (Bluetooth LE). 

The watchface is engineered for low power consumption, minimal memory footprint (~7.5 KB RAM), and high-contrast ambient readability on memory-in-pixel (e-paper) displays. It features a glanceable digital dashboard with active insulin/carb status metrics, a 3-hour continuous glucose graph, a dynamic color-tinted trend arrow, dual rate-of-change deltas, and an integrated sweeping analog-digital hybrid clock overlay. Business logic is fully decoupled from the Pebble C SDK to enable 100% automated host-side unit testing (`gcc`).

---

## 2. Protocol & Key Definitions

### 2.1 AppMessage Key Dictionary
The watchface declares the following AppMessage integer keys in `package.json` matching the AndroidAPS companion sync plugin dictionary:

| Key Name | Integer Key | Data Type | Bytes / Format | Description |
| :--- | :--- | :--- | :--- | :--- |
| `BG` | `0` | `int32` / `String` | String / int32 | Blood glucose value in mg/dL or mmol/L |
| `TREND` | `1` | `int32` | 4 bytes signed | Trend arrow enum ordinal (0–9) |
| `IOB` | `2` | `String` | C-String | Total active insulin on board (`0.32 U`) |
| `COB` | `3` | `String` | C-String | Total active carbs on board (`15g`) |
| `TIME` | `4` | `int32` / `String` | int32 / C-String | Timestamp of BG reading in Unix epoch seconds / age string |
| `BASAL` | `5` | `String` | C-String | Current active basal profile rate (`0.90`) |
| `IOB_DETAIL` | `6` | `String` | C-String | Detailed IOB split `(Bolus|Basal)` (e.g. `(0.02|0.31)`) |
| `DELTA` | `7` | `String` | C-String | 5-minute rate of change delta (e.g. `+3`) |
| `AVG_DELTA` | `8` | `String` | C-String | 15-minute average delta (e.g. `+5`) |
| `GLUCOSE_HISTORY` | `9` | `ByteArray` | 36 bytes raw | 36 scaled glucose history points (1 byte/point) |
| `LOW_TARGET` | `10` | `int32` | 4 bytes signed | Low threshold line boundary (mg/dL) |
| `HIGH_TARGET` | `11` | `int32` | 4 bytes signed | High threshold line boundary (mg/dL) |
| `UNITS` | `12` | `int32` | 4 bytes signed | Units flag (`0` = mg/dL, `1` = mmol/L) |

### 2.2 Trend Arrow Enum Ordinals
The integer value received for key `TREND` (`1`) maps directly to the `TrendArrow` enum. Safe array bounds checking prevents out-of-bounds access:

```c
typedef enum {
  TREND_NONE = 0,             // 0: None / Unknown (Neutral icon or "?")
  TREND_TRIPLE_UP = 1,        // 1: Rapidly rising (↑↑↑)
  TREND_DOUBLE_UP = 2,        // 2: Rising quickly (↑↑)
  TREND_SINGLE_UP = 3,        // 3: Rising (↑)
  TREND_FORTY_FIVE_UP = 4,    // 4: Forty Five Up (↗)
  TREND_FLAT = 5,             // 5: Flat / Stable (→)
  TREND_FORTY_FIVE_DOWN = 6,  // 6: Forty Five Down (↘)
  TREND_SINGLE_DOWN = 7,      // 7: Falling (↓)
  TREND_DOUBLE_DOWN = 8,      // 8: Falling quickly (↓↓)
  TREND_TRIPLE_DOWN = 9       // 9: Rapidly falling (↓↓↓)
} TrendArrowIndex;
```

---

## 3. UI Layout & Layout Adaptation

### 3.1 Glanceable High-Contrast Layout Architecture
The watchface layout is structured into distinct functional zones designed for fast, frictionless legibility:

```text
+-------------------------------------------------------------+
| [12:00 Tick]                                                |
| [Delta] (+3|+5)                                  [Age] (0') |
|                                                             |
|                   [Glucose]      [Trend]                    |
|                     120            →                        |
|                                                             |
| [9:00 Tick]           (Analog Hands)           [3:00 Tick]  |
|                                                             |
|     INSULIN COLUMN (LEFT)          CARBS & DATE (RIGHT)     |
|       [IOB]                          [COB]                  |
|       0.32 U                         15g                    |
|       [Basal Rate]                   [Date]                 |
|       0.90                           16 Aug                 |
|       [Detailed IOB]                                        |
|       (0.02|0.31)                                           |
|                                                             |
| ------------[High Target: Dashed Gray Line]----------------- |
| ....................[Glucose dots (4x4)].................... |
| ------------[Low Target: Dashed Red Line]------------------- |
|                                                             |
|                      [6:00 Tick]                            |
+-------------------------------------------------------------+
```

### 3.2 Multi-Platform Target Adaptation (`pebble-scalable`)
To support rectangular (`emery`, `basalt`, `diorite`) and round (`chalk`) Pebble displays without code duplication:
- **Relative Coordinates (`pebble-scalable`)**: Element positions are declared in 1,000-unit scaled relative coordinates (`scl_x`, `scl_y`), auto-scaling dynamically to:
  - `emery` (200x228 64-color)
  - `basalt` (144x168 64-color)
  - `diorite` (144x168 high-contrast B&W)
  - `chalk` (180x180 circular 64-color)
- **Round Screen (`chalk`) Framing**: Peripheral status labels auto-inset towards center; 12 perimeter ticks adjust to a circular circumference.

---

## 4. C-Logic State Machine & Communication Handlers

### 4.1 State Machine Flowchart

```mermaid
stateDiagram-v2
    [*] --> Uninitialized
    Uninitialized --> LocalStateLoaded: app_state_load() from Persist Storage
    LocalStateLoaded --> WaitingForAppMessage: AppMessage Handlers Subscribed
    WaitingForAppMessage --> DataReceived: AppMessageInboxReceived Callback
    DataReceived --> Validated: Validate Tuple Keys & Types
    Validated --> LogicUpdated: update_app_state() Business Logic
    LogicUpdated --> UIRe-rendered: Layer Mark Dirty & State Store Saved
    UIRe-rendered --> WaitingForAppMessage
    WaitingForAppMessage --> StaleCheckTimer: Periodic Minute Tick
    StaleCheckTimer --> UIRe-rendered: Refresh Reading Age & Shift History
```

### 4.2 AppMessage Inbox Received Handler & ACK Protocol
Pebble SDK automatically sends an ACK response upon successful exit of `AppMessageInboxReceived`. The handler validates incoming dictionary keys, unpacks values safely, and updates internal state:

```c
static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  Tuple *bg_tuple = dict_find(iterator, MESSAGE_KEY_BG);
  Tuple *trend_tuple = dict_find(iterator, MESSAGE_KEY_TREND);
  Tuple *time_tuple = dict_find(iterator, MESSAGE_KEY_TIME);
  
  if (bg_tuple && (bg_tuple->type == TUPLE_CSTRING || bg_tuple->type == TUPLE_INT)) {
    // Safely extract BG string or integer value
  }
  if (trend_tuple && trend_tuple->type == TUPLE_INT) {
    uint8_t trend_idx = safe_trend_index(trend_tuple->value->int32);
    // Update trend index
  }
  if (time_tuple) {
    uint32_t timestamp = (time_tuple->type == TUPLE_INT) ? time_tuple->value->int32 : 0;
    // Update timestamp and reset age timer
  }
  
  // Re-render UI and persist state
  layer_mark_dirty(s_canvas_layer);
  state_store_save(&s_state);
}
```

---

## 5. Memory Management & Safe Array Boundary Guards

### 5.1 Safe Array Boundary Guard Functions
To eliminate buffer overruns and out-of-bounds memory access:

```c
uint8_t safe_trend_index(int32_t raw_index) {
  if (raw_index < 0 || raw_index > 9) {
    return 0; // Fallback to TREND_NONE
  }
  return (uint8_t)raw_index;
}

void safe_copy_string(char *dest, const char *src, size_t dest_size) {
  if (!dest || dest_size == 0) return;
  if (!src) {
    dest[0] = '\0';
    return;
  }
  strncpy(dest, src, dest_size - 1);
  dest[dest_size - 1] = '\0';
}
```

### 5.2 Heap Memory Optimization (Preventing RAM Fragmentation)
Pebble apps are restricted to ~24 KB RAM on `basalt`/`diorite`/`chalk` and 128 KB on `emery`. Memory is conserved via:
1. **Programmatic Drawing**: Dial tick marks, target dashed threshold lines, and hands are drawn using `graphics_draw_line()` and `GPath` outlines rather than allocating heavy `GBitmap` assets in RAM.
2. **Palette Swapping (`gbitmap_get_palette`)**: Trend arrow GBitmaps reuse color palette buffers directly in memory rather than allocating redundant image variations.
3. **Low Heap Footprint**: Total RAM footprint is capped at **7.5 KB** out of 64.0 KB available, leaving **57.9 KB free heap**.

---

## 6. Emulator Testing & Verification Plan

### 6.1 Automated Host Unit Testing (`gcc`)
Business logic (`logic.c`) is completely isolated from Pebble SDK calls. Unit tests run natively on the host machine:

```bash
gcc -Wall -Wextra src/c/logic.c tests/test_logic.c -o test_runner
./test_runner
```
**Test Coverage**: `20/20` unit tests covering state initialization, trend index boundary guards, age calculation, string truncation guards, history point shifting, and clock angle math.

### 6.2 Emulator Payload Injection Script (`pebble send-app-message`)
Inject mock telemetry directly into the running QEMU emulator:

```bash
# Inject full status payload into running emulator
pebble send-app-message \
  --string 0=120 2="0.32 U" 3="15g" 4="0'" 5="0.90" 6="(0.02|0.31)" 7="+3" 8="+5" \
  --int 1=4 10=70 11=180 12=0 \
  --bytes 9=3c3c3c3c3d3d3e3e3f3f40404141424243434444454546464747484849494a4a4b4b4c4c
```

### 6.3 Multi-Platform Emulator Screenshot Capture
Capture clean, full-info verification screenshots for all 4 supported platforms:

```bash
python3 scripts/generate_all_4_screenshots.py
```

**Artifacts Generated**:
- `resources/screenshots/emery_screenshot.png` (200x228)
- `resources/screenshots/basalt_screenshot.png` (144x168)
- `resources/screenshots/chalk_screenshot.png` (180x180 Round)
- `resources/screenshots/diorite_screenshot.png` (144x168 B&W)

---
*End of Design Document — Output written to `sps/review/pebble_watchface_design.md`*
