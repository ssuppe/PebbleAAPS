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

// Trend Arrow resource ID mapping
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

  // Disassociate the old bitmap from the layer first to avoid dangling pointers
  bitmap_layer_set_bitmap(s_arrow_layer, NULL);

  // Free previous bitmap to avoid memory leaks
  if (s_arrow_bitmap) {
    gbitmap_destroy(s_arrow_bitmap);
    s_arrow_bitmap = NULL;
  }

  // Load new bitmap resource
  s_arrow_bitmap = gbitmap_create_with_resource(ARROW_RESOURCE_IDS[safe_idx]);
  
  if (s_arrow_bitmap) {
    bitmap_layer_set_bitmap(s_arrow_layer, s_arrow_bitmap);
  }
  
  // Mark layer dirty to redraw (or clear)
  layer_mark_dirty(bitmap_layer_get_layer(s_arrow_layer));
}

static void update_time() {
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  static char s_buffer[8];
  strftime(s_buffer, sizeof(s_buffer), clock_is_24h_style() ? "%H:%M" : "%I:%M", tick_time);
  text_layer_set_text(s_time_layer, s_buffer);
}

// Tick handler to keep clock and data age updating
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
  update_age_display();
}

// AppMessage Inbox Callback
static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  Tuple *bg_tuple = dict_find(iterator, MESSAGE_KEY_BG);
  Tuple *trend_tuple = dict_find(iterator, MESSAGE_KEY_TREND);
  Tuple *time_tuple = dict_find(iterator, MESSAGE_KEY_TIME);

  int32_t bg = s_state.bg_value;
  int32_t trend = s_state.trend_value;
  time_t rx_time = s_state.last_reading_time;

  if (bg_tuple && bg_tuple->type == TUPLE_INT) {
    bg = bg_tuple->value->int32;
  }
  if (trend_tuple && trend_tuple->type == TUPLE_INT) {
    trend = trend_tuple->value->int32;
  }
  if (time_tuple && time_tuple->type == TUPLE_INT) {
    rx_time = (time_t)time_tuple->value->int32;
  }

  update_aaps_state(&s_state, bg, trend, rx_time);

  update_bg_display();
  update_trend_arrow();
  update_age_display();
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "AppMessage Dropped: %d", reason);
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // System Time Layer (Top Center)
  s_time_layer = text_layer_create(GRect(0, 10, bounds.size.w, 40));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorWhite);
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_LECO_36_BOLD_NUMBERS));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));

  // Blood Glucose Layer (Middle Left)
  s_bg_layer = text_layer_create(GRect(10, 75, 120, 50));
  text_layer_set_background_color(s_bg_layer, GColorClear);
  text_layer_set_text_color(s_bg_layer, GColorWhite);
  text_layer_set_font(s_bg_layer, fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS));
  text_layer_set_text_alignment(s_bg_layer, GTextAlignmentRight);
  layer_add_child(window_layer, text_layer_get_layer(s_bg_layer));

  // Trend Arrow Layer (Middle Right)
  s_arrow_layer = bitmap_layer_create(GRect(140, 76, 48, 48));
  bitmap_layer_set_background_color(s_arrow_layer, GColorClear);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_arrow_layer));

  // Age of Reading Layer (Bottom Center)
  s_age_layer = text_layer_create(GRect(0, 165, bounds.size.w, 30));
  text_layer_set_background_color(s_age_layer, GColorClear);
  text_layer_set_text_color(s_age_layer, GColorWhite);
  text_layer_set_font(s_age_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_age_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_age_layer));

  // Perform initial rendering
  update_time();
  update_bg_display();
  update_trend_arrow();
  update_age_display();
}

static void main_window_unload(Window *window) {
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_bg_layer);
  text_layer_destroy(s_age_layer);
  bitmap_layer_destroy(s_arrow_layer);

  if (s_arrow_bitmap) {
    gbitmap_destroy(s_arrow_bitmap);
    s_arrow_bitmap = NULL;
  }
}

static void init() {
  init_aaps_state(&s_state);

  s_main_window = window_create();
  window_set_background_color(s_main_window, GColorBlack);
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });

  window_stack_push(s_main_window, true);

  // Subscribe to Tick Service for minute changes
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

  // Register AppMessage listeners
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  
  // Open AppMessage with 64 byte buffers (3 integer values requires ~33 bytes)
  app_message_open(64, 64);
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
}
