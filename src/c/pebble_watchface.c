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

static GFont      s_font_lilita_22;
static GFont      s_font_lilita_32;
static GFont      s_font_lilita_48;

// ──────────────────────────────────────────────────────────────
// Design constants mapped to scalable values (t_perc = pixels * 1000 / dim)
// Emery screen dimensions: W=200, H=228
// ──────────────────────────────────────────────────────────────
#define CLOCK_CX_T 500  // 100 px -> 500
#define CLOCK_CY_T 500  // 114 px -> 500

// Graph
#define GRAPH_Y_HIGH_T 833 // 190 px
#define GRAPH_Y_LOW_T  930 // 212 px
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

  // Dynamically position the arrow layer right after the BG text
  GSize size = text_layer_get_content_size(s_bg_layer);
  GRect bg_frame = layer_get_frame(text_layer_get_layer(s_bg_layer));
  GRect arrow_frame = layer_get_frame(bitmap_layer_get_layer(s_arrow_layer));

  // Set the Arrow's X to be right after the text size, with a small gap (e.g. 5px / 25 thousandths)
  int gap = scl_x(25); // 5px gap
  arrow_frame.origin.x = bg_frame.origin.x + size.w + gap;
  layer_set_frame(bitmap_layer_get_layer(s_arrow_layer), arrow_frame);
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
                            cs == COLOR_STATE_STALE ? GColorBulgarianRose : GColorBlack);
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

// Hands relative coordinates mapping to scalable units.
// Using GPath to allow rotation around (0,0) before shifting to Center.
static const GPathInfo HOUR_HAND_INFO = {
  .num_points = 2,
  .points     = (GPoint []) { {0, 0}, {0, -48} }  // Hand length ~48px (was 38)
};
static const GPathInfo MINUTE_HAND_INFO = {
  .num_points = 2,
  .points     = (GPoint []) { {0, 0}, {0, -85} }  // Hand length ~85px (was 72)
};

