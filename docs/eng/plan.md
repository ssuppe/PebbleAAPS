# Engineering Design & Implementation Plan: Robust PebbleAAPS Watchface v2

**Status:** READY FOR IMPLEMENTATION  
**Target Platform:** Pebble Time 2 (Emery, 200×228px, 64-color)  
**Companion Protocol Doc:** See [`docs/eng/androidaps_pebble_protocol.md`](./androidaps_pebble_protocol.md)

---

## TL;DR

This plan fully replaces the MVP watchface with a production-quality, stateful, layered architecture. Key changes from the POC:

1. **Three-layer architecture**: Storage (persistent flash) / Logic (pure C, host-testable) / UX (Pebble SDK only).
2. **Responsive layout**: All coordinates use `pebble-scalable` thousandths-of-screen percentages — no hardcoded pixel values.
3. **Full PFS layout**: Exactly matches `docs/design/PebbleAAPS.pfs` — analog hands, ticks, delta, IOB/COB/basal columns, and a 36-point glucose history graph.
4. **State persists**: Loading the app after switching away shows last-known values immediately, not "No Data".
5. **Unit preference (planned, not implemented)**: Architecture reserves a settings slot. Toggle will be added in a future phase.
6. **Graph shift-left on stale data**: If no new reading arrives, the graph slides left at 5-minute intervals, leaving empty space on the right until cleared.

Implementation is split into **3 phases**, each with a visual verification gate before moving on.

---

## Invariants

| # | Rule |
|---|------|
| 1 | **No raw pixel coordinates.** Every GRect must use `scl_x` / `scl_y` / `scl_grect`. |
| 2 | **Free before allocate.** Any `gbitmap_create_with_resource` must be preceded by `gbitmap_destroy` + `NULL` assignment. |
| 3 | **Logic is SDK-free.** `logic.c` and `state_store.c` compile cleanly with `gcc` on host — zero `#include <pebble.h>`. |
| 4 | **TDD first.** Write the failing test → implement minimum code → refactor. No implementation without a test. |
| 5 | **Persist on every update.** Call `state_store_save()` immediately after any AppMessage is processed. |
| 6 | **Static strings only.** All text buffers (`char[]`) are statically allocated inside `AAPSState`. No heap `malloc` for strings. |

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│  AndroidAPS (Phone)                                         │
│  AppMessage dict → [BG, TREND, TIME, IOB, COB, BASAL,      │
│                     IOB_DETAIL, DELTA, AVG_DELTA,           │
│                     GLUCOSE_HISTORY, LOW_TARGET, HIGH_TARGET]│
└───────────────────────────┬─────────────────────────────────┘
                            │ Bluetooth AppMessage
┌───────────────────────────▼─────────────────────────────────┐
│  pebble_watchface.c  (UX Layer — Pebble SDK only)           │
│  • Parses AppMessage tuples                                 │
│  • Calls logic.c functions                                  │
│  • Owns all layers / drawing / tick handler                 │
└──────────┬──────────────────────────────────────────────────┘
           │ calls
┌──────────▼────────────────────────────────────────────────┐
│  logic.c  (Logic Layer — pure C, host-testable)            │
│  • AAPSState struct                                        │
│  • format_age_string(), get_age_color_state()              │
│  • get_safe_trend_index()                                  │
│  • add_to_history(), shift_history_left()                  │
│  • calculate_graph_y()                                     │
└──────────┬────────────────────────────────────────────────┘
           │ calls
┌──────────▼────────────────────────────────────────────────┐
│  state_store.c  (Storage Layer — Pebble persist API)       │
│  • state_store_load()  → persist_read_data()               │
│  • state_store_save()  → persist_write_data()              │
│  • state_store_clear() → persist_delete()                  │
│  Key: PERSIST_KEY_AAPS_STATE = 100                         │
└───────────────────────────────────────────────────────────┘
```

---

## Data Model

### `AAPSState` struct (in `src/c/logic.h`)

```c
#define BG_HISTORY_COUNT 36
#define DEFAULT_LOW_TARGET  70
#define DEFAULT_HIGH_TARGET 180

