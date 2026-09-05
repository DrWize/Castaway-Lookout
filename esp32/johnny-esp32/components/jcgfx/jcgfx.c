#include "jcgfx.h"
#include "weather_pixel_icons.h"

#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint16_t rgb565(uint8_t red, uint8_t green, uint8_t blue)
{
    return (uint16_t)(((uint16_t)(red & 0xf8) << 8) |
                      ((uint16_t)(green & 0xfc) << 3) |
                      ((uint16_t)blue >> 3));
}

enum {
    VALIDATION_SIDEBAR_X = 640,
    VALIDATION_TEXT_X = 648,
    VALIDATION_SCENE_Y = 8,
    VALIDATION_TITLE_Y = 32,
    VALIDATION_TECHNICAL_Y = 56,
    VALIDATION_FRAME_Y = 80,
    VALIDATION_MARK_Y = 98,
    VALIDATION_PAUSE_X = 656,
    VALIDATION_PAUSE_Y = 112,
    VALIDATION_BACK_X = 656,
    VALIDATION_BACK_Y = 164,
    VALIDATION_PREVIOUS_X = 656,
    VALIDATION_PREVIOUS_Y = 216,
    VALIDATION_NEXT_X = 656,
    VALIDATION_NEXT_Y = 268,
    VALIDATION_OK_X = 656,
    VALIDATION_OK_Y = 324,
    VALIDATION_INVESTIGATE_X = 656,
    VALIDATION_INVESTIGATE_Y = 380,
    VALIDATION_BUTTON_WIDTH = 128,
    VALIDATION_BUTTON_HEIGHT = 48,
    VALIDATION_TITLE_MAX_CHARS = 18,
};

static const uint8_t *glyph_rows(char glyph)
{
    static const uint8_t digits[10][5] = {
        {0x7, 0x5, 0x5, 0x5, 0x7}, {0x2, 0x6, 0x2, 0x2, 0x7},
        {0x7, 0x1, 0x7, 0x4, 0x7}, {0x7, 0x1, 0x7, 0x1, 0x7},
        {0x5, 0x5, 0x7, 0x1, 0x1}, {0x7, 0x4, 0x7, 0x1, 0x7},
        {0x7, 0x4, 0x7, 0x5, 0x7}, {0x7, 0x1, 0x1, 0x1, 0x1},
        {0x7, 0x5, 0x7, 0x5, 0x7}, {0x7, 0x5, 0x7, 0x1, 0x7},
    };
    static const uint8_t letters[26][5] = {
        {0x2, 0x5, 0x7, 0x5, 0x5}, {0x6, 0x5, 0x6, 0x5, 0x6},
        {0x3, 0x4, 0x4, 0x4, 0x3}, {0x6, 0x5, 0x5, 0x5, 0x6},
        {0x7, 0x4, 0x6, 0x4, 0x7}, {0x7, 0x4, 0x6, 0x4, 0x4},
        {0x3, 0x4, 0x5, 0x5, 0x3}, {0x5, 0x5, 0x7, 0x5, 0x5},
        {0x7, 0x2, 0x2, 0x2, 0x7}, {0x1, 0x1, 0x1, 0x5, 0x2},
        {0x5, 0x5, 0x6, 0x5, 0x5}, {0x4, 0x4, 0x4, 0x4, 0x7},
        {0x5, 0x7, 0x7, 0x5, 0x5}, {0x5, 0x7, 0x7, 0x7, 0x5},
        {0x2, 0x5, 0x5, 0x5, 0x2}, {0x6, 0x5, 0x6, 0x4, 0x4},
        {0x2, 0x5, 0x5, 0x7, 0x3}, {0x6, 0x5, 0x6, 0x5, 0x5},
        {0x3, 0x4, 0x2, 0x1, 0x6}, {0x7, 0x2, 0x2, 0x2, 0x2},
        {0x5, 0x5, 0x5, 0x5, 0x7}, {0x5, 0x5, 0x5, 0x5, 0x2},
        {0x5, 0x5, 0x7, 0x7, 0x5}, {0x5, 0x5, 0x2, 0x5, 0x5},
        {0x5, 0x5, 0x2, 0x2, 0x2}, {0x7, 0x1, 0x2, 0x4, 0x7},
    };
    static const uint8_t minus[5] = {0x0, 0x0, 0x7, 0x0, 0x0};
    static const uint8_t plus[5] = {0x0, 0x2, 0x7, 0x2, 0x0};
    static const uint8_t period[5] = {0x0, 0x0, 0x0, 0x0, 0x2};
    static const uint8_t slash[5] = {0x1, 0x1, 0x2, 0x4, 0x4};
    static const uint8_t colon[5] = {0x0, 0x2, 0x0, 0x2, 0x0};
    static const uint8_t space[5] = {0, 0, 0, 0, 0};
    if (glyph >= '0' && glyph <= '9') return digits[glyph - '0'];
    if (glyph >= 'A' && glyph <= 'Z') return letters[glyph - 'A'];
    switch (glyph) {
    case '-': return minus;
    case '+': return plus;
    case '.': return period;
    case '/': return slash;
    case ':': return colon;
    case ' ': return space;
    default: return NULL;
    }
}

static void fill_rect(uint16_t *destination, size_t width, size_t height,
                      size_t x, size_t y, size_t rect_width,
                      size_t rect_height, uint16_t color)
{
    if (destination == NULL || x >= width || y >= height) return;
    size_t x_end = x + rect_width < width ? x + rect_width : width;
    size_t y_end = y + rect_height < height ? y + rect_height : height;
    for (size_t py = y; py < y_end; ++py) {
        for (size_t px = x; px < x_end; ++px) {
            destination[py * width + px] = color;
        }
    }
}

