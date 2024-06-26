#include <pebble.h>
#include "omni.h"
#include "utils.h"
#include "omni_paths.c"
#include "debug_flags.h"

static Layer *s_layer_background;
static TextLayer *s_layer_date;
static TextLayer *s_layer_weather;

static GFont omni_font_time;

static GPath *s_path_green_stripes;
static GPath *s_path_mask_inner;
static GPath *s_path_stroke_inner;
static GPath *s_path_stroke_outer;

static GColor omni_color_main; //These don't actually change in 
static GColor omni_color_data; //flight, so we can keep them up here
static GColor omni_color_status; // this one does though

static bool omni_time_showing = false;

/* ---------- UI ----------*/
static GColor prv_omni_color() {
  GColor color = PAL_OMNI_STRIPES;
  #if defined(PBL_COLOR)
  if (s_settings.DoColorOverride) { color = s_settings.CustomColor; }
  #endif
  return color;
}

static GColor prv_omni_color_data() {
  GColor color = PAL_OMNI_DATA;
  #if defined(PBL_COLOR)
  if (s_settings.HighContrast) {
    color = gcolor_legible_over(prv_omni_color());
    return color;
  }

  if (s_settings.DoColorOverride) { color = gcolor_legible_over(s_settings.CustomColor); }
  #endif
  return color;
}

static GColor prv_omni_color_status() {
  GColor color = PAL_OMNI_BG;
  if (battery_state_service_peek().charge_percent <= 10) { color = PAL_OMNI_STATUS_LOWBATT; }
  if (!connection_service_peek_pebble_app_connection()) { color = PAL_OMNI_STATUS_DISCONNECT; }
  return color;
}

void omni_update_minute() {
  layer_mark_dirty(s_layer_background);
}

void omni_update_date() {
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
  static char buffer_date[16];
  strftime(buffer_date, sizeof(buffer_date), "%a%n%b%e", tick_time);
  text_layer_set_text(s_layer_date, buffer_date);
}

void omni_update_weather(int response_code) {
  // set textlayer and icon
}

void omni_update_style() {
  omni_color_status = prv_omni_color_status();
  layer_mark_dirty(s_layer_background);
}

void omni_ui_set_hidden(bool value) {
  omni_time_showing = !value;
  layer_set_hidden(text_layer_get_layer(s_layer_date), value);
  if (s_settings.DoWeather) { layer_set_hidden(text_layer_get_layer(s_layer_weather), value); }
  layer_mark_dirty(s_layer_background);
}

