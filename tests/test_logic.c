/**
 * test_logic.c — Host unit tests for PebbleAAPS logic layer.
 *
 * Compile & run (no Pebble SDK required):
 *   gcc -Wall -Wextra src/c/logic.c tests/test_logic.c -o test_runner && ./test_runner
 *
 * The state_store tests mock the Pebble persist API with an in-memory buffer
 * so state_store.c (which #includes <pebble.h>) is NOT linked here.
 */
#include "../src/c/logic.h"
#include "../src/c/state_store.h"
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

// ══════════════════════════════════════════════════════════════
// Mock Pebble persist API — used by state_store_* stubs below
// ══════════════════════════════════════════════════════════════
static uint8_t  s_mock_storage[512];
static bool     s_storage_has_data = false;

// Stub implementations of state_store_* (replaces state_store.c for host tests)
bool state_store_load(AAPSState *state) {
  if (!state)               return false;
  if (!s_storage_has_data)  return false;
  memcpy(state, s_mock_storage, sizeof(AAPSState));
  return true;
}

void state_store_save(const AAPSState *state) {
  if (!state) return;
  memcpy(s_mock_storage, state, sizeof(AAPSState));
  s_storage_has_data = true;
}

void state_store_clear(void) {
  memset(s_mock_storage, 0, sizeof(s_mock_storage));
  s_storage_has_data = false;
}

// ══════════════════════════════════════════════════════════════
// Phase 1 — Original tests (unchanged)
// ══════════════════════════════════════════════════════════════

void test_init_state() {
  AAPSState state;
  // Initialize with dummy values first to make sure init cleans them
  state.bg_value = 999;
  state.trend_value = 9;
  state.last_reading_time = 12345678;
  state.has_data = true;

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

  // Test watch time behind phone time (clock skew)
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

// ══════════════════════════════════════════════════════════════
// Task 1.1 — State store round-trip tests
// ══════════════════════════════════════════════════════════════

void test_state_store_roundtrip() {
  s_storage_has_data = false;
  memset(s_mock_storage, 0, sizeof(s_mock_storage));

  AAPSState written = {0};
  written.bg_value = 142;
  written.has_data = true;
  strncpy(written.delta, "+3", sizeof(written.delta) - 1);

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

// ══════════════════════════════════════════════════════════════
// Task 2.1 — Pump / loop status string tests
// ══════════════════════════════════════════════════════════════

void test_update_status_strings() {
  AAPSState state = {0};
  init_aaps_state(&state);

  update_aaps_status(&state, "0.32 U", "0g", "0.90", "(0.02|0.31)");

  assert(strcmp(state.iob,        "0.32 U")     == 0);
  assert(strcmp(state.cob,        "0g")          == 0);
  assert(strcmp(state.basal,      "0.90")        == 0);
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

// ══════════════════════════════════════════════════════════════
// Task 3.1 — Glucose history / graph tests
// ══════════════════════════════════════════════════════════════

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
  assert(calculate_graph_y(125, 70, 180, 170, 195) == 183);  // midpoint: (55*25)/110=12 → 195-12=183
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

void test_clock_angles() {
  // TRIG_MAX_ANGLE = 65536
  
  // 12:00
  assert(calculate_hour_angle(12, 0) == 0);
  assert(calculate_minute_angle(0)   == 0);
  
  // 3:00
  // hour angle = (65536 * (3 * 6)) / 72 = 65536 * 18 / 72 = 16384 (90 degrees, i.e. TRIG_MAX_ANGLE / 4)
  assert(calculate_hour_angle(3, 0)  == 16384);
  assert(calculate_minute_angle(0)   == 0);
  
  // 6:30
  // min angle = 65536 * 30 / 60 = 32768 (180 degrees, i.e. TRIG_MAX_ANGLE / 2)
  // hour angle = (65536 * ((6 * 6) + 3)) / 72 = 65536 * 39 / 72 = 35498 (integer division)
  assert(calculate_minute_angle(30)  == 32768);
  assert(calculate_hour_angle(6, 30) == 35498);
  
  // 10:10
  // min angle = 65536 * 10 / 60 = 10922
  // hour angle = (65536 * ((10 * 6) + 1)) / 72 = 65536 * 61 / 72 = 55523
  assert(calculate_minute_angle(10)   == 10922);
  assert(calculate_hour_angle(10, 10) == 55523);
  
  printf("test_clock_angles passed!\n");
}

// ══════════════════════════════════════════════════════════════
// Main
// ══════════════════════════════════════════════════════════════

int main() {
  printf("Running tests...\n");

  // Phase 1 — original
  test_init_state();
  test_update_state();
  test_safe_trend_index();
  test_format_age_string();
  test_age_color_state();

  // Task 1.1 — state store
  test_state_store_roundtrip();
  test_state_store_empty();

  // Task 2.1 — pump status strings
  test_update_status_strings();
  test_update_status_truncation();

  // Task 3.1 — glucose history & graph
  test_add_to_history();
  test_shift_history_left();
  test_calculate_graph_y();
  test_target_defaults();
  
  // Analog Clock angles
  test_clock_angles();

  printf("All host tests passed successfully!\n");
  return 0;
}