typedef struct {
  // BG & trend (Phase 1)
  int32_t  bg_value;
  int32_t  trend_value;
  time_t   last_reading_time;
  bool     has_data;
  char     delta[16];       // e.g. "+3" or "+0.2"
  char     avg_delta[16];   // e.g. "+5"
  char     date_str[12];    // e.g. "9 Aug"

  // Pump status (Phase 2)
  char     iob[12];         // e.g. "0.32 U"
  char     cob[10];         // e.g. "0g"
  char     basal[12];       // e.g. "0.90"
  char     iob_detail[20];  // e.g. "(0.02|0.31)"

  // Graph (Phase 3)
  uint8_t  bg_history[BG_HISTORY_COUNT]; // BG/2, 0 = empty slot
  uint8_t  history_count;
  int32_t  low_target;      // mg/dL; 0 means "use default"
  int32_t  high_target;     // mg/dL; 0 means "use default"

  // Settings (reserved, not implemented yet)
  bool     is_mmol;         // false = mg/dL (default), true = mmol/L
} AAPSState;
```

**Storage key:** `PERSIST_KEY_AAPS_STATE 100`  
**Storage version key:** `PERSIST_KEY_STATE_VERSION 101` (for future migration)

---

## AppMessage Key Table

Defined in `package.json` under `messageKeys`. Must match [`androidaps_pebble_protocol.md`](./androidaps_pebble_protocol.md) exactly.

| Key Name | Integer ID | Type | Example | Phase |
|---|---|---|---|---|
| `BG` | 0 | `int32` | `120` | 1 |
| `TREND` | 1 | `int32` | `5` (Flat) | 1 |
| `TIME` | 4 | `int32` | Unix timestamp (seconds) | 1 |
| `DELTA` | 7 | `string` | `"+3"` | 1 |
| `AVG_DELTA` | 8 | `string` | `"+5"` | 1 |
| `IOB` | 2 | `string` | `"0.32 U"` | 2 |
| `COB` | 3 | `string` | `"0g"` | 2 |
| `BASAL` | 5 | `string` | `"0.90"` | 2 |
| `IOB_DETAIL` | 6 | `string` | `"(0.02\|0.31)"` | 2 |
| `LOW_TARGET` | 10 | `int32` | `70` | 3 |
| `HIGH_TARGET` | 11 | `int32` | `180` | 3 |
| `GLUCOSE_HISTORY` | 9 | `bytes` (36 bytes) | BG/2 per point, oldest→newest | 3 |

**AppMessage buffer size:** `app_message_open(512, 256)` (configured to handle enriched payload structure safely).

---

## Responsive Layout Reference

All coordinates are in `pebble-scalable` thousandths of screen dimension.  
Conversion formula: `t_perc = round(pixels * 1000 / screen_dim)`  
Screen: Emery = 200×228. Original rectangular = 144×168.

| Layer | scl_x | scl_y | scl_w | scl_h | Font (Emery) | Font (Original) |
|---|---|---|---|---|---|---|
| **BG** | 25 | 158 | 550 | 228 | `FONT_LILITA_48` | `FONT_LILITA_48` |
| **Arrow** | Dynamic | 184 | 180 | 158 | — | — |
| **Delta** | 50 | 44 | 375 | 105 | `GOTHIC_18` | `GOTHIC_14` |
| **Age** | 725 | 44 | 225 | 105 | `GOTHIC_18` | `GOTHIC_14` |
| **IOB** | 50 | 404 | 400 | 123 | `GOTHIC_24_BOLD` | `GOTHIC_18_BOLD` |
| **IOB Detail** | 50 | 526 | 400 | 105 | `GOTHIC_18` | `GOTHIC_14` |
| **Basal Rate** | 50 | 632 | 400 | 105 | `GOTHIC_18` | `GOTHIC_14` |
| **COB** | 575 | 404 | 375 | 123 | `GOTHIC_24_BOLD` | `GOTHIC_18_BOLD` |
| **Date** | 575 | 526 | 375 | 105 | `GOTHIC_18` | `GOTHIC_14` |

**Programmatic elements (drawn in update_proc, no layer):**
- Analog center: `scl_x(500)`, `scl_y(500)` → 100, 114px on Emery
- Hour ticks (4 cardinal, 8 diagonal corners): computed from center + radius
- High target dashed line: `scl_y(746)` → 170px
- Low target dashed line: `scl_y(855)` → 195px

Use `scl_set_fonts` to select custom fonts:

```c
// In init(), before window_stack_push():
scl_set_fonts(0, {.o = s_font_lilita_22, .e = s_font_lilita_22});
scl_set_fonts(1, {.o = s_font_lilita_32, .e = s_font_lilita_32});
scl_set_fonts(2, {.o = s_font_lilita_48, .e = s_font_lilita_48});
scl_set_fonts(3, {.o = s_font_lilita_22, .e = s_font_lilita_22});
scl_set_fonts(4, {.o = s_font_lilita_22, .e = s_font_lilita_22});
```

---

## Phase 1: Timekeeping & Core BG Display

**Goal:** Stateful watchface with analog clock, BG value, trend arrow, delta, and age. Exactly matches Phase 1 of the PFS layout.

### Task 1.1 — State + Storage Layer (TDD)

**Files:** `src/c/logic.h`, `src/c/logic.c`, `src/c/state_store.h`, `src/c/state_store.c`, `tests/test_logic.c`

**Red Phase — Write failing tests first:**

Add to `tests/test_logic.c`:
```c
// Mock the Pebble persist API with an in-memory buffer
static uint8_t s_mock_storage[512];
static bool s_storage_has_data = false;

