# PebbleAAPS Architecture & Technical Design

This document details the software architecture, design patterns, module separation, and optimization techniques used in **PebbleAAPS**.

---

## 🏛️ 1. Architecture Overview

PebbleAAPS is engineered with a strict **decoupled 3-tier architecture** that separates business logic from the Pebble SDK UI runtime. This allows 100% of state calculations, unit formatting, boundary checks, and history array management to be compiled and unit-tested on host machines (`gcc`) without needing QEMU emulator or watch hardware.

```text
+-------------------------------------------------------------------+
|                        Pebble SDK Runtime                         |
|                     (pebble_watchface.c)                          |
|  - Window, TextLayers, BitmapLayers, GPaths                       |
|  - AppMessage Inbox Callback                                      |
|  - TickTimerService (Clock Hands & Stale Checks)                  |
+-------------------------------------------------------------------+
                                  |
                                  v
+-------------------------------------------------------------------+
|                        State Store Module                         |
|                          (state_store.c)                          |
|  - Pebble Persistent Storage API (persist_read / persist_write)   |
|  - Boot-up restore & AppMessage save                              |
+-------------------------------------------------------------------+
                                  |
                                  v
+-------------------------------------------------------------------+
|                      Decoupled Business Logic                     |
|                      (logic.c / logic.h)                          |
|  - AAPSState Struct                                               |
|  - update_aaps_state(), update_aaps_status()                      |
|  - format_bg_string(), format_age_string()                        |
|  - shift_history_left(), add_to_history()                         |
|  - calculate_graph_y(), get_bg_color_state()                      |
+-------------------------------------------------------------------+
                                  |
                                  v
+-------------------------------------------------------------------+
|                       Host Unit Test Suite                        |
|                      (tests/test_logic.c)                         |
|  - Pure C host runner executed via `gcc`                          |
|  - 20+ automated tests covering edge cases & guards               |
+-------------------------------------------------------------------+
```

---

## 📦 2. Module Breakdown

### `src/c/logic.h` & `src/c/logic.c` (Pure Business Logic)
- **Zero Pebble SDK Dependencies**: Uses standard C headers (`stdint.h`, `stdbool.h`, `stdio.h`, `string.h`, `time.h`).
- **`AAPSState` Struct**: Central data model storing BG reading, trend code, timestamps, delta strings, active insulin (IOB), active carbs (COB), basal rate, history array, target thresholds, and display units.
- **Key Functions**:
  - `update_aaps_state(...)`: Safe state transition on receiving new readings.
  - `update_aaps_status(...)`: Safe string copy with truncation protection.
  - `format_bg_string(...)`: Formats mg/dL or mmol/L string representations.
  - `format_age_string(...)`: Computes relative reading age in minutes (`0'`, `5'`, etc.).
  - `get_bg_color_state(...)`: Evaluates target boundaries to return `BG_COLOR_IN_RANGE`, `BG_COLOR_HIGH`, `BG_COLOR_LOW`, or `BG_COLOR_NO_DATA`.
  - `calculate_graph_y(...)`: Maps raw BG values to 0–228 Y pixel coordinates inside graph bounds.

### `src/c/state_store.h` & `src/c/state_store.c` (Persistence)
- Encapsulates Pebble SDK `persist_read_data()` and `persist_write_data()`.
- Automatically reloads last known status upon watchface startup, ensuring zero data loss on app restarts.

### `src/c/pebble_watchface.c` (UI Shell & SDK Event Loop)
- **Responsive Scaling**: Utilizes `pebble-scalable` macros (`scl_x()`, `scl_y()`, `scl_font()`) to automatically adapt layout coordinates across Emery (200x228), Basalt (144x168), Chalk (180x180), and Diorite (144x168 B&W).
- **AppMessage Inbox Handling**: Parses incoming dictionary tuples and invokes `logic.c` updaters.
- **Dynamic Render Cycle**:
  - Updates text layer strings.
  - Swaps palette colors for trend arrow.
  - Redraws graph dots and target lines.
  - Sweeps analog clock hands on every minute tick.

---

## 🎨 3. Key Technical Optimizations

### 1. In-Memory Palette Swapping (`gbitmap_get_palette`)
To support dynamic color tinting (Green/Orange/Red/Gray) for trend arrows without shipping 28 separate color image files:
- All arrow assets are stored as single 1-bit crisp white-on-transparent PNG files.
- At runtime, `pebble_watchface.c` modifies color palette index 1:
  ```c
  GColor *palette = gbitmap_get_palette(s_arrow_bitmap);
  palette[1] = target_color;
  ```
- Reduces app RAM & storage footprint significantly.

### 2. Programmatic Drawing
- Target threshold lines (High/Low) are rendered programmatically using dashed `graphics_draw_line` operations.
- Peripheral tick marks and analog hands use vector `GPath` drawing, avoiding bitmap layer allocation overhead.

---

## 🧪 4. Testing Strategy (TDD)

Every business logic modification MUST be validated against the host test suite before committing:
```bash
gcc -Wall -Wextra src/c/logic.c tests/test_logic.c -o test_runner && ./test_runner && rm test_runner
```
The test suite validates:
- Null pointer and boundary safety.
- Stale data timeouts ($\ge$ 15 minutes).
- Division-by-zero guards on target line calculation.
- Array history overflow handling.
