/**
 * pebble_watchface.c — UX Layer (Pebble SDK only)
 *
 * Architecture:  UX → logic.c → state_store.c
 * Layout:        Responsive layout using pebble-scalable, mapping pixel coordinates
 *                from PebbleAAPS.pfs (Emery 200x228) to scalable values.
 *                Conversion: t_perc = round(pixels * 1000 / screen_dim)
 */
#include <pebble.h>
#include <pebble-scalable/pebble-scalable.h>
#include "logic.h"
#include "state_store.h"

// ══════════════════════════════════════════════════════════════
// Global state
// ══════════════════════════════════════════════════════════════
static AAPSState s_state;

// ══════════════════════════════════════════════════════════════
// Window & layers
// ══════════════════════════════════════════════════════════════
static Window     *s_main_window;

// Phase 1
static Layer      *s_background_layer;
static Layer      *s_hands_layer;
static GPath      *s_hour_path;
static GPath      *s_minute_path;
static TextLayer  *s_bg_layer;
static TextLayer  *s_delta_layer;
static TextLayer  *s_age_layer;
static BitmapLayer *s_arrow_layer;
static GBitmap    *s_arrow_bitmap = NULL;

// Phase 2
static TextLayer  *s_iob_layer;
static TextLayer  *s_iob_detail_layer;
static TextLayer  *s_basal_layer;
static TextLayer  *s_cob_layer;
static TextLayer  *s_date_layer;

// Phase 3
static Layer      *s_graph_layer;

// ──────────────────────────────────────────────────────────────
// Design constants mapped to scalable values (t_perc = pixels * 1000 / dim)
// Emery screen dimensions: W=200, H=228
// ──────────────────────────────────────────────────────────────
#define CLOCK_CX_T 500  // 100 px -> 500
#define CLOCK_CY_T 500  // 114 px -> 500

// Graph
#define GRAPH_Y_HIGH_T 746 // 170 px -> 746
#define GRAPH_Y_LOW_T  855 // 195 px -> 855
#define GRAPH_X_START_T  50  // 10 px -> 50
#define GRAPH_X_STEP_T   25  // 5 px -> 25

// ──────────────────────────────────────────────────────────────
// Trend Arrow resource map
// ──────────────────────────────────────────────────────────────
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

// ══════════════════════════════════════════════════════════════
// Phase 1 — Update display functions
// ══════════════════════════════════════════════════════════════

static void update_bg_display() {
  static char bg_buffer[8];
  format_bg_string(bg_buffer, sizeof(bg_buffer), s_state.bg_value, s_state.is_mmol, s_state.has_data);
  text_layer_set_text(s_bg_layer, bg_buffer);
}

static void update_delta_display() {
  static char delta_buf[32];
  if (s_state.has_data && s_state.delta[0] != '\0') {
    if (s_state.avg_delta[0] != '\0') {
      snprintf(delta_buf, sizeof(delta_buf), "(%s|%s)",
               s_state.delta, s_state.avg_delta);
    } else {
      snprintf(delta_buf, sizeof(delta_buf), "%s", s_state.delta);
    }
    text_layer_set_text(s_delta_layer, delta_buf);
  } else {
    text_layer_set_text(s_delta_layer, "");
  }
}

static void update_age_display() {
  static char age_buffer[16];
  time_t now = time(NULL);
  format_age_string(age_buffer, sizeof(age_buffer),
                    now, s_state.last_reading_time, s_state.has_data);
  text_layer_set_text(s_age_layer, age_buffer);

  ColorState cs = get_age_color_state(now, s_state.last_reading_time, s_state.has_data);
  text_layer_set_text_color(s_age_layer,
                            cs == COLOR_STATE_STALE ? GColorRed : GColorWhite);
}

