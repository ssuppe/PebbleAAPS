#ifndef LOGIC_H
#define LOGIC_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <stddef.h>
#include <string.h>

// ──────────────────────────────────────────────────────────────
// Constants
// ──────────────────────────────────────────────────────────────
#define BG_HISTORY_COUNT   36
#define DEFAULT_LOW_TARGET  70
#define DEFAULT_HIGH_TARGET 180

// ──────────────────────────────────────────────────────────────
// Color state enum
// ──────────────────────────────────────────────────────────────
typedef enum {
  COLOR_STATE_NORMAL,
  COLOR_STATE_STALE
} ColorState;

// ──────────────────────────────────────────────────────────────
// AAPSState — canonical data model (persisted to flash as-is)
// ──────────────────────────────────────────────────────────────
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

// ──────────────────────────────────────────────────────────────
// Phase 1 — Core BG / trend / age logic
// ──────────────────────────────────────────────────────────────
void init_aaps_state(AAPSState *state);
bool update_aaps_state(AAPSState *state, int32_t bg, int32_t trend, time_t rx_time);
int  get_safe_trend_index(int32_t trend_value, int max_resources);
void format_age_string(char *buffer, size_t buffer_size,
                       time_t current_time, time_t rx_time, bool has_data);
void format_bg_string(char *buffer, size_t buffer_size, int32_t bg_value, bool is_mmol, bool has_data);
ColorState get_age_color_state(time_t current_time, time_t rx_time, bool has_data);

// ──────────────────────────────────────────────────────────────
// Phase 2 — Pump / loop status strings
// ──────────────────────────────────────────────────────────────
void update_aaps_status(AAPSState *state,
                        const char *iob, const char *cob,
                        const char *basal, const char *iob_detail);

// ──────────────────────────────────────────────────────────────
// Phase 3 — Glucose history graph helpers
// ──────────────────────────────────────────────────────────────
void add_to_history(AAPSState *state, int32_t bg_mgdl);
void shift_history_left(AAPSState *state, int intervals);
int  calculate_graph_y(int32_t bg, int32_t low, int32_t high,
                       int y_top, int y_bottom);

// ──────────────────────────────────────────────────────────────
// Analog Clock helpers
// ──────────────────────────────────────────────────────────────
int32_t calculate_hour_angle(int hour, int minute);
int32_t calculate_minute_angle(int minute);

#endif // LOGIC_H
