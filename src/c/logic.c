#include "logic.h"
#include <stdio.h>
#include <string.h>

// ──────────────────────────────────────────────────────────────
// Phase 1 — Core BG / trend / age logic
// ──────────────────────────────────────────────────────────────

void init_aaps_state(AAPSState *state) {
  if (state) {
    memset(state, 0, sizeof(AAPSState));
    /* has_data is already false; all numerics 0; strings empty */
  }
}

bool update_aaps_state(AAPSState *state, int32_t bg, int32_t trend, time_t rx_time) {
  if (!state) return false;
  state->bg_value          = bg;
  state->trend_value       = trend;
  state->last_reading_time = rx_time;
  state->has_data          = true;
  return true;
}

int get_safe_trend_index(int32_t trend_value, int max_resources) {
  if (trend_value < 0 || trend_value >= max_resources) {
    return 0; // Fallback to safe default (None/Neutral)
  }
  return (int)trend_value;
}

void format_age_string(char *buffer, size_t buffer_size,
                       time_t current_time, time_t rx_time, bool has_data) {
  if (!buffer || buffer_size == 0) return;
  if (!has_data) {
    snprintf(buffer, buffer_size, "?");
    return;
  }

  int delta_seconds = (int)(current_time - rx_time);
  if (delta_seconds < 0) {
    snprintf(buffer, buffer_size, "0'");
  } else {
    int minutes = delta_seconds / 60;
    if (minutes > 999) {
      snprintf(buffer, buffer_size, ">999'");
    } else {
      snprintf(buffer, buffer_size, "%d'", minutes);
    }
  }
}

void format_bg_string(char *buffer, size_t buffer_size, int32_t bg_value, bool is_mmol, bool has_data) {
  if (!buffer || buffer_size == 0) return;
  if (!has_data) {
    snprintf(buffer, buffer_size, "---");
    return;
  }
  if (is_mmol) {
    int32_t val_x10 = (bg_value * 100000 + 90091) / 180182; // round to nearest tenth using exact 18.0182 conversion ratio
    int32_t whole = val_x10 / 10;
    int32_t frac = val_x10 % 10;
    snprintf(buffer, buffer_size, "%d.%d", (int)whole, (int)frac);
  } else {
    snprintf(buffer, buffer_size, "%d", (int)bg_value);
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

// ──────────────────────────────────────────────────────────────
// Phase 2 — Pump / loop status strings
// ──────────────────────────────────────────────────────────────

void update_aaps_status(AAPSState *state,
                        const char *iob, const char *cob,
                        const char *basal, const char *iob_detail) {
  if (!state) return;
  if (iob)        { strncpy(state->iob,        iob,        sizeof(state->iob) - 1);
                    state->iob[sizeof(state->iob) - 1] = '\0'; }
  if (cob)        { strncpy(state->cob,         cob,        sizeof(state->cob) - 1);
                    state->cob[sizeof(state->cob) - 1] = '\0'; }
  if (basal)      { strncpy(state->basal,       basal,      sizeof(state->basal) - 1);
                    state->basal[sizeof(state->basal) - 1] = '\0'; }
  if (iob_detail) { strncpy(state->iob_detail,  iob_detail, sizeof(state->iob_detail) - 1);
                    state->iob_detail[sizeof(state->iob_detail) - 1] = '\0'; }
}

// ──────────────────────────────────────────────────────────────
// Phase 3 — Glucose history graph helpers
// ──────────────────────────────────────────────────────────────

void add_to_history(AAPSState *state, int32_t bg_mgdl) {
  if (!state) return;
  /* Shift everything left by 1, insert new value at index 35 */
  memmove(&state->bg_history[0], &state->bg_history[1],
          BG_HISTORY_COUNT - 1);
  uint8_t encoded = (uint8_t)((bg_mgdl / 2) & 0xFF);
  state->bg_history[BG_HISTORY_COUNT - 1] = encoded;
  if (state->history_count < BG_HISTORY_COUNT) state->history_count++;
}

void shift_history_left(AAPSState *state, int intervals) {
  if (!state || intervals <= 0) return;
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
  int range_bg = (int)(high - low);
  int32_t clamped = bg < low ? low : (bg > high ? high : bg);
  int y = y_bottom - (int)(((long)(clamped - low) * range_px) / range_bg);
  return y < y_top ? y_top : (y > y_bottom ? y_bottom : y);
}

// ──────────────────────────────────────────────────────────────
// Analog Clock helpers
// ──────────────────────────────────────────────────────────────
#define LOGIC_TRIG_MAX_ANGLE 65536

int32_t calculate_hour_angle(int hour, int minute) {
  return (LOGIC_TRIG_MAX_ANGLE * (((hour % 12) * 6) + (minute / 10))) / 72;
}

int32_t calculate_minute_angle(int minute) {
  return LOGIC_TRIG_MAX_ANGLE * minute / 60;
}