static void update_trend_arrow() {
  int num_res = (int)(sizeof(ARROW_RESOURCE_IDS) / sizeof(ARROW_RESOURCE_IDS[0]));
  int idx     = get_safe_trend_index(s_state.trend_value, num_res);

  bitmap_layer_set_bitmap(s_arrow_layer, NULL);
  if (s_arrow_bitmap) {
    gbitmap_destroy(s_arrow_bitmap);
    s_arrow_bitmap = NULL;
  }
  s_arrow_bitmap = gbitmap_create_with_resource(ARROW_RESOURCE_IDS[idx]);
  if (s_arrow_bitmap) {
    bitmap_layer_set_bitmap(s_arrow_layer, s_arrow_bitmap);
  }
  layer_mark_dirty(bitmap_layer_get_layer(s_arrow_layer));
}

// ══════════════════════════════════════════════════════════════
// Phase 2 — Status column display functions
// ══════════════════════════════════════════════════════════════

static void update_status_display() {
  text_layer_set_text(s_iob_layer,
                      s_state.has_data ? s_state.iob        : "");
  text_layer_set_text(s_iob_detail_layer,
                      s_state.has_data ? s_state.iob_detail : "");
  text_layer_set_text(s_basal_layer,
                      s_state.has_data ? s_state.basal      : "");
  text_layer_set_text(s_cob_layer,
                      s_state.has_data ? s_state.cob        : "");
}

static void update_date_display(struct tm *tick_time) {
  static char date_buf[12];
  strftime(date_buf, sizeof(date_buf), "%e %b", tick_time);
  text_layer_set_text(s_date_layer, date_buf);
}

// ══════════════════════════════════════════════════════════════
// Phase 1 — Background layer: clock ticks
// ══════════════════════════════════════════════════════════════

static void background_layer_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  // ── Cardinal ticks (white, strokeWidth=2, drawn as lines) ──
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 2);
  
  // 12 o'clock: from top center down 10px
  graphics_draw_line(ctx, GPoint(scl_x(CLOCK_CX_T), 0), GPoint(scl_x(CLOCK_CX_T), scl_y(44)));
  
  // 6 o'clock: from bottom center up 10px
  graphics_draw_line(ctx, GPoint(scl_x(CLOCK_CX_T), bounds.size.h - 1), GPoint(scl_x(CLOCK_CX_T), bounds.size.h - 1 - scl_y(44)));

  // 3 o'clock: from right center left 10px
  graphics_draw_line(ctx, GPoint(bounds.size.w - 1, scl_y(CLOCK_CY_T)), GPoint(bounds.size.w - 1 - scl_x(50), scl_y(CLOCK_CY_T)));

  // 9 o'clock: from left center right 10px
  graphics_draw_line(ctx, GPoint(0, scl_y(CLOCK_CY_T)), GPoint(scl_x(50), scl_y(CLOCK_CY_T)));

  // ── Diagonal corner ticks (gray #888, strokeWidth=1, angled lines) ──
  graphics_context_set_stroke_color(ctx, GColorDarkGray);
  graphics_context_set_stroke_width(ctx, 1);

  // PFS coords scaled using scalable utility
  // 1 o'clock: (166,0) -> (-5,+9) -> scaled: x=830, y=0
  graphics_draw_line(ctx, GPoint(scl_x(830), 0), GPoint(scl_x(805), scl_y(39)));
  // 2 o'clock: (200,56) -> (-9,+5) -> scaled: x=1000, y=246
  graphics_draw_line(ctx, GPoint(bounds.size.w - 1, scl_y(246)), GPoint(bounds.size.w - 1 - scl_x(45), scl_y(268)));
  // 4 o'clock: (200,172) -> (-9,-5) -> scaled: x=1000, y=754
  graphics_draw_line(ctx, GPoint(bounds.size.w - 1, scl_y(754)), GPoint(bounds.size.w - 1 - scl_x(45), scl_y(732)));
  // 5 o'clock: (166,228) -> (-5,-9) -> scaled: x=830, y=1000
  graphics_draw_line(ctx, GPoint(scl_x(830), bounds.size.h - 1), GPoint(scl_x(805), bounds.size.h - 1 - scl_y(39)));
  // 7 o'clock: (34,228) -> (+5,-9) -> scaled: x=170, y=1000
  graphics_draw_line(ctx, GPoint(scl_x(170), bounds.size.h - 1), GPoint(scl_x(195), bounds.size.h - 1 - scl_y(39)));
  // 8 o'clock: (0,172) -> (+9,-5) -> scaled: x=0, y=754
  graphics_draw_line(ctx, GPoint(0, scl_y(754)), GPoint(scl_x(45), scl_y(732)));
  // 10 o'clock: (0,56) -> (+9,+5) -> scaled: x=0, y=246
  graphics_draw_line(ctx, GPoint(0, scl_y(246)), GPoint(scl_x(45), scl_y(268)));
  // 11 o'clock: (34,0) -> (+5,+9) -> scaled: x=170, y=0
  graphics_draw_line(ctx, GPoint(scl_x(170), 0), GPoint(scl_x(195), scl_y(39)));
}