// Implement stub persist_write_data / persist_read_data / persist_exists
// that read/write from s_mock_storage instead of flash.

void test_state_store_roundtrip() {
  AAPSState written = {0};
  written.bg_value = 142;
  written.has_data = true;
  strncpy(written.delta, "+3", sizeof(written.delta));

  state_store_save(&written);
  assert(s_storage_has_data == true);

  AAPSState loaded = {0};
  bool ok = state_store_load(&loaded);
  assert(ok == true);
  assert(loaded.bg_value == 142);
  assert(loaded.has_data == true);
  assert(strcmp(loaded.delta, "+3") == 0);
  printf("test_state_store_roundtrip passed!\n");
}

void test_state_store_empty() {
  s_storage_has_data = false;
  AAPSState loaded = {0};
  bool ok = state_store_load(&loaded);
  assert(ok == false);
  printf("test_state_store_empty passed!\n");
}
```

Compile to verify RED: `gcc -Wall -Wextra src/c/logic.c tests/test_logic.c -o test_runner && ./test_runner`

**Green Phase — Implement storage layer:**

`src/c/state_store.h`:
```c
#ifndef STATE_STORE_H
#define STATE_STORE_H
#include "logic.h"
#include <stdbool.h>
#define PERSIST_KEY_AAPS_STATE   100
#define PERSIST_KEY_STATE_VERSION 101
#define CURRENT_STATE_VERSION      1

bool state_store_load(AAPSState *state);
void state_store_save(const AAPSState *state);
void state_store_clear(void);
#endif
```

`src/c/state_store.c` (Pebble version — not compiled in host tests):
```c
#include "state_store.h"
#include <pebble.h>

bool state_store_load(AAPSState *state) {
  if (!state) return false;
  if (!persist_exists(PERSIST_KEY_AAPS_STATE)) return false;
  int bytes = persist_read_data(PERSIST_KEY_AAPS_STATE, state, sizeof(AAPSState));
  return (bytes == sizeof(AAPSState));
}

void state_store_save(const AAPSState *state) {
  if (!state) return;
  persist_write_data(PERSIST_KEY_AAPS_STATE, state, sizeof(AAPSState));
}

void state_store_clear(void) {
  persist_delete(PERSIST_KEY_AAPS_STATE);
}
```

Confirm GREEN: all tests pass.

### Task 1.2 — pebble-scalable Integration + Layer Refactor

**File:** `src/c/pebble_watchface.c`

1. Add `#include <pebble-scalable/pebble-scalable.h>` at top.
2. Add `#include "state_store.h"`.
3. In `init()` — before `window_stack_push()` — call `scl_set_fonts(...)` for all 3 size IDs.
4. In `main_window_load()`, replace every `GRect(x, y, w, h)` with `GRect(scl_x(X), scl_y(Y), scl_x(W), scl_y(H))` using the table above.
5. In `init()`, after `init_aaps_state()`: try `state_store_load(&s_state)`; if it succeeds, call all update display functions immediately so stale data is visible before first AppMessage.
6. In `inbox_received_callback()`, call `state_store_save(&s_state)` immediately after `update_aaps_state()`.
7. Expand `app_message_open(64, 64)` → `app_message_open(256, 256)`.
8. Add parsing for `MESSAGE_KEY_DELTA` and `MESSAGE_KEY_AVG_DELTA` strings in the inbox callback.