static void draw_glyph(uint16_t *destination, size_t width, size_t height,
                       size_t x, size_t y, char glyph, size_t scale,
                       uint16_t color)
{
    const uint8_t *rows = glyph_rows(glyph);
    if (rows == NULL) return;
    for (size_t row = 0; row < 5; ++row) {
        for (size_t column = 0; column < 3; ++column) {
            if ((rows[row] & (1U << (2U - column))) == 0) continue;
            fill_rect(destination, width, height, x + column * scale,
                      y + row * scale, scale, scale, color);
        }
    }
}

static void draw_label(uint16_t *destination, size_t width, size_t height,
                       size_t x, size_t y, const char *label, size_t scale,
                       uint16_t color)
{
    for (size_t index = 0; label[index] != '\0'; ++index) {
        draw_glyph(destination, width, height,
                   x + index * 4 * scale, y, label[index], scale, color);
    }
}

static void draw_button(uint16_t *destination, size_t width, size_t height,
                        size_t x, size_t y, const char *label, size_t label_x,
                        size_t label_scale, uint16_t color)
{
    fill_rect(destination, width, height, x, y, VALIDATION_BUTTON_WIDTH,
              VALIDATION_BUTTON_HEIGHT, color);
    fill_rect(destination, width, height, x + 2, y + 2,
              VALIDATION_BUTTON_WIDTH - 4, VALIDATION_BUTTON_HEIGHT - 4,
              rgb565(0, 0, 0));
    draw_label(destination, width, height, label_x,
               y + (VALIDATION_BUTTON_HEIGHT - 5 * label_scale) / 2,
               label, label_scale, color);
}

static void draw_wrapped_title(uint16_t *destination, size_t width,
                               size_t height, size_t x, size_t y,
                               const char *title, uint16_t color)
{
    if (title == NULL) return;
    size_t input = 0;
    for (size_t row = 0; row < 2 && title[input] != '\0'; ++row) {
        while (title[input] == ' ') ++input;
        size_t count = 0;
        size_t last_space = 0;
        while (title[input + count] != '\0' &&
               count < VALIDATION_TITLE_MAX_CHARS) {
            if (title[input + count] == ' ') last_space = count;
            ++count;
        }
        if (title[input + count] != '\0' && last_space > 0) count = last_space;
        char line[VALIDATION_TITLE_MAX_CHARS + 1] = {0};
        memcpy(line, title + input, count);
        draw_label(destination, width, height, x, y + row * 12, line, 2,
                   color);
        input += count;
    }
}

static size_t scene_x(jcgfx_layout_t layout)
{
    switch (layout) {
    case JCGFX_LAYOUT_LEFT:
        return 160;
    case JCGFX_LAYOUT_CENTER:
        return 80;
    case JCGFX_LAYOUT_RIGHT:
    default:
        return 0;
    }
}

static uint8_t dominant_bottom_index(const jcrez_screen_t *screen)
{
    unsigned counts[16] = {0};
    unsigned best_count = 0;
    uint8_t best = 0;
    if (screen->width == 0 || screen->height == 0) {
        return 0;
    }
    size_t row = (size_t)(screen->height - 1) * (screen->width / 2);
    for (uint16_t x = 0; x < screen->width; ++x) {
        uint8_t packed = screen->packed_pixels[row + x / 2];
        uint8_t index = (x & 1) == 0 ? packed >> 4 : packed & 0x0f;
        if (++counts[index] > best_count) {
            best_count = counts[index];
            best = index;
        }
    }
    return best;
}

esp_err_t jcgfx_render_screen(uint16_t *destination, size_t destination_width,
                              size_t destination_height, jcgfx_layout_t layout,
                              const jcrez_palette_t *palette,
                              const jcrez_screen_t *screen)
{
    if (destination == NULL || palette == NULL || screen == NULL ||
        screen->packed_pixels == NULL || destination_width < 800 || destination_height < 480 ||
        screen->width == 0 || screen->width > 640 || (screen->width & 1) != 0 ||
        screen->height == 0 || screen->height > 480 ||
        screen->packed_size < (size_t)screen->width * screen->height / 2) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(destination, 0, destination_width * destination_height * sizeof(*destination));
    size_t x_offset = scene_x(layout);
    uint8_t fill_index = dominant_bottom_index(screen);
    uint16_t fill = rgb565(palette->rgb[fill_index][0], palette->rgb[fill_index][1],
                           palette->rgb[fill_index][2]);
    for (size_t y = 0; y < 480; ++y) {
        for (size_t x = 0; x < 640; ++x) {
            destination[y * destination_width + x_offset + x] = fill;
        }
    }

    size_t bytes_per_row = screen->width / 2;
    for (uint16_t y = 0; y < screen->height; ++y) {
        for (uint16_t x = 0; x < screen->width; ++x) {
            uint8_t packed = screen->packed_pixels[(size_t)y * bytes_per_row + x / 2];
            uint8_t index = (x & 1) == 0 ? packed >> 4 : packed & 0x0f;
            destination[(size_t)y * destination_width + x_offset + x] =
                rgb565(palette->rgb[index][0], palette->rgb[index][1], palette->rgb[index][2]);
        }
    }
    return ESP_OK;
}

esp_err_t jcgfx_draw_bitmap_sprite(uint16_t *destination, size_t destination_width,
                                   size_t destination_height, jcgfx_layout_t layout,
                                   const jcrez_palette_t *palette,
                                   const jcrez_bitmap_t *bitmap, size_t sprite_index,
                                   int x, int y, bool flip)
{
    return jcgfx_draw_bitmap_sprite_clipped(
        destination, destination_width, destination_height, layout, palette,
        bitmap, sprite_index, x, y, flip, scene_x(layout), 0, 640, 480);
}

