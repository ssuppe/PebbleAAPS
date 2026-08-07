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