**Compile check:** `pebble build` must succeed with no errors.

### Task 1.3 — Analog Clock (Background Canvas + Hands Layer)

**File:** `src/c/pebble_watchface.c`

Add two new layers: `s_background_layer` (draws ticks, dashes) and `s_hands_layer` (draws hands), created in `main_window_load` and destroyed in `main_window_unload`.

**Background layer update proc** — cardinal ticks:
```c
// 12 o'clock: vertical bar at top center
graphics_fill_rect(ctx,
  GRect(scl_x(495), 0, scl_x(10), scl_y(44)),
  0, GCornerNone);

// 6 o'clock
graphics_fill_rect(ctx,
  GRect(scl_x(495), bounds.size.h - scl_y(44), scl_x(10), scl_y(44)),
  0, GCornerNone);

// 9 o'clock  
graphics_fill_rect(ctx,
  GRect(0, scl_y(495), scl_x(50), scl_y(9)),
  0, GCornerNone);

// 3 o'clock
graphics_fill_rect(ctx,
  GRect(bounds.size.w - scl_x(50), scl_y(495), scl_x(50), scl_y(9)),
  0, GCornerNone);

// Diagonal corner ticks (1,2,4,5,7,8,10,11 o'clock positions)
// Use the GPath pixel positions from docs/design/PebbleAAPS.pfs:
// 1=166,0  2=200,56  4=200,172  5=166,228
// 7=34,228 8=0,172   10=0,56    11=34,0
// Draw 8×8 squares scaled: scl_pp({.o=6, .e=8})
int tick_sz = scl_pp({.o = 6, .e = 8});
// ... (draw at each position with graphics_fill_rect)
```

**Hands layer update proc:**
```c
// Hour hand GPathInfo: points from center, length ~35px on Emery
// Minute hand GPathInfo: points from center, length ~65px on Emery
// Use TRIG_MAX_ANGLE for angle computation:
int32_t hour_angle = (TRIG_MAX_ANGLE * (((tick_time->tm_hour % 12) * 6)
                      + (tick_time->tm_min / 10))) / 72;
int32_t min_angle  = TRIG_MAX_ANGLE * tick_time->tm_min / 60;
gpath_rotate_to(s_hour_path, hour_angle);
gpath_rotate_to(s_minute_path, min_angle);
gpath_move_to(s_hour_path,   GPoint(scl_x(500), scl_y(500)));
gpath_move_to(s_minute_path, GPoint(scl_x(500), scl_y(500)));
graphics_context_set_stroke_width(ctx, 5);
gpath_draw_outline(ctx, s_hour_path);
graphics_context_set_stroke_width(ctx, 3);
gpath_draw_outline(ctx, s_minute_path);
```

**Tick handler:** subscribe with `MINUTE_UNIT`, call `layer_mark_dirty(s_hands_layer)` in addition to existing `update_time()` and `update_age_display()`.

### Phase 1 Visual Verification Gate

```bash
# 1. Host tests pass
gcc -Wall -Wextra src/c/logic.c tests/test_logic.c -o test_runner && ./test_runner

# 2. Build
pebble build

# 3. Deploy + screenshot
Xvfb :99 -screen 0 800x600x24 &
DISPLAY=:99 pebble install --emulator emery --vnc
pebble screenshot docs/review/phase1_no_data.png

# 4. Send BG data
pebble send-app-message --int 0=120 1=5 4=$(date +%s) --string 7="+3" 8="+5"
pebble screenshot docs/review/phase1_with_data.png
```

**Acceptance criteria:**
- [ ] Analog hands point to correct current system time
- [ ] 12 cardinal ticks and 8 corner ticks are visible
- [ ] BG = 120, trend arrow (flat), delta "+3", age "0m ago" displayed
- [ ] Closing the emulator, reopening it — "120" is shown immediately (persistence)

---

## Phase 2: Pump & Loop Status Columns

**Goal:** Left column (IOB, IOB Detail, Basal Rate) and right column (COB, Date) fully functional.

### Task 2.1 — Expand AAPSState + Logic (TDD)