static void hands_layer_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  // ── Dial Ticks (drawn under the hands, but on top of the graph) ──
  // Cardinal ticks (black, strokeWidth=2, drawn as lines)
  graphics_context_set_stroke_color(ctx, GColorBlack);
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

  // 1 o'clock: scaled: x=830, y=0
  graphics_draw_line(ctx, GPoint(scl_x(830), 0), GPoint(scl_x(805), scl_y(39)));
  // 2 o'clock: scaled: x=1000, y=246
  graphics_draw_line(ctx, GPoint(bounds.size.w - 1, scl_y(246)), GPoint(bounds.size.w - 1 - scl_x(45), scl_y(268)));
  // 4 o'clock: scaled: x=1000, y=754
  graphics_draw_line(ctx, GPoint(bounds.size.w - 1, scl_y(754)), GPoint(bounds.size.w - 1 - scl_x(45), scl_y(732)));
  // 5 o'clock: scaled: x=830, y=1000
  graphics_draw_line(ctx, GPoint(scl_x(830), bounds.size.h - 1), GPoint(scl_x(805), bounds.size.h - 1 - scl_y(39)));
  // 7 o'clock: scaled: x=170, y=1000
  graphics_draw_line(ctx, GPoint(scl_x(170), bounds.size.h - 1), GPoint(scl_x(195), bounds.size.h - 1 - scl_y(39)));
  // 8 o'clock: scaled: x=0, y=754
  graphics_draw_line(ctx, GPoint(0, scl_y(754)), GPoint(scl_x(45), scl_y(732)));
  // 10 o'clock: scaled: x=0, y=246
  graphics_draw_line(ctx, GPoint(0, scl_y(246)), GPoint(scl_x(45), scl_y(268)));
  // 11 o'clock: scaled: x=170, y=0
  graphics_draw_line(ctx, GPoint(scl_x(170), 0), GPoint(scl_x(195), scl_y(39)));

  // ── Hands Drawing ──
  time_t now      = time(NULL);
  struct tm *tick = localtime(&now);

  GPoint center = GPoint(scl_x(CLOCK_CX_T), scl_y(CLOCK_CY_T));

  int32_t hour_angle = calculate_hour_angle(tick->tm_hour, tick->tm_min);
  int32_t min_angle  = calculate_minute_angle(tick->tm_min);

  graphics_context_set_stroke_color(ctx, GColorCobaltBlue);

  // Minute hand
  gpath_rotate_to(s_minute_path, min_angle);
  gpath_move_to(s_minute_path, center);
  graphics_context_set_stroke_width(ctx, 4); // Thicker (was 3)
  gpath_draw_outline(ctx, s_minute_path);

  // Hour hand
  gpath_rotate_to(s_hour_path, hour_angle);
  gpath_move_to(s_hour_path, center);
  graphics_context_set_stroke_width(ctx, 7); // Thicker (was 5)
  gpath_draw_outline(ctx, s_hour_path);

  // Center dot
  graphics_context_set_fill_color(ctx, GColorCobaltBlue);
  graphics_fill_circle(ctx, center, 5); // Larger dot for thicker hands
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
    else if (bg_val > high) dot_color = GColorOrange;
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

  // BG: (5, 36, 110, 52) -> 5*1000/200 = 25, 36*1000/228 = 158, 110*1000/200 = 550, 52*1000/228 = 228
  s_bg_layer = text_layer_create(GRect(scl_x(25), scl_y(158), scl_x(550), scl_y(228)));
  text_layer_set_background_color(s_bg_layer, GColorClear);
  text_layer_set_text_color(s_bg_layer, GColorIslamicGreen);
  text_layer_set_font(s_bg_layer, scl_get_font(2));
  text_layer_set_text_alignment(s_bg_layer, GTextAlignmentLeft);
  layer_add_child(window_layer, text_layer_get_layer(s_bg_layer));

  // Arrow: (30, 42, 36, 36) -> 30*1000/200 = 150, 42*1000/228 = 184, 36*1000/200 = 180, 36*1000/228 = 158
  // Initial frame; will be dynamically adjusted in update_bg_display()
  // Arrow: (30, 42, 36, 36) -> 30*1000/200 = 150, 42*1000/228 = 184, 36*1000/200 = 180, 36*1000/228 = 158
  // Initial frame; will be dynamically adjusted in update_bg_display()
  s_arrow_layer = bitmap_layer_create(GRect(scl_x(150), scl_y(184), scl_x(180), scl_y(158)));
  bitmap_layer_set_background_color(s_arrow_layer, GColorClear);
  bitmap_layer_set_compositing_mode(s_arrow_layer, GCompOpSet);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_arrow_layer));

  // Delta: x=10 y=10 w=125 h=27 -> 10*1000/200 = 50, 10*1000/228 = 44, 125*1000/200 = 625, 27*1000/228 = 118
  s_delta_layer = text_layer_create(GRect(scl_x(50), scl_y(44), scl_x(625), scl_y(118)));
  text_layer_set_background_color(s_delta_layer, GColorClear);
  text_layer_set_text_color(s_delta_layer, GColorBlack);
  text_layer_set_font(s_delta_layer, scl_get_font(4)); // Bold Gothic 18 (index 4)
  layer_add_child(window_layer, text_layer_get_layer(s_delta_layer));

  // Age: x=130 y=10 w=60 h=27 -> 130*1000/200 = 650, 10*1000/228 = 44, 60*1000/200 = 300, 27*1000/228 = 118
  s_age_layer = text_layer_create(GRect(scl_x(650), scl_y(44), scl_x(300), scl_y(118)));
  text_layer_set_background_color(s_age_layer, GColorClear);
  text_layer_set_text_color(s_age_layer, GColorBlack);
  text_layer_set_font(s_age_layer, scl_get_font(4)); // Bold Gothic 18 (index 4)
  text_layer_set_text_alignment(s_age_layer, GTextAlignmentRight);
  layer_add_child(window_layer, text_layer_get_layer(s_age_layer));

  // IOB: (5, 88, 115, 40) -> 5*1000/200 = 25, 88*1000/228 = 386, 115*1000/200 = 575, 40*1000/228 = 175
  s_iob_layer = text_layer_create(GRect(scl_x(25), scl_y(386), scl_x(575), scl_y(175)));
  text_layer_set_background_color(s_iob_layer, GColorClear);
  text_layer_set_text_color(s_iob_layer, GColorBlack);
  text_layer_set_font(s_iob_layer, scl_get_font(1));
  layer_add_child(window_layer, text_layer_get_layer(s_iob_layer));

  // IOB Detail: (5, 160, 190, 30) -> 5*1000/200 = 25, 160*1000/228 = 702, 190*1000/200 = 950, 30*1000/228 = 132
  s_iob_detail_layer = text_layer_create(GRect(scl_x(25), scl_y(702), scl_x(950), scl_y(132)));
  text_layer_set_background_color(s_iob_detail_layer, GColorClear);
  text_layer_set_text_color(s_iob_detail_layer, GColorDarkGray);
  text_layer_set_font(s_iob_detail_layer, scl_get_font(3)); // Bold Lilita 22
  layer_add_child(window_layer, text_layer_get_layer(s_iob_detail_layer));

  // Basal: (5, 128, 115, 32) -> 5*1000/200 = 25, 128*1000/228 = 561, 115*1000/200 = 575, 32*1000/228 = 140
  s_basal_layer = text_layer_create(GRect(scl_x(25), scl_y(561), scl_x(575), scl_y(140)));
  text_layer_set_background_color(s_basal_layer, GColorClear);
  text_layer_set_text_color(s_basal_layer, GColorDarkGray);
  text_layer_set_font(s_basal_layer, scl_get_font(3)); // Bold Lilita 22
  layer_add_child(window_layer, text_layer_get_layer(s_basal_layer));

  // COB: (125, 88, 70, 40) -> 125*1000/200 = 625, 88*1000/228 = 386, 70*1000/200 = 350, 40*1000/228 = 175
  s_cob_layer = text_layer_create(GRect(scl_x(625), scl_y(386), scl_x(350), scl_y(175)));
  text_layer_set_background_color(s_cob_layer, GColorClear);
  text_layer_set_text_color(s_cob_layer, GColorBlack);
  text_layer_set_font(s_cob_layer, scl_get_font(1));
  text_layer_set_text_alignment(s_cob_layer, GTextAlignmentRight);
  layer_add_child(window_layer, text_layer_get_layer(s_cob_layer));

  // Date: (125, 128, 70, 32) -> 125*1000/200 = 625, 128*1000/228 = 561, 70*1000/200 = 350, 32*1000/228 = 140
  s_date_layer = text_layer_create(GRect(scl_x(625), scl_y(561), scl_x(350), scl_y(140)));
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_color(s_date_layer, GColorDarkGray);
  text_layer_set_font(s_date_layer, scl_get_font(3)); // Bold Lilita 22
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
  // Load custom chunky fonts
  s_font_lilita_22 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_LILITA_22));
  s_font_lilita_32 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_LILITA_32));
  s_font_lilita_48 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_LILITA_48));

  // Font styling configuration using Lilita One
  scl_set_fonts(0, {.o = s_font_lilita_22, .e = s_font_lilita_22});
  scl_set_fonts(1, {.o = s_font_lilita_32, .e = s_font_lilita_32});
  scl_set_fonts(2, {.o = s_font_lilita_48, .e = s_font_lilita_48});
  scl_set_fonts(3, {.o = s_font_lilita_22, .e = s_font_lilita_22});
  scl_set_fonts(4, {.o = s_font_lilita_22, .e = s_font_lilita_22});

  init_aaps_state(&s_state);
  state_store_load(&s_state);

  s_hour_path   = gpath_create(&HOUR_HAND_INFO);
  s_minute_path = gpath_create(&MINUTE_HAND_INFO);

  s_main_window = window_create();
  window_set_background_color(s_main_window, GColorWhite);
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

  // Unload custom fonts to release heap memory
  fonts_unload_custom_font(s_font_lilita_22);
  fonts_unload_custom_font(s_font_lilita_32);
  fonts_unload_custom_font(s_font_lilita_48);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
  return 0;
}