// ══════════════════════════════════════════════════════════════
// Phase 1 — Hands layer (scalable paths)
// ══════════════════════════════════════════════════════════════

// Hands relative coordinates mapping to scalable units.
// Using GPath to allow rotation around (0,0) before shifting to Center.
static const GPathInfo HOUR_HAND_INFO = {
  .num_points = 2,
  .points     = (GPoint []) { {0, 0}, {0, -38} }  // Hand length ~38px
};
static const GPathInfo MINUTE_HAND_INFO = {
  .num_points = 2,
  .points     = (GPoint []) { {0, 0}, {0, -72} }  // Hand length ~72px
};

static void hands_layer_update_proc(Layer *layer, GContext *ctx) {
  time_t now      = time(NULL);
  struct tm *tick = localtime(&now);

  GPoint center = GPoint(scl_x(CLOCK_CX_T), scl_y(CLOCK_CY_T));

  int32_t hour_angle = calculate_hour_angle(tick->tm_hour, tick->tm_min);
  int32_t min_angle  = calculate_minute_angle(tick->tm_min);

  graphics_context_set_stroke_color(ctx, GColorWhite);

  // Minute hand
  gpath_rotate_to(s_minute_path, min_angle);
  gpath_move_to(s_minute_path, center);
  graphics_context_set_stroke_width(ctx, 3);
  gpath_draw_outline(ctx, s_minute_path);

  // Hour hand
  gpath_rotate_to(s_hour_path, hour_angle);
  gpath_move_to(s_hour_path, center);
  graphics_context_set_stroke_width(ctx, 5);
  gpath_draw_outline(ctx, s_hour_path);

  // Center dot
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_circle(ctx, center, 4);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_circle(ctx, center, 2);
}

// ══════════════════════════════════════════════════════════════
// Phase 3 — Graph layer (scalable rendering)
// ══════════════════════════════════════════════════════════════