esp_err_t jcgfx_draw_bitmap_sprite_clipped(
    uint16_t *destination, size_t destination_width, size_t destination_height,
    jcgfx_layout_t layout, const jcrez_palette_t *palette,
    const jcrez_bitmap_t *bitmap, size_t sprite_index, int x, int y, bool flip,
    size_t clip_x, size_t clip_y, size_t clip_width, size_t clip_height)
{
    if (destination == NULL || palette == NULL || bitmap == NULL ||
        bitmap->packed_pixels == NULL || sprite_index >= bitmap->sprite_count ||
        destination_width < 800 || destination_height < 480 ||
        clip_x > destination_width || clip_y > destination_height ||
        clip_width > destination_width - clip_x ||
        clip_height > destination_height - clip_y) {
        return ESP_ERR_INVALID_ARG;
    }
    const jcrez_bitmap_sprite_t *sprite = &bitmap->sprites[sprite_index];
    size_t sprite_size = (size_t)sprite->width * sprite->height / 2U;
    if (sprite->packed_offset > bitmap->packed_size ||
        sprite_size > bitmap->packed_size - sprite->packed_offset) {
        return ESP_ERR_INVALID_SIZE;
    }
    const uint8_t *pixels = bitmap->packed_pixels + sprite->packed_offset;
    int x_offset = (int)scene_x(layout);
    for (uint16_t source_y = 0; source_y < sprite->height; ++source_y) {
        for (uint16_t source_x = 0; source_x < sprite->width; ++source_x) {
            uint8_t packed = pixels[(size_t)source_y * (sprite->width / 2U) + source_x / 2U];
            uint8_t index = (source_x & 1U) == 0 ? packed >> 4 : packed & 0x0f;
            const uint8_t *color = palette->rgb[index];
            if (color[0] == 0xa8 && color[1] == 0x00 && color[2] == 0xa8) {
                continue;
            }
            int destination_x = x_offset + x +
                (flip ? (int)sprite->width - 1 - source_x : source_x);
            int destination_y = y + source_y;
            if (destination_x < x_offset || destination_x >= x_offset + 640 ||
                destination_y < 0 || destination_y >= 480 ||
                destination_x < (int)clip_x ||
                destination_x >= (int)(clip_x + clip_width) ||
                destination_y < (int)clip_y ||
                destination_y >= (int)(clip_y + clip_height)) {
                continue;
            }
            destination[(size_t)destination_y * destination_width + destination_x] =
                rgb565(color[0], color[1], color[2]);
        }
    }
    return ESP_OK;
}

esp_err_t jcgfx_draw_line_clipped(
    uint16_t *destination, size_t destination_width, size_t destination_height,
    jcgfx_layout_t layout, const jcrez_palette_t *palette, uint8_t color_index,
    int x1, int y1, int x2, int y2, size_t clip_x, size_t clip_y,
    size_t clip_width, size_t clip_height)
{
    if (destination == NULL || palette == NULL || color_index >= 16 ||
        destination_width < 800 || destination_height < 480 ||
        clip_x > destination_width || clip_y > destination_height ||
        clip_width > destination_width - clip_x ||
        clip_height > destination_height - clip_y) {
        return ESP_ERR_INVALID_ARG;
    }

    int x_offset = (int)scene_x(layout);
    int destination_x = x_offset + x1;
    int destination_y = y1;
    int end_x = x_offset + x2;
    int end_y = y2;
    int delta_x = end_x >= destination_x ? end_x - destination_x
                                          : destination_x - end_x;
    int step_x = destination_x < end_x ? 1 : -1;
    int delta_y = end_y >= destination_y ? destination_y - end_y
                                          : end_y - destination_y;
    int step_y = destination_y < end_y ? 1 : -1;
    int error = delta_x + delta_y;
    const uint8_t *color = palette->rgb[color_index];
    uint16_t pixel = rgb565(color[0], color[1], color[2]);

    while (true) {
        if (destination_x >= x_offset && destination_x < x_offset + 640 &&
            destination_y >= 0 && destination_y < 480 &&
            destination_x >= (int)clip_x &&
            destination_x < (int)(clip_x + clip_width) &&
            destination_y >= (int)clip_y &&
            destination_y < (int)(clip_y + clip_height)) {
            destination[(size_t)destination_y * destination_width +
                        destination_x] = pixel;
        }
        if (destination_x == end_x && destination_y == end_y) break;
        int doubled_error = 2 * error;
        if (doubled_error >= delta_y) {
            error += delta_y;
            destination_x += step_x;
        }
        if (doubled_error <= delta_x) {
            error += delta_x;
            destination_y += step_y;
        }
    }
    return ESP_OK;
}

