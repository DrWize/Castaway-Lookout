#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define JCBOARD_WIDTH 800
#define JCBOARD_HEIGHT 480

typedef struct {
    esp_lcd_panel_handle_t panel;
    esp_lcd_touch_handle_t touch;
    uint16_t *framebuffer;
    uint16_t *panel_framebuffers[2];
    uint8_t next_panel_framebuffer;
    TaskHandle_t presentation_task;
    bool presented_once;
} jcboard_t;

esp_err_t jcboard_init(jcboard_t *board);
uint16_t *jcboard_framebuffer(jcboard_t *board);
esp_err_t jcboard_present(jcboard_t *board);
bool jcboard_read_touch(jcboard_t *board, uint16_t *x, uint16_t *y);