/* ---------- Update Procs ---------- */
static void update_proc_omni_bg(Layer *layer, GContext *ctx) {
    LOG_IF_ENABLED(DEBUG_LOG_LIFECYCLE, APP_LOG_LEVEL_INFO, "update_proc_omni_bg");
    GRect bounds = layer_get_bounds(layer);

    if (!s_path_green_stripes) { s_path_green_stripes = gpath_create(&PATH_GREEN_STRIPE); }
    if (!s_path_mask_inner) { s_path_mask_inner = gpath_create(&PATH_INNER_MASK); }
    if (!s_path_stroke_outer) {s_path_stroke_outer = gpath_create(&PATH_STRIPE_STROKE_OUTER); }
    if (!s_path_stroke_inner) {s_path_stroke_inner = gpath_create(&PATH_STRIPE_STROKE_INNER); }
    
    //Clear canvas
    graphics_context_set_fill_color(ctx, PAL_OMNI_BG);
    graphics_fill_rect(ctx, bounds, 0, GCornersAll);
    //Green carets
    graphics_context_set_fill_color(ctx, omni_color_main);
    gpath_rotate_to(s_path_green_stripes, DEG_TO_TRIGANGLE(0));
    gpath_move_to(s_path_green_stripes, GPoint(-1, 0));
    gpath_draw_filled(ctx, s_path_green_stripes);

    gpath_rotate_to(s_path_green_stripes, DEG_TO_TRIGANGLE(180));
    gpath_move_to(s_path_green_stripes, GPoint(bounds.size.w, bounds.size.h-1));
    gpath_draw_filled(ctx, s_path_green_stripes);

    // Draw time
    if (!s_settings.HideUI || omni_time_showing) {
    if (!omni_font_time) {
      omni_font_time = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_ALATSI_56));
    }
    graphics_context_set_text_color(ctx, omni_color_data);
    // Get a tm structure
    time_t temp = time(NULL);
    struct tm *tick_time = localtime(&temp);

    // Write the current hours and minutes into a buffer
    char omni_time_hour[8];
    char omni_time_minute[8];
    strftime(omni_time_hour, sizeof(omni_time_hour), clock_is_24h_style() ?
                                        "%H" : "%I", tick_time);
    strftime(omni_time_minute, sizeof(omni_time_minute), "%M", tick_time);

    LOG_IF_ENABLED(DEBUG_LOG_TIMESTRINGS, APP_LOG_LEVEL_DEBUG, "time_hour: %s", omni_time_hour);
    LOG_IF_ENABLED(DEBUG_LOG_TIMESTRINGS, APP_LOG_LEVEL_DEBUG, "time_minute: %s", omni_time_minute);

    static char oh1[2];
    static char oh2[2];
    static char om1[2];
    static char om2[2];

    strncpy(oh1, omni_time_hour+0, 1);
    strncpy(oh2, omni_time_hour+1, 1);
    strncpy(om1, omni_time_minute+0, 1);
    strncpy(om2, omni_time_minute+1, 1);
    
    graphics_draw_text(ctx, oh1, omni_font_time, BOUND_OMNI_TIME_H1, GTextOverflowModeWordWrap, GTextAlignmentCenter, 0); //Hour1
    graphics_draw_text(ctx, oh2, omni_font_time, BOUND_OMNI_TIME_H2, GTextOverflowModeWordWrap, GTextAlignmentCenter, 0); //Hour2
    graphics_draw_text(ctx, om1, omni_font_time, BOUND_OMNI_TIME_M1, GTextOverflowModeWordWrap, GTextAlignmentCenter, 0); //Minute1
    graphics_draw_text(ctx, om2, omni_font_time, BOUND_OMNI_TIME_M2, GTextOverflowModeWordWrap, GTextAlignmentCenter, 0); //Minute2
    }

    //Mask inside stripes
    graphics_context_set_fill_color(ctx, omni_color_status);
    gpath_draw_filled(ctx, s_path_mask_inner);
    
    //Caret stroke
    graphics_context_set_stroke_color(ctx, PAL_OMNI_BG);
    graphics_context_set_stroke_width(ctx, 4);
    gpath_rotate_to(s_path_stroke_inner, DEG_TO_TRIGANGLE(0));
    gpath_rotate_to(s_path_stroke_outer, DEG_TO_TRIGANGLE(0));
    gpath_move_to(s_path_stroke_outer, GPoint(-1,0));
    gpath_move_to(s_path_stroke_inner, GPoint(-1,0));
    gpath_draw_outline_open(ctx, s_path_stroke_outer);
    gpath_draw_outline_open(ctx, s_path_stroke_inner);

    gpath_rotate_to(s_path_stroke_inner, DEG_TO_TRIGANGLE(180));
    gpath_rotate_to(s_path_stroke_outer, DEG_TO_TRIGANGLE(180));
    gpath_move_to(s_path_stroke_outer, GPoint(bounds.size.w, bounds.size.h-1));
    gpath_move_to(s_path_stroke_inner, GPoint(bounds.size.w, bounds.size.h-1));
    gpath_draw_outline_open(ctx, s_path_stroke_outer);
    gpath_draw_outline_open(ctx, s_path_stroke_inner);
}

/* ---------- Life cycle ----------*/

void omni_window_load(Window *window) {
  //Store main color
  omni_color_main = prv_omni_color();
  omni_color_data = prv_omni_color_data();
  omni_color_status = prv_omni_color_status();

  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_layer_background = layer_create(bounds);
  layer_set_update_proc(s_layer_background, update_proc_omni_bg);
  layer_add_child(window_layer, s_layer_background);

  //Date layer
  s_layer_date = text_layer_create(BOUND_OMNI_DATE);
  text_layer_set_text_color(s_layer_date, omni_color_data);
  text_layer_set_background_color(s_layer_date, GColorClear);
  text_layer_set_text_alignment(s_layer_date, GTextAlignmentLeft);
  text_layer_set_font(s_layer_date, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  layer_add_child(window_layer, text_layer_get_layer(s_layer_date));
  omni_update_date();

  if (s_settings.DoWeather) {
    //Weather layer
    s_layer_weather = text_layer_create(BOUND_OMNI_WEATHER);
    text_layer_set_text_color(s_layer_weather, omni_color_data);
    text_layer_set_background_color(s_layer_weather, GColorClear);
    text_layer_set_text_alignment(s_layer_weather, GTextAlignmentRight);
    text_layer_set_font(s_layer_weather, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    text_layer_set_text(s_layer_weather, "Weather!");
    layer_add_child(window_layer, text_layer_get_layer(s_layer_weather));
  }

  omni_ui_set_hidden(s_settings.HideUI);
}

void omni_window_unload(Window *window) {
  layer_destroy(s_layer_background);
  text_layer_destroy(s_layer_date);
  text_layer_destroy(s_layer_weather);
}