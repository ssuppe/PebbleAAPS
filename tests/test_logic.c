#include "../src/c/logic.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

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

int main() {
  printf("Running tests...\n");
  test_init_state();
  test_update_state();
  test_safe_trend_index();
  test_format_age_string();
  test_age_color_state();
  printf("All host tests passed successfully!\n");
  return 0;
}
