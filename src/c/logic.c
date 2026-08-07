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
  if (delta_seconds >= 900) { // 15 minutes * 60 seconds
    return COLOR_STATE_STALE;
  }
  return COLOR_STATE_NORMAL;
}