**Red Phase — Write failing tests:**
```c
void test_update_status_strings() {
  AAPSState state = {0};
  init_aaps_state(&state);

  update_aaps_status(&state, "0.32 U", "0g", "0.90", "(0.02|0.31)");

  assert(strcmp(state.iob, "0.32 U") == 0);
  assert(strcmp(state.cob, "0g") == 0);
  assert(strcmp(state.basal, "0.90") == 0);
  assert(strcmp(state.iob_detail, "(0.02|0.31)") == 0);
  printf("test_update_status_strings passed!\n");
}

void test_update_status_truncation() {
  // A very long string must not overflow the buffer
  AAPSState state = {0};
  char long_str[100];
  memset(long_str, 'X', 99); long_str[99] = '\0';
  update_aaps_status(&state, long_str, "0g", "0.90", "(0|0)");
  assert(strlen(state.iob) < sizeof(state.iob));
  printf("test_update_status_truncation passed!\n");
}
```

**Green Phase — Implement:**

In `logic.h`:
```c
void update_aaps_status(AAPSState *state,
                        const char *iob, const char *cob,
                        const char *basal, const char *iob_detail);
```

In `logic.c`:
```c
void update_aaps_status(AAPSState *state,
                        const char *iob, const char *cob,
                        const char *basal, const char *iob_detail) {
  if (!state) return;
  if (iob)        strncpy(state->iob,        iob,        sizeof(state->iob) - 1);
  if (cob)        strncpy(state->cob,         cob,        sizeof(state->cob) - 1);
  if (basal)      strncpy(state->basal,       basal,      sizeof(state->basal) - 1);
  if (iob_detail) strncpy(state->iob_detail,  iob_detail, sizeof(state->iob_detail) - 1);
}
```

### Task 2.2 — Add TextLayers and AppMessage Parse

1. **`package.json`** — add keys: `IOB=2`, `COB=3`, `BASAL=5`, `IOB_DETAIL=6`.
2. **`pebble_watchface.c`** — declare and manage 4 new `TextLayer*` pointers:
   - `s_iob_layer`, `s_iob_detail_layer`, `s_basal_layer`, `s_cob_layer`, `s_date_layer`
3. Each layer: `text_layer_create(GRect(scl_x(...), scl_y(...), scl_x(...), scl_y(...)))` using the layout table above.
4. Set background `GColorClear`, text color `GColorWhite`, font `scl_get_font(0)` or `scl_get_font(1)` as appropriate.
5. In `inbox_received_callback()`, parse the 4 new string keys and call `update_aaps_status()`.
6. Add `update_status_display()` function that calls `text_layer_set_text()` for all 5 layers.
7. In `main_window_unload()`, destroy all new layers.
8. The **Date layer** is populated from the current time (not AppMessage); update in `tick_handler`.

### Phase 2 Visual Verification Gate

```bash
pebble build

pebble send-app-message \
  --int 0=120 1=5 4=$(date +%s) \
  --string 2="0.32 U" 3="0g" 5="0.90" 6="(0.02|0.31)" 7="+3" 8="+5"

pebble screenshot docs/review/phase2_status.png
```

**Acceptance criteria:**
- [ ] Left column: IOB "0.32 U", IOB detail "(0.02|0.31)", Basal "0.90"
- [ ] Right column: COB "0g", Date shows today's date (e.g. "9 Aug")
- [ ] All text is white, correctly positioned, no overlap with analog hands
- [ ] State persistence: close + reopen shows all values

---

## Phase 3: Glucose History Graph

**Goal:** 36-point history graph at the bottom of the screen with dashed target lines, shift-left behavior on stale data, and default targets of 70/180 mg/dL.

### Graph Behavior Specification

| Scenario | Behavior |
|---|---|
| Normal update received | Shift array left by 1, insert new BG/2 at index 35 |
| Missing update (5-min slot) | Insert `0` at index 35 (empty slot), shift left |
| `LOW_TARGET` / `HIGH_TARGET` not received or zero | Use defaults: `70` / `180` |
| Reading age > 3 hours (36 slots) | All history slots = 0, graph is blank |
| BG = 0 (sentinel) in slot | Dot is not drawn; line to previous point is broken |

### Task 3.1 — History Logic (TDD)