esp_err_t jcgfx_draw_rect_clipped(
    uint16_t *destination, size_t destination_width, size_t destination_height,
    jcgfx_layout_t layout, const jcrez_palette_t *palette, uint8_t color_index,
    int x, int y, size_t width, size_t height, size_t clip_x, size_t clip_y,
    size_t clip_width, size_t clip_height)
{
    if (destination == NULL || palette == NULL || color_index >= 16 ||
        destination_width < 800 || destination_height < 480 ||
        clip_x > destination_width || clip_y > destination_height ||
        clip_width > destination_width - clip_x ||
        clip_height > destination_height - clip_y) {
        return ESP_ERR_INVALID_ARG;
    }
    if (width == 0 || height == 0) return ESP_OK;

    int scene_left = (int)scene_x(layout);
    int left = scene_left + x;
    int top = y;
    int right = left + (int)width;
    int bottom = top + (int)height;
    int clip_right = (int)(clip_x + clip_width);
    int clip_bottom = (int)(clip_y + clip_height);
    if (left < scene_left) left = scene_left;
    if (top < 0) top = 0;
    if (right > scene_left + 640) right = scene_left + 640;
    if (bottom > 480) bottom = 480;
    if (left < (int)clip_x) left = (int)clip_x;
    if (top < (int)clip_y) top = (int)clip_y;
    if (right > clip_right) right = clip_right;
    if (bottom > clip_bottom) bottom = clip_bottom;
    if (left >= right || top >= bottom) return ESP_OK;

    const uint8_t *color = palette->rgb[color_index];
    uint16_t pixel = rgb565(color[0], color[1], color[2]);
    for (int destination_y = top; destination_y < bottom; ++destination_y) {
        for (int destination_x = left; destination_x < right; ++destination_x) {
            destination[(size_t)destination_y * destination_width +
                        (size_t)destination_x] = pixel;
        }
    }
    return ESP_OK;
}

static void draw_filled_circle_clipped(
    uint16_t *destination, size_t destination_width, int scene_left,
    int center_x, int center_y, int radius, uint16_t pixel, size_t clip_x,
    size_t clip_y, size_t clip_width, size_t clip_height)
{
    int clip_right = (int)(clip_x + clip_width);
    int clip_bottom = (int)(clip_y + clip_height);
    int radius_squared = radius * radius;
    for (int y = center_y - radius; y <= center_y + radius; ++y) {
        if (y < 0 || y >= 480 || y < (int)clip_y || y >= clip_bottom) continue;
        for (int x = center_x - radius; x <= center_x + radius; ++x) {
            if (x < scene_left || x >= scene_left + 640 || x < (int)clip_x ||
                x >= clip_right) {
                continue;
            }
            int dx = x - center_x;
            int dy = y - center_y;
            if (dx * dx + dy * dy <= radius_squared) {
                destination[(size_t)y * destination_width + (size_t)x] = pixel;
            }
        }
    }
}

esp_err_t jcgfx_draw_circle_clipped(
    uint16_t *destination, size_t destination_width, size_t destination_height,
    jcgfx_layout_t layout, const jcrez_palette_t *palette,
    uint8_t foreground_color_index, uint8_t background_color_index,
    int center_x, int center_y, size_t width, size_t height, size_t clip_x,
    size_t clip_y, size_t clip_width, size_t clip_height)
{
    if (destination == NULL || palette == NULL ||
        foreground_color_index >= 16 || background_color_index >= 16 ||
        destination_width < 800 || destination_height < 480 ||
        clip_x > destination_width || clip_y > destination_height ||
        clip_width > destination_width - clip_x ||
        clip_height > destination_height - clip_y || width != height ||
        width > INT_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    if (width == 0) return ESP_OK;

    int scene_left = (int)scene_x(layout);
    center_x += scene_left;
    const uint8_t *foreground = palette->rgb[foreground_color_index];
    draw_filled_circle_clipped(
        destination, destination_width, scene_left, center_x, center_y,
        (int)width, rgb565(foreground[0], foreground[1], foreground[2]),
        clip_x, clip_y, clip_width, clip_height);
    if (foreground_color_index != background_color_index) {
        const uint8_t *background = palette->rgb[background_color_index];
        draw_filled_circle_clipped(
            destination, destination_width, scene_left, center_x + 1,
            center_y + 1, (int)width,
            rgb565(background[0], background[1], background[2]), clip_x,
            clip_y, clip_width, clip_height);
    }
    return ESP_OK;
}

void jcgfx_draw_color_bars(uint16_t *destination, size_t width, size_t height)
{
    static const uint8_t colors[8][3] = {
        {255, 255, 255}, {255, 255, 0}, {0, 255, 255}, {0, 255, 0},
        {255, 0, 255}, {255, 0, 0}, {0, 0, 255}, {0, 0, 0},
    };
    if (destination == NULL || width == 0 || height == 0) {
        return;
    }
    for (size_t y = 0; y < height; ++y) {
        for (size_t x = 0; x < width; ++x) {
            size_t bar = x * 8 / width;
            destination[y * width + x] = rgb565(colors[bar][0], colors[bar][1], colors[bar][2]);
        }
    }
}

void jcgfx_draw_cursor(uint16_t *destination, size_t width, size_t height,
                       uint16_t x, uint16_t y)
{
    if (destination == NULL || x >= width || y >= height) {
        return;
    }
    uint16_t outline = rgb565(0, 0, 0);
    uint16_t center = rgb565(255, 255, 255);
    for (int delta = -8; delta <= 8; ++delta) {
        int px = (int)x + delta;
        int py = (int)y + delta;
        if (px >= 0 && (size_t)px < width) {
            if (y > 0) destination[(size_t)(y - 1) * width + px] = outline;
            destination[(size_t)y * width + px] = center;
            if ((size_t)y + 1 < height) destination[(size_t)(y + 1) * width + px] = outline;
        }
        if (py >= 0 && (size_t)py < height) {
            if (x > 0) destination[(size_t)py * width + x - 1] = outline;
            destination[(size_t)py * width + x] = center;
            if ((size_t)x + 1 < width) destination[(size_t)py * width + x + 1] = outline;
        }
    }
}

void jcgfx_draw_setup_sidebar(uint16_t *destination, size_t width,
                              size_t height, const char *ssid,
                              const char *password)
{
    if (destination == NULL || width < 800 || height < 480 || ssid == NULL ||
        password == NULL) return;
    uint16_t white = rgb565(255, 255, 255);
    uint16_t yellow = rgb565(255, 213, 74);
    fill_rect(destination, width, height, 640, 0, 160, 480, rgb565(0, 0, 0));
    draw_label(destination, width, height, 648, 20, "WIFI SETUP", 2, yellow);
    draw_label(destination, width, height, 648, 55, "WIFI NAME", 2, white);
    char upper[24] = {0};
    size_t count = strlen(ssid) < sizeof(upper) - 1
                       ? strlen(ssid)
                       : sizeof(upper) - 1;
    for (size_t index = 0; index < count; ++index) {
        char value = ssid[index];
        upper[index] = value >= 'a' && value <= 'z' ? value - 32 : value;
    }
    draw_label(destination, width, height, 648, 80, upper, 3, yellow);
    draw_label(destination, width, height, 648, 125, "PASSWORD", 2, white);
    draw_label(destination, width, height, 648, 155, password, 4, yellow);
    draw_label(destination, width, height, 648, 210, "OPEN", 2, white);
    draw_label(destination, width, height, 648, 235, "192.168.4.1", 2, yellow);
}

void jcgfx_clear_sidebar(uint16_t *destination, size_t width, size_t height)
{
    if (destination == NULL || width < 800 || height < 480) return;
    fill_rect(destination, width, height, VALIDATION_SIDEBAR_X, 0, 160, 480,
              rgb565(0, 0, 0));
}

typedef enum {
    WEATHER_ICON_CLEAR,
    WEATHER_ICON_PARTLY_CLOUDY,
    WEATHER_ICON_CLOUDY,
    WEATHER_ICON_RAIN,
    WEATHER_ICON_SNOW,
    WEATHER_ICON_STORM,
} weather_icon_kind_t;

static weather_icon_kind_t weather_icon_kind(uint8_t code)
{
    if (code == 0) return WEATHER_ICON_CLEAR;
    if (code == 1 || code == 2) return WEATHER_ICON_PARTLY_CLOUDY;
    if (code == 3 || code == 45 || code == 48) return WEATHER_ICON_CLOUDY;
    if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) {
        return WEATHER_ICON_RAIN;
    }
    if ((code >= 71 && code <= 77) || code == 85 || code == 86) {
        return WEATHER_ICON_SNOW;
    }
    if (code >= 95 && code <= 99) return WEATHER_ICON_STORM;
    return WEATHER_ICON_CLOUDY;
}

