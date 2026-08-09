# Pebble Watchface Design & Implementation Plan: AAPS Companion (Pebble Time 2 MVP)

## 1. Title & TL;DR
* **Title:** Engineering Design Document for AndroidAPS (AAPS) Pebble Time 2 (Emery) Watchface MVP
* **Status:** PROPOSED
* **Author:** AI Coding Assistant
* **Target Platform:** Emery (Pebble Time 2)
* **Screen Spec:** 200 x 228 pixels, 64-color screen (8-bit)
* **TL;DR:** This document outlines the Pebble Time 2 watchface architecture, Python 3-based build environment setup, decoupled business logic, host-runnable TDD unit test runner, and emulator integration verification plan.

---

## 2. Pebble Build Environment Setup

To compile the watchface app locally, you need the Pebble SDK and command-line tools. The Core Devices and Rebble developer communities maintain a Python 3 compatible fork of the command-line developer tools (`pebble-tool`).

### A. Host System Dependencies
Install system libraries needed for node-based asset processing, gcc compiling, and the QEMU-based Pebble emulator:

```bash
# Ubuntu / Debian systems:
sudo apt update
sudo apt install -y nodejs npm libsdl2-2.0-0 libglib2.0-0 libpixman-1-0 zlib1g libsndio7.0
```

### B. Python 3 `pebble-tool` Installation via `uv`
The modern `pebble-tool` is installed as a Python application using `uv` (a fast Python package runner/manager):

1. **Install `uv`** (if not already installed):
   ```bash
   curl -LsSf https://astral.sh/uv/install.sh | sh
   ```
2. **Install `pebble-tool`**:
   ```bash
   uv tool install pebble-tool --python 3.13
   ```
   *(Note: Target Python versions >= 3.10 are supported. Specifying `--python 3.13` ensures standard compatibility with modern environments).*
3. **Verify Installation**:
   ```bash
   pebble --version
   ```

### C. Download the Pebble SDK Binaries
Initialize the Pebble SDK using the command line:

```bash
# Install the latest official SDK platform binaries
pebble sdk install latest

# List installed SDK platforms to confirm 'emery' (Pebble Time 2) is active
pebble sdk list
```

---

## 3. Protocol & Key Definitions

The watchface acts as a passive client communicating via Pebble's **AppMessage** framework. AndroidAPS pushes new data packages roughly every 5 minutes. The watchface deserializes the payload, updates its internal state, and automatically issues an acknowledgement (ACK).

### AppMessage Keys Configuration (`package.json`)
The watchface must declare the following appKeys in its `package.json` to map integers to C symbols:

```json
{
  "uuid": "54d3008f-e144-4712-b201-24bc515c40ba",
  "watchapp": {
    "watchface": true
  },
  "appKeys": {
    "BG": 0,
    "TREND": 1,
    "TIME": 4
  },
  "targetPlatforms": [
    "emery"
  ]
}
```

> [!NOTE]
> The target platform is strictly set to `emery`. The UUID is configured to `54D3008F-E144-4712-B201-24BC515C40BA` to match the default value expected by the AAPS Pebble Plugin companion module.

### AppMessage Payload Details
* **`BG` (Key `0`)**: `int32` — Blood Glucose reading in mg/dL (e.g., `120`).
* **`TREND` (Key `1`)**: `int32` — Integer from `0` to `9` representing the trend direction.
* **`TIME` (Key `4`)**: `int32` — Unix epoch timestamp (seconds) when the reading was recorded.

### Trend Arrow Mapping
The `TREND` key integer maps directly to a visual trend graphic according to the AAPS `TrendArrow` enum. We use larger 48x48 color assets for the Pebble Time 2's high-resolution display.