static void graph_layer_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  int y_top    = scl_y(GRAPH_Y_HIGH_T);
  int y_bottom = scl_y(GRAPH_Y_LOW_T);
  int x_start  = scl_x(GRAPH_X_START_T);
  int x_step   = scl_x(GRAPH_X_STEP_T);

  int32_t low  = s_state.low_target  ? s_state.low_target  : DEFAULT_LOW_TARGET;
  int32_t high = s_state.high_target ? s_state.high_target : DEFAULT_HIGH_TARGET;

  // 1. Dashed high-target line (gray)
  graphics_context_set_stroke_color(ctx, GColorDarkGray);
  graphics_context_set_stroke_width(ctx, 1);
  for (int x = 0; x < bounds.size.w; x += 6) {
    graphics_draw_pixel(ctx, GPoint(x, y_top));
    if (x + 1 < bounds.size.w) graphics_draw_pixel(ctx, GPoint(x + 1, y_top));
    if (x + 2 < bounds.size.w) graphics_draw_pixel(ctx, GPoint(x + 2, y_top));
  }

  // 2. Dashed low-target line (red)
  graphics_context_set_stroke_color(ctx, GColorBulgarianRose);
  for (int x = 0; x < bounds.size.w; x += 6) {
    graphics_draw_pixel(ctx, GPoint(x, y_bottom));
    if (x + 1 < bounds.size.w) graphics_draw_pixel(ctx, GPoint(x + 1, y_bottom));
    if (x + 2 < bounds.size.w) graphics_draw_pixel(ctx, GPoint(x + 2, y_bottom));
  }

  // 3. Glucose curve: green line (2px) + colored dots (4x4)
  GPoint prev   = GPoint(0, 0);
  bool has_prev = false;
  graphics_context_set_stroke_width(ctx, 2);

  for (int i = 0; i < BG_HISTORY_COUNT; i++) {
    uint8_t encoded = s_state.bg_history[i];
    if (encoded == 0) { has_prev = false; continue; }

    int32_t bg_val = (int32_t)encoded * 2;
    int x = x_start + (i * x_step);
    int y = calculate_graph_y(bg_val, low, high, y_top, y_bottom);

    GColor dot_color;
    if      (bg_val < low)  dot_color = GColorBulgarianRose;
    else if (bg_val > high) dot_color = GColorLimerick;
    else                    dot_color = GColorIslamicGreen;

    if (has_prev) {
      graphics_context_set_stroke_color(ctx, GColorIslamicGreen);
      graphics_draw_line(ctx, prev, GPoint(x, y));
    }

    graphics_context_set_fill_color(ctx, dot_color);
    graphics_fill_rect(ctx, GRect(x - 2, y - 2, 4, 4), 0, GCornerNone);

    prev     = GPoint(x, y);
    has_prev = true;
  }
}

// ══════════════════════════════════════════════════════════════
// Tick handler
// ══════════════════════════════════════════════════════════════

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_age_display();
  update_date_display(tick_time);
  layer_mark_dirty(s_hands_layer);
  if (s_state.has_data) {
    layer_mark_dirty(s_graph_layer);
  }
}