static void draw_weather_mask(uint16_t *destination, size_t width,
                              size_t height, size_t x, size_t y,
                              const uint32_t rows[32], uint16_t color)
{
    for (size_t source_y = 0; source_y < WEATHER_PIXEL_ICON_HEIGHT;
         ++source_y) {
        for (size_t source_x = 0; source_x < WEATHER_PIXEL_ICON_WIDTH;
             ++source_x) {
            if ((rows[source_y] & (UINT32_C(1) << source_x)) == 0) continue;
            fill_rect(destination, width, height,
                      x + source_x * WEATHER_PIXEL_ICON_SCALE,
                      y + source_y * WEATHER_PIXEL_ICON_SCALE,
                      WEATHER_PIXEL_ICON_SCALE, WEATHER_PIXEL_ICON_SCALE,
                      color);
        }
    }
}

static void draw_weather_icon(uint16_t *destination, size_t width,
                              size_t height, size_t x, size_t y, uint8_t code,
                              bool night, bool muted)
{
    uint16_t blue = muted ? rgb565(125, 145, 160) : rgb565(86, 190, 255);
    uint16_t yellow = muted ? rgb565(125, 145, 160) : rgb565(255, 213, 74);
    uint16_t white = muted ? rgb565(125, 145, 160) : rgb565(235, 240, 248);
    weather_icon_kind_t kind = weather_icon_kind(code);

    if (kind == WEATHER_ICON_CLEAR) {
        draw_weather_mask(destination, width, height, x, y,
                          night ? WEATHER_MOON_ROWS : WEATHER_SUN_ROWS,
                          night ? white : yellow);
        return;
    }
    if (kind == WEATHER_ICON_CLOUDY) {
        draw_weather_mask(destination, width, height, x, y,
                          WEATHER_CLOUDS_ROWS, blue);
        return;
    }

    draw_weather_mask(destination, width, height, x, y, WEATHER_CLOUD_ROWS,
                      blue);
    if (kind == WEATHER_ICON_PARTLY_CLOUDY || kind == WEATHER_ICON_RAIN ||
        kind == WEATHER_ICON_SNOW) {
        draw_weather_mask(destination, width, height, x, y,
                          night ? WEATHER_MOON_ACCENT_ROWS
                                : WEATHER_SUN_ACCENT_ROWS,
                          night ? white : yellow);
    }
    if (kind == WEATHER_ICON_RAIN) {
        draw_weather_mask(destination, width, height, x, y,
                          WEATHER_RAIN_ACCENT_ROWS, white);
    } else if (kind == WEATHER_ICON_SNOW) {
        draw_weather_mask(destination, width, height, x, y,
                          WEATHER_SNOW_ACCENT_ROWS, white);
    } else if (kind == WEATHER_ICON_STORM) {
        draw_weather_mask(destination, width, height, x, y,
                          WEATHER_STORM_RAIN_ACCENT_ROWS, white);
        draw_weather_mask(destination, width, height, x, y,
                          WEATHER_LIGHTNING_ACCENT_ROWS, yellow);
    }
}

