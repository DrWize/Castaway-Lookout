#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "jcrez.h"

typedef enum {
    JCGFX_LAYOUT_LEFT,
    JCGFX_LAYOUT_CENTER,
    JCGFX_LAYOUT_RIGHT,
} jcgfx_layout_t;

typedef enum {
    JCGFX_VALIDATION_CONTROL_NONE,
    JCGFX_VALIDATION_CONTROL_PAUSE,
    JCGFX_VALIDATION_CONTROL_BACK_10,
    JCGFX_VALIDATION_CONTROL_PREVIOUS_SCENE,
    JCGFX_VALIDATION_CONTROL_NEXT_SCENE,
    JCGFX_VALIDATION_CONTROL_OK,
    JCGFX_VALIDATION_CONTROL_REVIEW,
} jcgfx_validation_control_t;

typedef enum {
    JCGFX_VALIDATION_REVIEW_UNREVIEWED,
    JCGFX_VALIDATION_REVIEW_OK,
    JCGFX_VALIDATION_REVIEW_NEEDS_REVIEW,
} jcgfx_validation_review_t;

typedef struct {
    uint8_t scene_number;
    uint8_t scene_count;
    uint32_t frame_number;
    bool paused;
    const char *title;
    const char *ads_name;
    uint16_t ads_tag;
    jcgfx_validation_review_t review;
} jcgfx_validation_sidebar_status_t;

typedef struct {
    bool time_valid;
    bool weather_available;
    bool weather_stale;
    bool weather_updated_valid;
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t weekday;
    uint8_t hour;
    uint8_t minute;
    uint8_t weather_updated_hour;
    uint8_t weather_updated_minute;
    int16_t temperature_tenths;
    int16_t high_tenths;
    int16_t low_tenths;
    uint8_t weather_code;
    const char *location;
} jcgfx_clock_weather_status_t;

esp_err_t jcgfx_render_screen(uint16_t *destination, size_t destination_width,
                              size_t destination_height, jcgfx_layout_t layout,
                              const jcrez_palette_t *palette,
                              const jcrez_screen_t *screen);
esp_err_t jcgfx_draw_bitmap_sprite(uint16_t *destination, size_t destination_width,
                                   size_t destination_height, jcgfx_layout_t layout,
                                   const jcrez_palette_t *palette,
                                   const jcrez_bitmap_t *bitmap, size_t sprite_index,
                                   int x, int y, bool flip);
esp_err_t jcgfx_draw_bitmap_sprite_clipped(
    uint16_t *destination, size_t destination_width, size_t destination_height,
    jcgfx_layout_t layout, const jcrez_palette_t *palette,
    const jcrez_bitmap_t *bitmap, size_t sprite_index, int x, int y, bool flip,
    size_t clip_x, size_t clip_y, size_t clip_width, size_t clip_height);
esp_err_t jcgfx_draw_line_clipped(
    uint16_t *destination, size_t destination_width, size_t destination_height,
    jcgfx_layout_t layout, const jcrez_palette_t *palette, uint8_t color_index,
    int x1, int y1, int x2, int y2, size_t clip_x, size_t clip_y,
    size_t clip_width, size_t clip_height);
esp_err_t jcgfx_draw_rect_clipped(
    uint16_t *destination, size_t destination_width, size_t destination_height,
    jcgfx_layout_t layout, const jcrez_palette_t *palette, uint8_t color_index,
    int x, int y, size_t width, size_t height, size_t clip_x, size_t clip_y,
    size_t clip_width, size_t clip_height);
esp_err_t jcgfx_draw_circle_clipped(
    uint16_t *destination, size_t destination_width, size_t destination_height,
    jcgfx_layout_t layout, const jcrez_palette_t *palette,
    uint8_t foreground_color_index, uint8_t background_color_index,
    int center_x, int center_y, size_t width, size_t height, size_t clip_x,
    size_t clip_y, size_t clip_width, size_t clip_height);
void jcgfx_draw_color_bars(uint16_t *destination, size_t width, size_t height);
void jcgfx_draw_cursor(uint16_t *destination, size_t width, size_t height,
                       uint16_t x, uint16_t y);
void jcgfx_draw_setup_sidebar(uint16_t *destination, size_t width,
                              size_t height, const char *ssid,
                              const char *password);
void jcgfx_clear_sidebar(uint16_t *destination, size_t width, size_t height);
void jcgfx_draw_clock_weather_sidebar(
    uint16_t *destination, size_t width, size_t height,
    const jcgfx_clock_weather_status_t *status);
esp_err_t jcgfx_verify_weather_icon_fixtures(void);
void jcgfx_draw_validation_sidebar(uint16_t *destination, size_t width,
                                   size_t height,
                                   const jcgfx_validation_sidebar_status_t *status);
void jcgfx_draw_validation_review_summary(
    uint16_t *destination, size_t width, size_t height,
    const jcgfx_validation_review_t *reviews, size_t review_count);
void jcgfx_draw_validation_all_resolved(uint16_t *destination, size_t width,
                                        size_t height);
jcgfx_validation_control_t jcgfx_validation_sidebar_hit_test(uint16_t x,
                                                              uint16_t y);