// ══════════════════════════════════════════════════════════════
// AppMessage callback
// ══════════════════════════════════════════════════════════════

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  Tuple *bg_tuple    = dict_find(iterator, MESSAGE_KEY_BG);
  Tuple *trend_tuple = dict_find(iterator, MESSAGE_KEY_TREND);
  Tuple *time_tuple  = dict_find(iterator, MESSAGE_KEY_TIME);

  int32_t bg      = s_state.bg_value;
  int32_t trend   = s_state.trend_value;
  time_t  rx_time = s_state.last_reading_time;

  if (bg_tuple    && bg_tuple->type    == TUPLE_INT)
    bg      = bg_tuple->value->int32;
  if (trend_tuple && trend_tuple->type == TUPLE_INT)
    trend   = trend_tuple->value->int32;
  if (time_tuple  && time_tuple->type  == TUPLE_INT)
    rx_time = (time_t)time_tuple->value->int32;

  update_aaps_state(&s_state, bg, trend, rx_time);

  Tuple *delta_tuple     = dict_find(iterator, MESSAGE_KEY_DELTA);
  Tuple *avg_delta_tuple = dict_find(iterator, MESSAGE_KEY_AVG_DELTA);

  if (delta_tuple && delta_tuple->type == TUPLE_CSTRING) {
    strncpy(s_state.delta, delta_tuple->value->cstring, sizeof(s_state.delta) - 1);
    s_state.delta[sizeof(s_state.delta) - 1] = '\0';
  }
  if (avg_delta_tuple && avg_delta_tuple->type == TUPLE_CSTRING) {
    strncpy(s_state.avg_delta, avg_delta_tuple->value->cstring,
            sizeof(s_state.avg_delta) - 1);
    s_state.avg_delta[sizeof(s_state.avg_delta) - 1] = '\0';
  }

  const char *iob        = NULL;
  const char *cob        = NULL;
  const char *basal      = NULL;
  const char *iob_detail = NULL;

  Tuple *iob_t   = dict_find(iterator, MESSAGE_KEY_IOB);
  Tuple *cob_t   = dict_find(iterator, MESSAGE_KEY_COB);
  Tuple *basal_t = dict_find(iterator, MESSAGE_KEY_BASAL);
  Tuple *iobd_t  = dict_find(iterator, MESSAGE_KEY_IOB_DETAIL);

  if (iob_t   && iob_t->type   == TUPLE_CSTRING) iob        = iob_t->value->cstring;
  if (cob_t   && cob_t->type   == TUPLE_CSTRING) cob        = cob_t->value->cstring;
  if (basal_t && basal_t->type == TUPLE_CSTRING) basal      = basal_t->value->cstring;
  if (iobd_t  && iobd_t->type  == TUPLE_CSTRING) iob_detail = iobd_t->value->cstring;

  update_aaps_status(&s_state, iob, cob, basal, iob_detail);

  Tuple *low_t  = dict_find(iterator, MESSAGE_KEY_LOW_TARGET);
  Tuple *high_t = dict_find(iterator, MESSAGE_KEY_HIGH_TARGET);
  Tuple *units_t = dict_find(iterator, MESSAGE_KEY_UNITS);

  if (low_t  && low_t->type  == TUPLE_INT) s_state.low_target  = low_t->value->int32;
  if (high_t && high_t->type == TUPLE_INT) s_state.high_target = high_t->value->int32;
  if (units_t && units_t->type == TUPLE_INT) {
    s_state.is_mmol = (units_t->value->int32 == 1);
  }

  Tuple *hist_t = dict_find(iterator, MESSAGE_KEY_GLUCOSE_HISTORY);
  if (hist_t && hist_t->type == TUPLE_BYTE_ARRAY &&
      hist_t->length == BG_HISTORY_COUNT) {
    memcpy(s_state.bg_history, hist_t->value->data, BG_HISTORY_COUNT);
    s_state.history_count = BG_HISTORY_COUNT;
  } else {
    time_t now = time(NULL);
    if (s_state.has_data && s_state.last_reading_time > 0) {
      int elapsed_sec = (int)(now - s_state.last_reading_time);
      int missed = (elapsed_sec - 30) / 300;
      if (missed > 1) shift_history_left(&s_state, missed - 1);
    }
    add_to_history(&s_state, bg);
  }

  state_store_save(&s_state);

  update_bg_display();
  update_delta_display();
  update_trend_arrow();
  update_age_display();
  update_status_display();
  layer_mark_dirty(s_graph_layer);
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "AppMessage Dropped: %d", reason);
}