esp_err_t jcgfx_verify_weather_icon_fixtures(void)
{
    typedef struct {
        uint8_t code;
        bool night;
        bool muted;
        uint32_t expected_hash;
    } weather_fixture_t;
    static const weather_fixture_t fixtures[] = {
        {0, false, false, 0xf6bea6e5U},
        {0, true, false, 0x2cd13fe5U},
        {2, false, false, 0x2816d4a1U},
        {3, false, false, 0xb6b541c5U},
        {45, false, false, 0xb6b541c5U},
        {61, false, false, 0xf0fb2d7dU},
        {61, true, false, 0x1bcc03d9U},
        {71, false, false, 0x60b3c33dU},
        {95, false, false, 0x5e1156a1U},
        {61, false, true, 0x8cb1e245U},
        {255, false, true, 0x31cf3045U},
        {255, false, false, 0xb6b541c5U},
    };
    const size_t pixel_count = 64U * 64U;
    uint16_t *pixels = calloc(pixel_count, sizeof(*pixels));
    if (pixels == NULL) return ESP_ERR_NO_MEM;
    esp_err_t result = ESP_OK;
    for (size_t fixture = 0; fixture < sizeof(fixtures) / sizeof(fixtures[0]);
         ++fixture) {
        memset(pixels, 0, pixel_count * sizeof(*pixels));
        draw_weather_icon(pixels, 64, 64, 0, 0, fixtures[fixture].code,
                          fixtures[fixture].night, fixtures[fixture].muted);
        uint32_t hash = UINT32_C(2166136261);
        for (size_t pixel = 0; pixel < pixel_count; ++pixel) {
            hash ^= pixels[pixel];
            hash *= UINT32_C(16777619);
        }
        if (hash != fixtures[fixture].expected_hash) {
            result = ESP_ERR_INVALID_CRC;
            break;
        }
    }
    free(pixels);
    return result;
}

static const char *weather_label(uint8_t code)
{
    if (code == 0) return "SUNNY";
    if (code <= 3) return "CLOUDY";
    if (code == 45 || code == 48) return "FOG";
    if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) {
        return "RAIN";
    }
    if ((code >= 71 && code <= 77) || code == 85 || code == 86) {
        return "SNOW";
    }
    if (code >= 95 && code <= 99) return "STORM";
    return "WEATHER";
}

static void uppercase_ascii(const char *input, char *output, size_t capacity)
{
    size_t used = 0;
    for (const unsigned char *at = (const unsigned char *)input;
         at != NULL && *at && used + 1 < capacity; ++at) {
        if (*at >= 'a' && *at <= 'z') {
            output[used++] = (char)(*at - ('a' - 'A'));
        } else if ((*at >= 'A' && *at <= 'Z') || (*at >= '0' && *at <= '9') ||
                   *at == ' ' || *at == '-' || *at == '.') {
            output[used++] = (char)*at;
        }
    }
    output[used] = '\0';
}

void jcgfx_draw_clock_weather_sidebar(
    uint16_t *destination, size_t width, size_t height,
    const jcgfx_clock_weather_status_t *status)
{
    if (destination == NULL || width < 800 || height < 480 || status == NULL ||
        status->location == NULL) return;
    static const char *const weekdays[] = {
        "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT",
    };
    static const char *const months[] = {
        "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
        "JUL", "AUG", "SEP", "OCT", "NOV", "DEC",
    };
    uint16_t white = rgb565(235, 240, 248);
    uint16_t blue = rgb565(86, 190, 255);
    uint16_t yellow = rgb565(255, 213, 74);
    uint16_t muted = rgb565(125, 145, 160);
    jcgfx_clear_sidebar(destination, width, height);
    char line[32] = {0};
    if (status->time_valid && status->weekday < 7 && status->month >= 1 &&
        status->month <= 12) {
        snprintf(line, sizeof(line), "%02u:%02u", status->hour,
                 status->minute);
        draw_label(destination, width, height, 650, 18, line, 7, yellow);
        snprintf(line, sizeof(line), "%s %02u %s",
                 weekdays[status->weekday], status->day,
                 months[status->month - 1]);
        draw_label(destination, width, height, 650, 66, line, 2, white);
    } else {
        draw_label(destination, width, height, 650, 18, "--:--", 7, muted);
        draw_label(destination, width, height, 650, 66, "SYNCING TIME", 2,
                   muted);
    }
    char location[40] = {0};
    uppercase_ascii(status->location, location, sizeof(location));
    draw_wrapped_title(destination, width, height, 648, 96, location, blue);
    fill_rect(destination, width, height, 648, 132, 144, 2, muted);
    if (!status->weather_available) {
        draw_weather_icon(destination, width, height, 688, 142, 255, false,
                          true);
        draw_label(destination, width, height, 650, 216, "WAITING FOR", 2, muted);
        draw_label(destination, width, height, 666, 244, "WEATHER", 3, white);
        return;
    }
    bool night = status->time_valid && (status->hour < 6 || status->hour >= 18);
    draw_weather_icon(destination, width, height, 688, 142,
                      status->weather_code, night, status->weather_stale);
    const char *condition = weather_label(status->weather_code);
    size_t condition_scale = strlen(condition) > 6 ? 2 : 3;
    size_t condition_width = strlen(condition) * 4 * condition_scale;
    draw_label(destination, width, height,
               640 + (160 - condition_width) / 2, 212, condition,
               condition_scale, status->weather_stale ? muted : blue);
    int temperature = status->temperature_tenths;
    snprintf(line, sizeof(line), "%s%d.%d C", temperature < 0 ? "-" : "",
             abs(temperature) / 10, abs(temperature) % 10);
    draw_label(destination, width, height, 650, 240, line, 4, yellow);
    int high = status->high_tenths;
    int low = status->low_tenths;
    snprintf(line, sizeof(line), "HIGH %s%d.%d C", high < 0 ? "-" : "",
             abs(high) / 10, abs(high) % 10);
    draw_label(destination, width, height, 650, 286, line, 2, white);
    snprintf(line, sizeof(line), "LOW  %s%d.%d C", low < 0 ? "-" : "",
             abs(low) / 10, abs(low) % 10);
    draw_label(destination, width, height, 650, 308, line, 2, white);
    bool updated_valid = status->time_valid && status->weather_updated_valid;
    draw_label(destination, width, height, 650, 362,
               updated_valid ? "LAST UPDATED" : "SAVED WEATHER", 2, muted);
    if (updated_valid) {
        snprintf(line, sizeof(line), "%02u:%02u",
                 status->weather_updated_hour, status->weather_updated_minute);
        draw_label(destination, width, height, 650, 382, line, 2, muted);
    } else {
        draw_label(destination, width, height, 650, 382, "TIME UNKNOWN", 2, muted);
    }
    draw_label(destination, width, height, 650, 402, "DATA FROM METEO", 2,
               muted);
}

