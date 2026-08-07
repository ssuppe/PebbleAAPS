#ifndef LOGIC_H
#define LOGIC_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <stddef.h>

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