**Red Phase:**
```c
void test_add_to_history() {
  AAPSState state = {0};
  init_aaps_state(&state);

  // First reading
  add_to_history(&state, 120);  // stored as 60
  assert(state.history_count == 1);
  assert(state.bg_history[35] == 60);

  // Second reading
  add_to_history(&state, 130);  // stored as 65
  assert(state.history_count == 2);
  assert(state.bg_history[34] == 60);
  assert(state.bg_history[35] == 65);

  // Fill past 36 — should cap at 36
  for (int i = 0; i < 40; i++) add_to_history(&state, 100);
  assert(state.history_count == 36);
  printf("test_add_to_history passed!\n");
}

void test_shift_history_left() {
  AAPSState state = {0};
  state.bg_history[35] = 60;  // one reading
  state.history_count = 1;

  shift_history_left(&state, 2);  // 2 missed intervals
  assert(state.bg_history[33] == 60);
  assert(state.bg_history[34] == 0);
  assert(state.bg_history[35] == 0);
  printf("test_shift_history_left passed!\n");
}

void test_calculate_graph_y() {
  // Low = 70, High = 180, graph area: Y_TOP=170, Y_BOTTOM=195 (25px range)
  assert(calculate_graph_y(70,  70, 180, 170, 195) == 195);  // at low = bottom
  assert(calculate_graph_y(180, 70, 180, 170, 195) == 170);  // at high = top
  assert(calculate_graph_y(125, 70, 180, 170, 195) == 182);  // midpoint ~182
  // Boundary guard: clamp to [Y_TOP, Y_BOTTOM]
  assert(calculate_graph_y(300, 70, 180, 170, 195) == 170);  // very high → top
  assert(calculate_graph_y(40,  70, 180, 170, 195) == 195);  // very low → bottom
  printf("test_calculate_graph_y passed!\n");
}

void test_target_defaults() {
  int low  = (0 == 0) ? DEFAULT_LOW_TARGET  : 0;
  int high = (0 == 0) ? DEFAULT_HIGH_TARGET : 0;
  assert(low  == 70);
  assert(high == 180);
  printf("test_target_defaults passed!\n");
}
```

**Green Phase** — implement in `logic.h` / `logic.c`:

```c
// In logic.h:
void add_to_history(AAPSState *state, int32_t bg_mgdl);
void shift_history_left(AAPSState *state, int intervals);
int  calculate_graph_y(int32_t bg, int32_t low, int32_t high,
                       int y_top, int y_bottom);

// In logic.c:
void add_to_history(AAPSState *state, int32_t bg_mgdl) {
  // Shift everything left by 1
  memmove(&state->bg_history[0], &state->bg_history[1],
          BG_HISTORY_COUNT - 1);
  uint8_t encoded = (uint8_t)((bg_mgdl / 2) & 0xFF);
  state->bg_history[BG_HISTORY_COUNT - 1] = encoded;
  if (state->history_count < BG_HISTORY_COUNT) state->history_count++;
}

void shift_history_left(AAPSState *state, int intervals) {
  if (intervals <= 0) return;
  if (intervals >= BG_HISTORY_COUNT) {
    memset(state->bg_history, 0, BG_HISTORY_COUNT);
    state->history_count = 0;
    return;
  }
  memmove(&state->bg_history[0], &state->bg_history[intervals],
          BG_HISTORY_COUNT - intervals);
  memset(&state->bg_history[BG_HISTORY_COUNT - intervals], 0, intervals);
  if (state->history_count > (uint8_t)intervals) {
    state->history_count -= (uint8_t)intervals;
  } else {
    state->history_count = 0;
  }
}

int calculate_graph_y(int32_t bg, int32_t low, int32_t high,
                      int y_top, int y_bottom) {
  if (high <= low) return y_bottom; // guard division by zero
  int range_px = y_bottom - y_top;
  int range_bg = high - low;
  int clamped  = bg < low ? low : (bg > high ? high : bg);
  int y = y_bottom - (int)(((long)(clamped - low) * range_px) / range_bg);
  return y < y_top ? y_top : (y > y_bottom ? y_bottom : y);
}
```

**On AppMessage receive** (in `pebble_watchface.c`):
```c
// Get targets with defaults
int32_t low  = s_state.low_target  ? s_state.low_target  : DEFAULT_LOW_TARGET;
int32_t high = s_state.high_target ? s_state.high_target : DEFAULT_HIGH_TARGET;

// Calculate how many 5-minute slots have elapsed since last reading
// before inserting the new one (for gaps)
time_t now = time(NULL);
if (s_state.has_data && s_state.last_reading_time > 0) {
  int elapsed_sec = (int)(now - s_state.last_reading_time);
  int missed = (elapsed_sec - 30) / 300; // subtract 30s tolerance
  if (missed > 1) shift_history_left(&s_state, missed - 1);
}
add_to_history(&s_state, bg);
```