// ══════════════════════════════════════════════════════════════
// Window load / unload (Scalable coordinate layout)
// ══════════════════════════════════════════════════════════════

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect  bounds       = layer_get_bounds(window_layer);

  // Background
  s_background_layer = layer_create(bounds);
  layer_set_update_proc(s_background_layer, background_layer_update_proc);
  layer_add_child(window_layer, s_background_layer);

  // BG: x=60 y=35 w=80 h=40 -> 60*1000/200 = 300, 35*1000/228 = 154, 80*1000/200 = 400, 40*1000/228 = 175
  s_bg_layer = text_layer_create(GRect(scl_x(300), scl_y(154), scl_x(400), scl_y(175)));
  text_layer_set_background_color(s_bg_layer, GColorClear);
  text_layer_set_text_color(s_bg_layer, GColorGreen);
  text_layer_set_font(s_bg_layer, scl_get_font(2));
  text_layer_set_text_alignment(s_bg_layer, GTextAlignmentLeft);
  layer_add_child(window_layer, text_layer_get_layer(s_bg_layer));

  // Arrow: x=142 y=45 w=20 h=20 -> 142*1000/200 = 710, 45*1000/228 = 197, 20*1000/200 = 100, 20*1000/228 = 88
  s_arrow_layer = bitmap_layer_create(GRect(scl_x(710), scl_y(197), scl_x(100), scl_y(88)));
  bitmap_layer_set_background_color(s_arrow_layer, GColorClear);
  bitmap_layer_set_compositing_mode(s_arrow_layer, GCompOpSet);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_arrow_layer));

  // Delta: x=10 y=10 w=75 h=24 -> 10*1000/200 = 50, 10*1000/228 = 44, 75*1000/200 = 375, 24*1000/228 = 105
  s_delta_layer = text_layer_create(GRect(scl_x(50), scl_y(44), scl_x(375), scl_y(105)));
  text_layer_set_background_color(s_delta_layer, GColorClear);
  text_layer_set_text_color(s_delta_layer, GColorWhite);
  text_layer_set_font(s_delta_layer, scl_get_font(0));
  layer_add_child(window_layer, text_layer_get_layer(s_delta_layer));

  // Age: x=145 y=10 w=45 h=24 -> 145*1000/200 = 725, 10*1000/228 = 44, 45*1000/200 = 225, 24*1000/228 = 105
  s_age_layer = text_layer_create(GRect(scl_x(725), scl_y(44), scl_x(225), scl_y(105)));
  text_layer_set_background_color(s_age_layer, GColorClear);
  text_layer_set_text_color(s_age_layer, GColorWhite);
  text_layer_set_font(s_age_layer, scl_get_font(0));
  text_layer_set_text_alignment(s_age_layer, GTextAlignmentRight);
  layer_add_child(window_layer, text_layer_get_layer(s_age_layer));

  // IOB: x=10 y=92 w=80 h=28 -> 10*1000/200 = 50, 92*1000/228 = 404, 80*1000/200 = 400, 28*1000/228 = 123
  s_iob_layer = text_layer_create(GRect(scl_x(50), scl_y(404), scl_x(400), scl_y(123)));
  text_layer_set_background_color(s_iob_layer, GColorClear);
  text_layer_set_text_color(s_iob_layer, GColorWhite);
  text_layer_set_font(s_iob_layer, scl_get_font(1));
  layer_add_child(window_layer, text_layer_get_layer(s_iob_layer));

  // IOB Detail: x=10 y=120 w=80 h=24 -> 10*1000/200 = 50, 120*1000/228 = 526, 80*1000/200 = 400, 24*1000/228 = 105
  s_iob_detail_layer = text_layer_create(GRect(scl_x(50), scl_y(526), scl_x(400), scl_y(105)));
  text_layer_set_background_color(s_iob_detail_layer, GColorClear);
  text_layer_set_text_color(s_iob_detail_layer, GColorWhite);
  text_layer_set_font(s_iob_detail_layer, scl_get_font(0));
  layer_add_child(window_layer, text_layer_get_layer(s_iob_detail_layer));

  // Basal: x=10 y=144 w=80 h=24 -> 10*1000/200 = 50, 144*1000/228 = 632, 80*1000/200 = 400, 24*1000/228 = 105
  s_basal_layer = text_layer_create(GRect(scl_x(50), scl_y(632), scl_x(400), scl_y(105)));
  text_layer_set_background_color(s_basal_layer, GColorClear);
  text_layer_set_text_color(s_basal_layer, GColorWhite);
  text_layer_set_font(s_basal_layer, scl_get_font(0));
  layer_add_child(window_layer, text_layer_get_layer(s_basal_layer));

  // COB: x=115 y=92 w=75 h=28 -> 115*1000/200 = 575, 92*1000/228 = 404, 75*1000/200 = 375, 28*1000/228 = 123
  s_cob_layer = text_layer_create(GRect(scl_x(575), scl_y(404), scl_x(375), scl_y(123)));
  text_layer_set_background_color(s_cob_layer, GColorClear);
  text_layer_set_text_color(s_cob_layer, GColorWhite);
  text_layer_set_font(s_cob_layer, scl_get_font(1));
  text_layer_set_text_alignment(s_cob_layer, GTextAlignmentRight);
  layer_add_child(window_layer, text_layer_get_layer(s_cob_layer));

  // Date: x=115 y=120 w=75 h=24 -> 115*1000/200 = 575, 120*1000/228 = 526, 75*1000/200 = 375, 24*1000/228 = 105
  s_date_layer = text_layer_create(GRect(scl_x(575), scl_y(526), scl_x(375), scl_y(105)));
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_color(s_date_layer, GColorWhite);
  text_layer_set_font(s_date_layer, scl_get_font(0));
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentRight);
  layer_add_child(window_layer, text_layer_get_layer(s_date_layer));

  // Graph
  s_graph_layer = layer_create(bounds);
  layer_set_update_proc(s_graph_layer, graph_layer_update_proc);
  layer_add_child(window_layer, s_graph_layer);

  // Hands
  s_hands_layer = layer_create(bounds);
  layer_set_update_proc(s_hands_layer, hands_layer_update_proc);
  layer_add_child(window_layer, s_hands_layer);

  // Init display
  time_t now = time(NULL);
  struct tm *tick = localtime(&now);
  update_bg_display();
  update_delta_display();
  update_trend_arrow();
  update_age_display();
  update_status_display();
  update_date_display(tick);
}