| Ordinal Value | Trend Direction | Visual Description | Target Resource ID (C Symbol) |
|---|---|---|---|
| **0** | None | Unknown/missing trend (gray `??` icon) | `RESOURCE_ID_ARROW_NONE` |
| **1** | Triple Up | Extremely fast rising BG (red/orange ↑↑↑) | `RESOURCE_ID_ARROW_TRIPLE_UP` |
| **2** | Double Up | Very fast rising BG (red/orange ↑↑) | `RESOURCE_ID_ARROW_DOUBLE_UP` |
| **3** | Single Up | Fast rising BG (orange ↑) | `RESOURCE_ID_ARROW_SINGLE_UP` |
| **4** | Forty Five Up | Moderately rising BG (yellow/green ↗) | `RESOURCE_ID_ARROW_FORTY_FIVE_UP` |
| **5** | Flat | Stable BG (green →) | `RESOURCE_ID_ARROW_FLAT` |
| **6** | Forty Five Down| Moderately falling BG (yellow/green ↘) | `RESOURCE_ID_ARROW_FORTY_FIVE_DOWN` |
| **7** | Single Down | Fast falling BG (orange ↓) | `RESOURCE_ID_ARROW_SINGLE_DOWN` |
| **8** | Double Down | Very fast falling BG (red/orange ↓↓) | `RESOURCE_ID_ARROW_DOUBLE_DOWN` |
| **9** | Triple Down | Extremely fast falling BG (red/orange ↓↓↓) | `RESOURCE_ID_ARROW_TRIPLE_DOWN` |

---

## 4. UI Layout (Pebble Time 2 - 200 x 228 px)

The Pebble Time 2 has a large, high-resolution color screen (200x228) compared to the original Pebble Time (144x168). This allows us to use large, high-visibility typography and larger 48x48 color trend icons.

### Layout Coordinates
Since we are building strictly for Pebble Time 2, all coordinate offsets are absolute:

* **System Time Layer (Top):**
  * **Frame:** `GRect(0, 10, 200, 40)`
  * **Alignment:** Centered
  * **Font:** `FONT_KEY_LECO_36_BOLD_NUMBERS` or `FONT_KEY_BITHAM_34_MEDIUM_NUMBERS`
* **Blood Glucose (BG) Value Layer (Middle-Left):**
  * **Frame:** `GRect(10, 75, 120, 50)`
  * **Alignment:** Right
  * **Font:** `FONT_KEY_LECO_42_NUMBERS` (Very readable, large font)
* **Trend Arrow Layer (Middle-Right):**
  * **Frame:** `GRect(140, 76, 48, 48)` (48x48 color icon)
* **Age Layer (Bottom):**
  * **Frame:** `GRect(0, 165, 200, 30)`
  * **Alignment:** Centered
  * **Font:** `FONT_KEY_GOTHIC_24_BOLD`

### Visual Structure
```
+-----------------------------------+
|             10:42 AM              |  <- System Time (H: 40px)
|                                   |
|                                   |
|       120     [Color Arrow]       |  <- BG & 48x48 Trend Icon (H: 50px)
|                                   |
|                                   |
|              3m ago               |  <- Age of BG reading (H: 30px)
+-----------------------------------+
```

---

## 5. TDD Architecture: Host-Runnable Logic Module

To follow strict Test-Driven Development (TDD), we separate the watchface logic from the Pebble SDK. The business logic file compiles on the host machine (`x86_64` Linux) using standard C compilation tools, allowing us to verify state code and string calculations before importing them to the Pebble C app shell.

```
Project File Layout:
├── package.json
├── wscript
├── src/
│   ├── logic.h              <-- Business Logic Headers (Decoupled from SDK)
│   ├── logic.c              <-- Business Logic Functions (C-only)
│   └── pebble_watchface.c   <-- Pebble SDK App Shell & UI Layout code
└── tests/
    └── test_logic.c         <-- Host Test Runner (runs in TDD loop on host PC)
```

### A. Interface (`src/logic.h`)
```c
#ifndef LOGIC_H
#define LOGIC_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// AAPS Glucose Display Colors
typedef enum {
  COLOR_STATE_NORMAL,
  COLOR_STATE_STALE
} ColorState;

// State Data Structure
typedef struct {
  int32_t bg_value;
  int32_t trend_value;
  time_t last_reading_time;
  bool has_data;
} AAPSState;

// Decoupled logic interface
void init_aaps_state(AAPSState *state);
bool update_aaps_state(AAPSState *state, int32_t bg, int32_t trend, time_t rx_time);
int get_safe_trend_index(int32_t trend_value, int max_resources);
void format_age_string(char *buffer, size_t buffer_size, time_t current_time, time_t rx_time, bool has_data);
ColorState get_age_color_state(time_t current_time, time_t rx_time, bool has_data);

#endif // LOGIC_H
```

