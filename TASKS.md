# Pebble Watchface MVP (Pebble Time 2) Implementation Tasks

This file outlines the step-by-step roadmap for building the AndroidAPS Pebble Time 2 Watchface. This project enforces a strict **TDD (Red/Green/Refactor)** workflow, where core logic is decoupled from the Pebble SDK and tested on the host machine using standard C unit testing.

---

## Task 1: Pebble Build Environment Setup
- [x] **1.1: Install Host Prerequisites**
  - Install dependencies: `libsdl2-2.0-0`, `libglib2.0-0`, `libpixman-1-0`, `zlib1g`, `libsndio7.0`, `nodejs`, `npm`.
  - **Verification:** Run `node --version` and `npm --version`.
- [x] **1.2: Install Python 3 `pebble-tool`**
  - Install Python package manager `uv` (if not installed).
  - Run `uv tool install pebble-tool --python 3.13` (or target python >= 3.10).
  - **Verification:** Run `pebble --version`.
- [x] **1.3: Download Pebble SDK**
  - Execute `pebble sdk install latest`.
  - **Verification:** Run `pebble sdk list` to confirm `latest` is active and targets `emery`.

---

## Task 2: Core Watchface Logic (TDD Implementation)
- [x] **2.1: Establish Host Test Harness (Red Phase)**
  - Create directory layout: `src/` and `tests/`.
  - Write test file `tests/test_logic.c` asserting the functionality of AAPS state initialization, state updating, trend arrow boundary guarding (0–9), reading age string formatting (`Nm ago`, `No Data`, `>999m ago`), and stale color state triggering (stale when age >= 15 minutes).
  - Create a temporary stub header `src/logic.h` and empty functions in `src/logic.c` to compile.
  - **Verification:** Compile and run `gcc -Wall src/logic.c tests/test_logic.c -o test_runner && ./test_runner`. Verify it compiles but assertions fail (RED).
- [x] **2.2: Implement Core Logic (Green Phase)**
  - Complete the implementation of functions in `src/logic.h` and `src/logic.c`.
  - **Verification:** Re-compile and run host tests. Verify `./test_runner` outputs `All host tests passed successfully!` with no failures (GREEN).

---

## Task 3: Pebble UI & AppMessage Integration
- [x] **3.1: Define App Metadata & Mapping**
  - Create `package.json` specifying the UUID `54D3008F-E144-4712-B201-24BC515C40BA`, targeted platform `emery`, and appKeys: `BG` (0), `TREND` (1), `TIME` (4).
  - **Verification:** Check `package.json` syntax and confirm UUID match with companion app settings.
- [x] **3.2: Implement Pebble UI Shell & Lifecycle**
  - Create `src/pebble_watchface.c` implementing:
    - Layout coordinates matching the Pebble Time 2 (200x228 px rectangular screen).
    - Subscribing to Pebble tick service (`tick_timer_service_subscribe` on `MINUTE_UNIT`).
    - Initializing TextLayers for time, BG value, and reading age, plus a BitmapLayer for the trend arrow.
    - Implementing the window load, unload, init, and deinit callbacks (clean memory destruction of layers and bitmaps).
  - **Verification:** Run `pebble build` to verify compiling with mock resources.
- [x] **3.3: AppMessage Registration & Dynamic Redrawing**
  - Implement `app_message_open(64, 64)` buffer size configuration.
  - Implement the inbox received callback parsing `BG`, `TREND`, and `TIME` via `dict_find` and type validation, passing values to `update_aaps_state`.
  - Hook logic functions to UI redraws (dynamic color loading of the trend arrow bitmap, changing text color of the age layer to `GColorRed` if state is `COLOR_STATE_STALE`).
  - **Verification:** Run `pebble build` successfully.

---