static void main_window_unload(Window *window) {
  layer_destroy(s_background_layer);
  layer_destroy(s_hands_layer);
  gpath_destroy(s_hour_path);
  gpath_destroy(s_minute_path);
  text_layer_destroy(s_bg_layer);
  text_layer_destroy(s_delta_layer);
  text_layer_destroy(s_age_layer);
  bitmap_layer_destroy(s_arrow_layer);
  if (s_arrow_bitmap) {
    gbitmap_destroy(s_arrow_bitmap);
    s_arrow_bitmap = NULL;
  }
  text_layer_destroy(s_iob_layer);
  text_layer_destroy(s_iob_detail_layer);
  text_layer_destroy(s_basal_layer);
  text_layer_destroy(s_cob_layer);
  text_layer_destroy(s_date_layer);
  layer_destroy(s_graph_layer);
}

// ══════════════════════════════════════════════════════════════
// Init / deinit
// ══════════════════════════════════════════════════════════════

static void init() {
  // Font styling configuration
  scl_set_fonts(0, {.o = fonts_get_system_font(FONT_KEY_GOTHIC_14),
                    .e = fonts_get_system_font(FONT_KEY_GOTHIC_18)});
  scl_set_fonts(1, {.o = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                    .e = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD)});
  scl_set_fonts(2, {.o = fonts_get_system_font(FONT_KEY_LECO_36_BOLD_NUMBERS),
                    .e = fonts_get_system_font(FONT_KEY_LECO_38_BOLD_NUMBERS)});

  init_aaps_state(&s_state);
  state_store_load(&s_state);

  s_hour_path   = gpath_create(&HOUR_HAND_INFO);
  s_minute_path = gpath_create(&MINUTE_HAND_INFO);

  s_main_window = window_create();
  window_set_background_color(s_main_window, GColorBlack);
  window_set_window_handlers(s_main_window, (WindowHandlers){
    .load   = main_window_load,
    .unload = main_window_unload
  });
  window_stack_push(s_main_window, true);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_open(512, 256);
}

static void deinit() {
  tick_timer_service_unsubscribe();
  app_message_deregister_callbacks();
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
  return 0;
}