**In `tick_handler`** (once per minute):
```c
// Check if we have slid off the right edge on stale data
if (s_state.has_data) {
  time_t now = time(NULL);
  int elapsed_sec = (int)(now - s_state.last_reading_time);
  int elapsed_intervals = elapsed_sec / 300;
  // No need to shift — graph was already shifted on last message receipt.
  // Just mark graph layer dirty to reflect current time position.
  layer_mark_dirty(s_graph_layer);
}
```

### Task 3.2 — Graph Canvas Rendering

Add `s_graph_layer` (`Layer*`) created in `main_window_load` over the full window bounds, positioned above other content via `layer_add_child` last.

```c
// update_proc for s_graph_layer:
static void graph_layer_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int y_top    = scl_y(746);  // 170px
  int y_bottom = scl_y(855);  // 195px
  int x_start  = scl_x(40);  //   8px
  int x_step   = scl_x(25);  //   5px per slot

  int32_t low  = s_state.low_target  ? s_state.low_target  : DEFAULT_LOW_TARGET;
  int32_t high = s_state.high_target ? s_state.high_target : DEFAULT_HIGH_TARGET;

  // 1. Draw dashed high target line (gray)
  graphics_context_set_stroke_color(ctx, GColorLightGray);
  for (int x = 0; x < bounds.size.w; x += 6) {
    graphics_draw_pixel(ctx, GPoint(x, y_top));
    if (x + 1 < bounds.size.w) graphics_draw_pixel(ctx, GPoint(x + 1, y_top));
    if (x + 2 < bounds.size.w) graphics_draw_pixel(ctx, GPoint(x + 2, y_top));
  }

  // 2. Draw dashed low target line (red)
  graphics_context_set_stroke_color(ctx, GColorRed);
  for (int x = 0; x < bounds.size.w; x += 6) {
    graphics_draw_pixel(ctx, GPoint(x, y_bottom));
    if (x + 1 < bounds.size.w) graphics_draw_pixel(ctx, GPoint(x + 1, y_bottom));
    if (x + 2 < bounds.size.w) graphics_draw_pixel(ctx, GPoint(x + 2, y_bottom));
  }

  // 3. Draw glucose curve and dots
  GPoint prev = GPoint(0, 0);
  bool has_prev = false;
  graphics_context_set_stroke_width(ctx, 2);

  for (int i = 0; i < BG_HISTORY_COUNT; i++) {
    uint8_t encoded = s_state.bg_history[i];
    if (encoded == 0) { has_prev = false; continue; } // empty slot → break line

    int32_t bg_val = (int32_t)encoded * 2;
    int x = x_start + (i * x_step);
    int y = calculate_graph_y(bg_val, low, high, y_top, y_bottom);

    // Color dot by zone
    GColor dot_color;
    if (bg_val < low)       dot_color = GColorRed;
    else if (bg_val > high) dot_color = GColorYellow;
    else                    dot_color = GColorWhite;

    // Connect line segment
    if (has_prev) {
      graphics_context_set_stroke_color(ctx, GColorWhite);
      graphics_draw_line(ctx, prev, GPoint(x, y));
    }

    // Draw dot (4×4 px)
    graphics_context_set_fill_color(ctx, dot_color);
    graphics_fill_rect(ctx, GRect(x - 2, y - 2, 4, 4), 0, GCornerNone);

    prev = GPoint(x, y);
    has_prev = true;
  }
}
```

### Phase 3 Visual Verification Gate

```bash
pebble build

# Build a 36-byte history (values like 70,75,80... stored as BG/2)
# Use a helper script or manual hex for mock
pebble send-app-message \
  --int 0=120 1=5 4=$(date +%s) 10=70 11=180 \
  --bytes 9="23262930333639..."  # 36 hex bytes

pebble screenshot docs/review/phase3_graph.png
```