### B. Implementation (`src/logic.c`)
```c
#include "logic.h"
#include <stdio.h>

void init_aaps_state(AAPSState *state) {
  if (state) {
    state->bg_value = 0;
    state->trend_value = 0;
    state->last_reading_time = 0;
    state->has_data = false;
  }
}

bool update_aaps_state(AAPSState *state, int32_t bg, int32_t trend, time_t rx_time) {
  if (!state) return false;
  state->bg_value = bg;
  state->trend_value = trend;
  state->last_reading_time = rx_time;
  state->has_data = true;
  return true;
}

int get_safe_trend_index(int32_t trend_value, int max_resources) {
  if (trend_value < 0 || trend_value >= max_resources) {
    return 0; // Fallback to safe default (None/Neutral)
  }
  return (int)trend_value;
}

void format_age_string(char *buffer, size_t buffer_size, time_t current_time, time_t rx_time, bool has_data) {
  if (!buffer || buffer_size == 0) return;
  if (!has_data) {
    snprintf(buffer, buffer_size, "No Data");
    return;
  }

  int delta_seconds = (int)(current_time - rx_time);
  if (delta_seconds < 0) {
    snprintf(buffer, buffer_size, "0m ago");
  } else {
    int minutes = delta_seconds / 60;
    if (minutes > 999) {
      snprintf(buffer, buffer_size, ">999m ago");
    } else {
      snprintf(buffer, buffer_size, "%dm ago", minutes);
    }
  }
}

ColorState get_age_color_state(time_t current_time, time_t rx_time, bool has_data) {
  if (!has_data) return COLOR_STATE_NORMAL;
  int delta_seconds = (int)(current_time - rx_time);
  if (delta_seconds >= 900) { // 15 minutes = 900 seconds
    return COLOR_STATE_STALE;
  }
  return COLOR_STATE_NORMAL;
}
```

### C. Host Unit Tests (`tests/test_logic.c`)
```c
#include "../src/logic.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

void test_init_state() {
  AAPSState state;
  init_aaps_state(&state);
  assert(state.bg_value == 0);
  assert(state.trend_value == 0);
  assert(state.last_reading_time == 0);
  assert(state.has_data == false);
  printf("test_init_state passed!\n");
}

void test_update_state() {
  AAPSState state;
  init_aaps_state(&state);
  bool res = update_aaps_state(&state, 120, 5, 1700000000);
  assert(res == true);
  assert(state.bg_value == 120);
  assert(state.trend_value == 5);
  assert(state.last_reading_time == 1700000000);
  assert(state.has_data == true);
  printf("test_update_state passed!\n");
}

void test_safe_trend_index() {
  // Test valid bounds
  assert(get_safe_trend_index(0, 10) == 0);
  assert(get_safe_trend_index(5, 10) == 5);
  assert(get_safe_trend_index(9, 10) == 9);
  
  // Test lower out of bounds
  assert(get_safe_trend_index(-1, 10) == 0);
  assert(get_safe_trend_index(-5, 10) == 0);
  
  // Test upper out of bounds
  assert(get_safe_trend_index(10, 10) == 0);
  assert(get_safe_trend_index(15, 10) == 0);
  printf("test_safe_trend_index passed!\n");
}

void test_format_age_string() {
  char buf[32];
  
  // Test no data
  format_age_string(buf, sizeof(buf), 1700000000, 0, false);
  assert(strcmp(buf, "No Data") == 0);
  
  // Test 0 seconds age
  format_age_string(buf, sizeof(buf), 1700000000, 1700000000, true);
  assert(strcmp(buf, "0m ago") == 0);
  
  // Test watch time behind phone time
  format_age_string(buf, sizeof(buf), 1700000000, 1700000100, true);
  assert(strcmp(buf, "0m ago") == 0);
  
  // Test normal minutes
  format_age_string(buf, sizeof(buf), 1700000300, 1700000000, true); // 300s = 5m
  assert(strcmp(buf, "5m ago") == 0);
  
  // Test very large age
  format_age_string(buf, sizeof(buf), 1700060000, 1700000000, true); // 60000s = 1000m
  assert(strcmp(buf, ">999m ago") == 0);
  printf("test_format_age_string passed!\n");
}

void test_age_color_state() {
  // Test no data
  assert(get_age_color_state(1700000000, 0, false) == COLOR_STATE_NORMAL);
  
  // Test fresh reading (less than 15 mins)
  assert(get_age_color_state(1700000899, 1700000000, true) == COLOR_STATE_NORMAL); // 14m 59s
  
  // Test stale reading (exactly 15 mins)
  assert(get_age_color_state(1700000900, 1700000000, true) == COLOR_STATE_STALE); // 15m 00s
  
  // Test stale reading (more than 15 mins)
  assert(get_age_color_state(1700001000, 1700000000, true) == COLOR_STATE_STALE);
  printf("test_age_color_state passed!\n");
}

int main() {
  test_init_state();
  test_update_state();
  test_safe_trend_index();
  test_format_age_string();
  test_age_color_state();
  printf("All host tests passed successfully!\n");
  return 0;
}
```