## Task 4: Emulator Verification & Edge Case Handling
- [x] **4.1: Deploy to Emery Emulator**
  - Launch the Pebble Time 2 emulator: `pebble install --emulator emery`.
  - **Verification:** Ensure the watchface loads, displays the current system time correctly, and shows `No Data` with a neutral icon initially.
- [x] **4.2: Verify Data Ingestion (Normal Case)**
  - Send normal reading: `pebble appmessage send 0:int32:115 1:int32:5 4:int32:$(date +%s)`.
  - **Verification:** Observe BG is `115`, Trend Arrow displays Flat (`→`), Age displays `0m ago` in white text.
- [x] **4.3: Verify Stale Reading Alert**
  - Send stale reading: `pebble appmessage send 0:int32:140 1:int32:3 4:int32:$(( $(date +%s) - 1200 ))` (20 minutes ago).
  - **Verification:** Observe Age displays `20m ago` in red text.
- [x] **4.4: Verify Safety Guards (Boundary Test)**
  - Send invalid trend indices: `pebble appmessage send 0:int32:95 1:int32:15 4:int32:$(date +%s)` and `pebble appmessage send 0:int32:95 1:int32:-2 4:int32:$(date +%s)`.
  - **Verification:** Confirm that the watchface does not crash, logs warnings in `pebble logs`, and falls back to displaying the `RESOURCE_ID_ARROW_NONE` (neutral gray `??` arrow).

---

## v2 Upgrade: Robust Architecture (plan.md)

### Phase 1: Timekeeping & Core BG Display

- [x] **Task 1.1 — State + Storage Layer (TDD)**
  - Expanded `AAPSState` struct in `src/c/logic.h` with all v2 fields (delta, avg_delta, date_str, iob, cob, basal, iob_detail, bg_history, history_count, low_target, high_target, is_mmol).
  - Created `src/c/state_store.h` and `src/c/state_store.c` (Pebble persist API wrapper).
  - Added `tests/test_logic.c` tests: `test_state_store_roundtrip`, `test_state_store_empty`.
  - **Verification:** `gcc -Wall -Wextra src/c/logic.c tests/test_logic.c -o test_runner && ./test_runner` → `All host tests passed successfully!` ✅

- [x] **Task 1.2 — pebble-scalable Integration + Layer Refactor**
  - Rewrote `src/c/pebble_watchface.c` with `#include <pebble-scalable/pebble-scalable.h>` and `#include "state_store.h"`.
  - Replaced all hardcoded pixel coordinates with `scl_x()` / `scl_y()` from the responsive layout table.
  - `app_message_open` expanded from `(64,64)` to `(256,256)`.
  - Added DELTA and AVG_DELTA string key parsing.
  - State loaded from flash on startup (`state_store_load`), saved on every AppMessage receipt.
  - **Verification:** `pebble build` succeeds ✅

- [x] **Task 1.3 — Analog Clock (Background Canvas + Hands Layer)**
  - Added `s_background_layer` (cardinal ticks + 8 diagonal corner ticks).
  - Added `s_hands_layer` with `HOUR_HAND_INFO` and `MINUTE_HAND_INFO` GPath data.
  - Tick handler marks hands layer dirty every minute.
  - **Verification:** `pebble build` succeeds ✅

- [x] **Phase 1 Visual Verification Gate**
  - Deploy to emery emulator, take screenshots, verify analog hands, ticks, BG display, persistence. ✅
  - **Added:** Clock hand angle unit tests (`calculate_hour_angle`, `calculate_minute_angle`) verified on host (`test_clock_angles` test case). ✅

### Phase 2: Pump & Loop Status Columns

- [x] **Task 2.1 — Expand AAPSState + Logic (TDD)**
  - Implemented `update_aaps_status()` in `logic.c` / `logic.h`.
  - Added tests: `test_update_status_strings`, `test_update_status_truncation`.
  - **Verification:** All host tests pass ✅