**Acceptance criteria:**
- [ ] Dashed gray line at Y=170 (high target)
- [ ] Dashed red line at Y=195 (low target)
- [ ] Glucose curve is visible as a connected line with colored 4×4 dots
- [ ] Dots below 70 are red, above 180 are yellow, in-range are white
- [ ] If no data for 30+ minutes, graph slides left with empty space on right

---

## Settings Page (Planned, NOT Implemented in v2)

The `is_mmol` field is already reserved in `AAPSState` and persisted to flash. When a settings page is added in a future phase:

- **Pebble SimpleMenu** or **ActionMenu** UI will be used (no new dependencies).
- The user toggles mg/dL ↔ mmol/L using the Pebble select button from the settings window.
- The BG display function checks `s_state.is_mmol` and converts with `BG_VALUE / 18.0` before formatting.
- The delta and avg_delta strings are **always received pre-formatted from the phone** (the phone already knows the user's unit preference), so no conversion is needed for those fields.
- The setting is persisted via `state_store_save()` the same way all other state is.

**No code should be written for this yet.** This note exists to ensure the architecture is not designed in a way that blocks it later.

---

## Complete File Structure (Post-v2)

```
├── package.json            # 12 messageKeys, pebble-scalable dependency
├── wscript
├── src/c/
│   ├── logic.h             # AAPSState, all function declarations
│   ├── logic.c             # Business logic (SDK-free)
│   ├── state_store.h       # Storage interface
│   ├── state_store.c       # Pebble persist_read/write_data wrappers
│   └── pebble_watchface.c  # UX shell: layers, ticks, AppMessage, drawing
├── tests/
│   └── test_logic.c        # Host unit tests (gcc, no Pebble SDK)
├── resources/images/       # 10 arrow PNGs (unchanged)
└── docs/
    ├── design/
    │   ├── PebbleAAPS.pfs
    │   └── pebble_aaps_design_spec.md
    └── eng/
        ├── plan.md                          ← this file
        └── androidaps_pebble_protocol.md    ← companion app changes
```

---

## Resource & Font Optimization (Added post-v2)

To address the large initial built `.pbw` size (~60KB) and potential heap exhaustion on Pebble watches, two major optimizations were implemented:
1.  **Pruned Unused Assets:** The target dashed lines are drawn programmatically, so the unused assets `HIGH_TARGET_DASHED` and `LOW_TARGET_DASHED` were deleted from `package.json`.
2.  **Custom Font Character Filtering:** The `characterRegex` parameter was configured for all Lilita One fonts in `package.json` to compile only the specific characters used in the watchface UI:
    *   `FONT_LILITA_48` (BG Value only): `[0-9\\.\\-]`
    *   `FONT_LILITA_32` (IOB/COB values only): `[0-9\\. Ug]`
    *   `FONT_LILITA_22` (Secondary labels/Age/Delta/Basal/Date): `[0-9\\.\\+\\-\\(\\)\\|>\\'? a-zA-Z]`

### Impact of Optimizations
*   **Compiled Resource Pack (`app_resources.pbpack`):** Shrunk from **49,495 bytes** to **12,995 bytes** (73.7% reduction).
*   **Built Bundle (`PebbleAAPS.pbw`):** Shrunk from **~60 KB** to **24.1 KB** (60% reduction).
*   **RAM Heap Footprint (Fonts):** Reduced from **~22 KB** to **~3.8 KB** (82.7% savings), providing maximum runtime safety.

---

## Acceptance Checklist (All Phases)

- [x] `gcc -Wall -Wextra src/c/logic.c tests/test_logic.c -o test_runner && ./test_runner` → `All host tests passed successfully!`
- [x] `pebble build` succeeds with no errors on Emery platform
- [x] Phase 1 screenshot shows correct analog time, BG, arrow, delta, age
- [x] Phase 2 screenshot shows IOB, IOB detail, basal, COB, date columns
- [x] Phase 3 screenshot shows glucose curve, dashed targets, correct dot colors
- [x] Persistence: restart watchface → all values load immediately from flash
- [x] Stale data: after 30+ minutes with no update, graph shifts left into empty space
- [x] Targets default to 70/180 when `LOW_TARGET`/`HIGH_TARGET` not received
- [x] `pebble logs` shows no repeated memory growth over 20 min of updates
- [x] **Optimization Verify:** built `.pbw` size is <= 25KB, resource pbpack size is <= 14KB, and fonts are compiled with strict character subsets.

---

*End of plan.md*