### D. TDD Compile & Run Command
To compile and execute the test runner during the red-green development cycle, run:
```bash
gcc -Wall -Wextra src/logic.c tests/test_logic.c -o test_runner && ./test_runner
```

---

## 6. Pebble UI Shell & Communication Handlers

The UI shell wraps around the tested core logic, mapping state variables to TextLayers and BitmapLayers.

### State & Structures
```c
#include <pebble.h>
#include "logic.h"

static AAPSState s_state;

// UI Elements
static Window *s_main_window;
static TextLayer *s_time_layer;
static TextLayer *s_bg_layer;
static TextLayer *s_age_layer;
static BitmapLayer *s_arrow_layer;
static GBitmap *s_arrow_bitmap = NULL;
```

### Dynamic Redraw Operations
```c
static void update_bg_display() {
  static char bg_buffer[8];
  if (!s_state.has_data) {
    snprintf(bg_buffer, sizeof(bg_buffer), "---");
  } else {
    snprintf(bg_buffer, sizeof(bg_buffer), "%d", (int)s_state.bg_value);
  }
  text_layer_set_text(s_bg_layer, bg_buffer);
}

static void update_age_display() {
  static char age_buffer[16];
  time_t now = time(NULL);
  
  format_age_string(age_buffer, sizeof(age_buffer), now, s_state.last_reading_time, s_state.has_data);
  text_layer_set_text(s_age_layer, age_buffer);

  // Apply stale color alert
  ColorState color_state = get_age_color_state(now, s_state.last_reading_time, s_state.has_data);
  if (color_state == COLOR_STATE_STALE) {
    text_layer_set_text_color(s_age_layer, GColorRed);
  } else {
    text_layer_set_text_color(s_age_layer, GColorWhite);
  }
}
```

### Safe Dynamic Asset Loading
```c
static const uint32_t ARROW_RESOURCE_IDS[] = {
  RESOURCE_ID_ARROW_NONE,             // 0
  RESOURCE_ID_ARROW_TRIPLE_UP,        // 1
  RESOURCE_ID_ARROW_DOUBLE_UP,        // 2
  RESOURCE_ID_ARROW_SINGLE_UP,        // 3
  RESOURCE_ID_ARROW_FORTY_FIVE_UP,    // 4
  RESOURCE_ID_ARROW_FLAT,             // 5
  RESOURCE_ID_ARROW_FORTY_FIVE_DOWN,  // 6
  RESOURCE_ID_ARROW_SINGLE_DOWN,      // 7
  RESOURCE_ID_ARROW_DOUBLE_DOWN,      // 8
  RESOURCE_ID_ARROW_TRIPLE_DOWN       // 9
};

static void update_trend_arrow() {
  int num_resources = sizeof(ARROW_RESOURCE_IDS) / sizeof(ARROW_RESOURCE_IDS[0]);
  int safe_idx = get_safe_trend_index(s_state.trend_value, num_resources);

  // Free previous image to save RAM
  if (s_arrow_bitmap) {
    gbitmap_destroy(s_arrow_bitmap);
    s_arrow_bitmap = NULL;
  }

  // Dynamic heap allocation
  s_arrow_bitmap = gbitmap_create_with_resource(ARROW_RESOURCE_IDS[safe_idx]);
  
  if (s_arrow_bitmap) {
    bitmap_layer_set_bitmap(s_arrow_layer, s_arrow_bitmap);
    layer_mark_dirty(bitmap_layer_get_layer(s_arrow_layer));
  }
}
```