static const char *review_label(jcgfx_validation_review_t review)
{
    switch (review) {
    case JCGFX_VALIDATION_REVIEW_OK:
        return "OK";
    case JCGFX_VALIDATION_REVIEW_NEEDS_REVIEW:
        return "REVIEW";
    default:
        return "UNREVIEWED";
    }
}

void jcgfx_draw_validation_sidebar(uint16_t *destination, size_t width,
                                   size_t height,
                                   const jcgfx_validation_sidebar_status_t *status)
{
    if (destination == NULL || width < 800 || height < 480 ||
        status == NULL || status->scene_number < 1 ||
        status->scene_count == 0 ||
        status->scene_number > status->scene_count || status->title == NULL ||
        status->ads_name == NULL) {
        return;
    }
    char scene_label[16] = {0};
    char technical_label[32] = {0};
    char frame_label[24] = {0};
    char mark_label[24] = {0};
    snprintf(scene_label, sizeof(scene_label), "SCENE %u/%u",
             status->scene_number, status->scene_count);
    snprintf(technical_label, sizeof(technical_label), "%s TAG %u",
             status->ads_name, status->ads_tag);
    snprintf(frame_label, sizeof(frame_label), "FRAME %04" PRIu32,
             status->frame_number);
    snprintf(mark_label, sizeof(mark_label), "MARK %s",
             review_label(status->review));
    uint16_t white = rgb565(255, 255, 255);
    uint16_t yellow = rgb565(255, 255, 0);
    fill_rect(destination, width, height, VALIDATION_SIDEBAR_X, 0, 160, 480,
              rgb565(0, 0, 0));
    draw_label(destination, width, height, VALIDATION_TEXT_X,
               VALIDATION_SCENE_Y, scene_label, 2, yellow);
    draw_wrapped_title(destination, width, height, VALIDATION_TEXT_X,
                       VALIDATION_TITLE_Y, status->title, white);
    draw_label(destination, width, height, VALIDATION_TEXT_X,
               VALIDATION_TECHNICAL_Y, technical_label, 2, white);
    draw_label(destination, width, height, VALIDATION_TEXT_X,
               VALIDATION_FRAME_Y, frame_label, 2, yellow);
    if (status->review != JCGFX_VALIDATION_REVIEW_UNREVIEWED) {
        draw_label(destination, width, height, VALIDATION_TEXT_X,
                   VALIDATION_MARK_Y, mark_label, 1, yellow);
    }
    draw_button(destination, width, height, VALIDATION_PAUSE_X,
                VALIDATION_PAUSE_Y, status->paused ? "PLAY" : "PAUSE",
                status->paused ? 696 : 686, 3,
                status->paused ? yellow : white);
    draw_button(destination, width, height, VALIDATION_BACK_X,
                VALIDATION_BACK_Y, "FRAME -10", 666, 3, white);
    draw_button(destination, width, height, VALIDATION_PREVIOUS_X,
                VALIDATION_PREVIOUS_Y, "SCENE -1", 674, 3, white);
    draw_button(destination, width, height, VALIDATION_NEXT_X,
                VALIDATION_NEXT_Y, "SCENE +1", 674, 3, white);
    draw_button(destination, width, height, VALIDATION_OK_X,
                VALIDATION_OK_Y, "OK", 708, 3, yellow);
    draw_button(destination, width, height, VALIDATION_INVESTIGATE_X,
                VALIDATION_INVESTIGATE_Y, "REVIEW", 690, 3, white);
}