- [x] **Task 2.2 — Add TextLayers and AppMessage Parse**
  - Added `s_iob_layer`, `s_iob_detail_layer`, `s_basal_layer`, `s_cob_layer`, `s_date_layer` TextLayers with scalable layout.
  - All 12 AppMessage keys parsed in `inbox_received_callback`.
  - `package.json` updated with all 12 keys (IOB=2, COB=3, BASAL=5, IOB_DETAIL=6, DELTA=7, AVG_DELTA=8, GLUCOSE_HISTORY=9, LOW_TARGET=10, HIGH_TARGET=11).
  - **Verification:** `pebble build` succeeds ✅

- [x] **Phase 2 Visual Verification Gate**
  - Send pump status data, take screenshot, verify left/right columns. ✅

### Phase 3: Glucose History Graph

- [x] **Task 3.1 — History Logic (TDD)**
  - Implemented `add_to_history()`, `shift_history_left()`, `calculate_graph_y()` in `logic.c`.
  - Added tests: `test_add_to_history`, `test_shift_history_left`, `test_calculate_graph_y`, `test_target_defaults`.
  - **Verification:** All host tests pass ✅

- [x] **Task 3.2 — Graph Canvas Rendering**
  - Added `s_graph_layer` with `graph_layer_update_proc` drawing dashed target lines and colored glucose dot/line curve.
  - Stale data slot insertion in AppMessage callback (gap detection via `shift_history_left`).
  - **Verification:** `pebble build` succeeds ✅

- [x] **Phase 3 Visual Verification Gate**
  - Send 36-byte glucose history, take screenshot, verify dashed lines, curve, dot colors. ✅

---

### Phase 4: Arrow Icon Refresh & Dynamic BG Color & Strikethrough

- [x] **Task 4.1 — Arrow Icon Asset Refresh**
  - Created white-on-transparent 36×36 PNG arrow assets (`arrow_flat.png`, `arrow_single_up.png`, `arrow_single_down.png`, `arrow_forty_five_up.png`, `arrow_forty_five_down.png`) derived from clean source design.
  - **Verification:** `pebble build` succeeds ✅

- [x] **Task 4.2 — BgColorState & Host TDD Tests**
  - Implemented `BgColorState` enum (`BG_COLOR_IN_RANGE`, `BG_COLOR_HIGH`, `BG_COLOR_LOW`, `BG_COLOR_NO_DATA`) and `get_bg_color_state()` in `logic.c`/`logic.h`.
  - Added 28 unit tests in `tests/test_logic.c` covering stale readings, missing readings, target defaults, custom targets, boundary exact matches, and edge cases.
  - **Verification:** Host tests pass cleanly (`./test_runner` → GREEN) ✅

- [x] **Task 4.3 — UX Palette Swapping, BG Text Color & Strikethrough Layer**
  - Applied runtime palette-swapping to `GBitmap` palette in `update_trend_arrow()` to tint white PNG arrows dynamically based on `BgColorState`.
  - Bound BG text color to `BgColorState` (`GColorIslamicGreen`, `GColorOrange`, `GColorBulgarianRose`, or `GColorDarkGray`).
  - Added `s_bg_strike_layer` to draw a 3px horizontal strikethrough line over the BG text whenever state is `BG_COLOR_NO_DATA`.
  - **Verification:** `pebble build` succeeds ✅

- [x] **Task 4.4 — Double Arrow Assets & Defensive Test Harness Expansion**
  - Processed `/home/clark/dev/tmp/doubleuporiginal.png` into crisp 1-bit solid white `arrow_double_up.png` and `arrow_double_down.png` 36×36 assets.
  - Expanded host test harness with 4 additional defensive edge case test suites (`test_calculate_graph_y_invalid_targets`, `test_shift_history_overflow`, `test_update_status_partial_null`, `test_null_buffer_guards`).
  - **Verification:** Host unit test harness passes 20 test functions / 36 assertions (`./test_runner` → GREEN) ✅