---

## 7. Emulator Testing & Verification Plan

Verification ensures the watchface functions correctly on the Pebble Time 2 emulator (`emery`).

### Build & Deploy Commands
```bash
# 1. Compile watchapp binary targeting Emery platform
pebble build

# 2. Launch the Pebble Time 2 (Emery) emulator and install
pebble install --emulator emery
```

### Mock Payloads for Developer Verification
Send AppMessage test payloads directly to the active Emery emulator:

#### Test Case 1: High Blood Glucose with Fast Rise
* **Payload:** `BG = 280`, `TREND = 2` (Double Up), `TIME = <current_time>`
* **Verification:** Verify BG is `280`, Trend shows red/orange `↑↑` icon, and Age is `0m ago`.
* **Command:**
  ```bash
  pebble appmessage send 0:int32:280 1:int32:2 4:int32:$(date +%s)
  ```

#### Test Case 2: Out of Bounds Recovery
* **Payload:** `BG = 80`, `TREND = -3` (Invalid), `TIME = <current_time>`
* **Verification:** Verify that the bounds guard catches `-3`, logs a warning, defaults the Trend to `RESOURCE_ID_ARROW_NONE` (gray `??` icon), and does not crash the app.
* **Command:**
  ```bash
  pebble appmessage send 0:int32:80 1:int32:-3 4:int32:$(date +%s)
  ```

#### Test Case 3: Stale Data Alert
* **Payload:** `BG = 115`, `TREND = 5` (Flat), `TIME = <current_time - 1200>` (20 minutes ago)
* **Verification:** Verify age reads `20m ago` and its text color automatically changes to red indicating stale data.
* **Command:**
  ```bash
  pebble send-app-message --int 0=115 1=5 4=$(( $(date +%s) - 1200 ))
  ```

---

## 8. Addendum: Implementation Adjustments & Successful POC Verification

During implementation and local verification, several environment constraints and C compilations required refinements to the original design plan. These adjustments were successfully integrated, and the final Proof of Concept (POC) has been physically verified.

### A. Core Adjustments
1. **Source File Layout Realignment**: The C source files `logic.h` and `logic.c` were relocated into `src/c/` to strictly align with the standard compiler file search rules defined in `wscript` (which checks `src/c/**/*.c` to compile).
2. **Standard Headers Inclusion**: In `src/c/logic.h`, the compilation originally failed due to the `size_t` symbol being undefined under the ARM embedded cross-compiler. This was resolved by adding `#include <stddef.h>` inside the header.
3. **Headless Emulator (SSH) Execution**: To run on the headless N100 desktop over SSH without an active display session:
   - Launched the emulator with VNC and WebSocket proxy services enabled: `pebble install --emulator emery --vnc`.
   - Used `pebble screenshot /path/to/screenshot.png` with absolute paths to capture frame buffers and verify layout.
   - Discovered that the command to send mock messages was `pebble send-app-message --int KEY=VALUE ...` instead of the legacy `appmessage` subcommand.

### B. Proof of Concept (POC) Verification
* **Physical Hardware Verification**: The final watchface binary bundle `PebbleAAPS.pbw` was compiled, sideloaded onto a physical Pebble Time 2 watch, and tested.
* **Functional Integration**: Pairing was completed, the Pebble sync plugin was enabled in AndroidAPS, and the watch successfully received real-time BG readings, displaying them with color-coded trend arrows and data ages in white/red dynamically.