void jcgfx_draw_validation_review_summary(
    uint16_t *destination, size_t width, size_t height,
    const jcgfx_validation_review_t *reviews, size_t review_count)
{
    if (destination == NULL || width < 800 || height < 480 || reviews == NULL ||
        review_count == 0 || review_count > 99) {
        return;
    }
    uint16_t white = rgb565(255, 255, 255);
    uint16_t yellow = rgb565(255, 255, 0);
    size_t ok_count = 0;
    size_t review_count_total = 0;
    size_t unreviewed_count = 0;
    for (size_t index = 0; index < review_count; ++index) {
        if (reviews[index] == JCGFX_VALIDATION_REVIEW_OK) {
            ++ok_count;
        } else if (reviews[index] ==
                   JCGFX_VALIDATION_REVIEW_NEEDS_REVIEW) {
            ++review_count_total;
        } else {
            ++unreviewed_count;
        }
    }
    fill_rect(destination, width, height, VALIDATION_SIDEBAR_X, 0, 160, 480,
              rgb565(0, 0, 0));
    draw_label(destination, width, height, VALIDATION_TEXT_X,
               VALIDATION_SCENE_Y, "REVIEW COMPLETE", 2, yellow);
    char count_label[24] = {0};
    snprintf(count_label, sizeof(count_label), "OK %02u", (unsigned)ok_count);
    draw_label(destination, width, height, VALIDATION_TEXT_X, 40,
               count_label, 2, white);
    snprintf(count_label, sizeof(count_label), "REVIEW %02u",
             (unsigned)review_count_total);
    draw_label(destination, width, height, VALIDATION_TEXT_X, 60,
               count_label, 2, review_count_total > 0 ? yellow : white);
    snprintf(count_label, sizeof(count_label), "LEFT %02u",
             (unsigned)unreviewed_count);
    draw_label(destination, width, height, VALIDATION_TEXT_X, 80,
               count_label, 2, unreviewed_count > 0 ? yellow : white);
    draw_label(destination, width, height, VALIDATION_TEXT_X, 104,
               "REVIEW SCENES", 1, white);
    if (review_count_total == 0) {
        draw_label(destination, width, height, VALIDATION_TEXT_X, 120,
                   "NONE", 1, white);
    } else {
        char line[36] = {0};
        size_t line_length = 0;
        size_t line_number = 0;
        for (size_t index = 0; index < review_count; ++index) {
            if (reviews[index] != JCGFX_VALIDATION_REVIEW_NEEDS_REVIEW) {
                continue;
            }
            char number[5] = {0};
            snprintf(number, sizeof(number), "%02u ", (unsigned)(index + 1));
            if (line_length + 3 >= sizeof(line)) {
                draw_label(destination, width, height, VALIDATION_TEXT_X,
                           120 + line_number * 12, line, 1, yellow);
                memset(line, 0, sizeof(line));
                line_length = 0;
                ++line_number;
            }
            memcpy(line + line_length, number, 3);
            line_length += 3;
        }
        if (line_length > 0) {
            draw_label(destination, width, height, VALIDATION_TEXT_X,
                       120 + line_number * 12, line, 1, yellow);
        }
    }
    draw_button(destination, width, height, VALIDATION_PREVIOUS_X,
                VALIDATION_PREVIOUS_Y, "SCENE -1", 674, 3, white);
    draw_button(destination, width, height, VALIDATION_NEXT_X,
                VALIDATION_NEXT_Y, "SCENE +1", 674, 3, white);
}

void jcgfx_draw_validation_all_resolved(uint16_t *destination, size_t width,
                                        size_t height)
{
    if (destination == NULL || width < 800 || height < 480) return;
    fill_rect(destination, width, height, 640, 0, 160, 480, rgb565(8, 12, 20));
    uint16_t white = rgb565(235, 240, 248);
    uint16_t green = rgb565(80, 230, 130);
    draw_label(destination, width, height, VALIDATION_TEXT_X,
               VALIDATION_SCENE_Y, "ALL RESOLVED", 2, green);
    draw_label(destination, width, height, VALIDATION_TEXT_X,
               VALIDATION_TITLE_Y, "REVIEW 00", 2, white);
    draw_label(destination, width, height, VALIDATION_TEXT_X,
               VALIDATION_TECHNICAL_Y, "SHORTLIST EMPTY", 1, white);
}

jcgfx_validation_control_t jcgfx_validation_sidebar_hit_test(uint16_t x,
                                                              uint16_t y)
{
    if (x >= VALIDATION_PAUSE_X &&
        x < VALIDATION_PAUSE_X + VALIDATION_BUTTON_WIDTH &&
        y >= VALIDATION_PAUSE_Y &&
        y < VALIDATION_PAUSE_Y + VALIDATION_BUTTON_HEIGHT) {
        return JCGFX_VALIDATION_CONTROL_PAUSE;
    }
    if (x >= VALIDATION_BACK_X &&
        x < VALIDATION_BACK_X + VALIDATION_BUTTON_WIDTH &&
        y >= VALIDATION_BACK_Y &&
        y < VALIDATION_BACK_Y + VALIDATION_BUTTON_HEIGHT) {
        return JCGFX_VALIDATION_CONTROL_BACK_10;
    }
    if (x >= VALIDATION_PREVIOUS_X &&
        x < VALIDATION_PREVIOUS_X + VALIDATION_BUTTON_WIDTH &&
        y >= VALIDATION_PREVIOUS_Y &&
        y < VALIDATION_PREVIOUS_Y + VALIDATION_BUTTON_HEIGHT) {
        return JCGFX_VALIDATION_CONTROL_PREVIOUS_SCENE;
    }
    if (x >= VALIDATION_NEXT_X &&
        x < VALIDATION_NEXT_X + VALIDATION_BUTTON_WIDTH &&
        y >= VALIDATION_NEXT_Y &&
        y < VALIDATION_NEXT_Y + VALIDATION_BUTTON_HEIGHT) {
        return JCGFX_VALIDATION_CONTROL_NEXT_SCENE;
    }
    if (x >= VALIDATION_OK_X &&
        x < VALIDATION_OK_X + VALIDATION_BUTTON_WIDTH &&
        y >= VALIDATION_OK_Y &&
        y < VALIDATION_OK_Y + VALIDATION_BUTTON_HEIGHT) {
        return JCGFX_VALIDATION_CONTROL_OK;
    }
    if (x >= VALIDATION_INVESTIGATE_X &&
        x < VALIDATION_INVESTIGATE_X + VALIDATION_BUTTON_WIDTH &&
        y >= VALIDATION_INVESTIGATE_Y &&
        y < VALIDATION_INVESTIGATE_Y + VALIDATION_BUTTON_HEIGHT) {
        return JCGFX_VALIDATION_CONTROL_REVIEW;
    }
    return JCGFX_VALIDATION_CONTROL_NONE;
}
