#include "jcboard.h"

#include <string.h>

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_log.h"
#include "esp_rom_sys.h"

static const char *TAG = "jcboard";

static bool IRAM_ATTR frame_buffer_complete(
    esp_lcd_panel_handle_t panel,
    const esp_lcd_rgb_panel_event_data_t *event_data, void *user_context)
{
    (void)panel;
    (void)event_data;
    jcboard_t *board = user_context;
    BaseType_t task_woken = pdFALSE;
    if (board != NULL && board->presentation_task != NULL) {
        vTaskNotifyGiveFromISR(board->presentation_task, &task_woken);
    }
    return task_woken == pdTRUE;
}

enum {
    I2C_PORT = I2C_NUM_0,
    I2C_SDA = 8,
    I2C_SCL = 9,
    TOUCH_RESET_GPIO = 4,
};

static esp_err_t i2c_init(void)
{
    const i2c_config_t config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA,
        .scl_io_num = I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    ESP_RETURN_ON_ERROR(i2c_param_config(I2C_PORT, &config), TAG, "configure I2C");
    esp_err_t err = i2c_driver_install(I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);
    return err == ESP_ERR_INVALID_STATE ? ESP_OK : err;
}

static esp_err_t ch422g_write(uint8_t address, uint8_t value)
{
    return i2c_master_write_to_device(I2C_PORT, address, &value, 1, pdMS_TO_TICKS(1000));
}

static esp_err_t reset_touch_and_enable_backlight(void)
{
    const gpio_config_t gpio = {
        .pin_bit_mask = 1ULL << TOUCH_RESET_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&gpio), TAG, "configure touch reset");

    ESP_RETURN_ON_ERROR(ch422g_write(0x24, 0x01), TAG, "enable CH422G outputs");
    ESP_RETURN_ON_ERROR(ch422g_write(0x38, 0x2c), TAG, "assert touch reset");
    esp_rom_delay_us(100000);
    ESP_RETURN_ON_ERROR(gpio_set_level(TOUCH_RESET_GPIO, 0), TAG, "select GT911 address");
    esp_rom_delay_us(100000);
    ESP_RETURN_ON_ERROR(ch422g_write(0x38, 0x2e), TAG, "release touch reset and enable backlight");
    esp_rom_delay_us(200000);
    return ESP_OK;
}

static esp_err_t init_panel(jcboard_t *board)
{
    const esp_lcd_rgb_panel_config_t config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .timings = {
            .pclk_hz = 16 * 1000 * 1000,
            .h_res = JCBOARD_WIDTH,
            .v_res = JCBOARD_HEIGHT,
            .hsync_pulse_width = 4,
            .hsync_back_porch = 8,
            .hsync_front_porch = 8,
            .vsync_pulse_width = 4,
            .vsync_back_porch = 8,
            .vsync_front_porch = 8,
            .flags.pclk_active_neg = 1,
        },
        .data_width = 16,
        .bits_per_pixel = 16,
        .num_fbs = 2,
        .bounce_buffer_size_px = JCBOARD_WIDTH * 10 * 16 / 8,
        .sram_trans_align = 4,
        .psram_trans_align = 64,
        .hsync_gpio_num = GPIO_NUM_46,
        .vsync_gpio_num = GPIO_NUM_3,
        .de_gpio_num = GPIO_NUM_5,
        .pclk_gpio_num = GPIO_NUM_7,
        .disp_gpio_num = GPIO_NUM_NC,
        .data_gpio_nums = {
            GPIO_NUM_14, GPIO_NUM_38, GPIO_NUM_18, GPIO_NUM_17,
            GPIO_NUM_10, GPIO_NUM_39, GPIO_NUM_0, GPIO_NUM_45,
            GPIO_NUM_48, GPIO_NUM_47, GPIO_NUM_21, GPIO_NUM_1,
            GPIO_NUM_2, GPIO_NUM_42, GPIO_NUM_41, GPIO_NUM_40,
        },
        .flags.fb_in_psram = 1,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_rgb_panel(&config, &board->panel), TAG, "create RGB panel");
    board->presentation_task = xTaskGetCurrentTaskHandle();
    const esp_lcd_rgb_panel_event_callbacks_t callbacks = {
        .on_frame_buf_complete = frame_buffer_complete,
    };
    ESP_RETURN_ON_ERROR(
        esp_lcd_rgb_panel_register_event_callbacks(board->panel, &callbacks,
                                                   board),
        TAG, "register RGB frame completion callback");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(board->panel), TAG, "initialize RGB panel");
    ESP_RETURN_ON_ERROR(
        esp_lcd_rgb_panel_get_frame_buffer(
            board->panel, 2, (void **)&board->panel_framebuffers[0],
            (void **)&board->panel_framebuffers[1]),
        TAG, "get RGB panel frame buffers");
    board->next_panel_framebuffer = 1;
    board->framebuffer = heap_caps_calloc(
        JCBOARD_WIDTH * JCBOARD_HEIGHT, sizeof(*board->framebuffer),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return board->framebuffer == NULL ? ESP_ERR_NO_MEM : ESP_OK;
}

static esp_err_t init_touch(jcboard_t *board)
{
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    io_config.scl_speed_hz = 0;
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)I2C_PORT, &io_config, &io),
        TAG, "create GT911 I2C IO");

    const esp_lcd_touch_config_t config = {
        .x_max = JCBOARD_WIDTH,
        .y_max = JCBOARD_HEIGHT,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = GPIO_NUM_NC,
        .levels = {.reset = 0, .interrupt = 0},
        .flags = {.swap_xy = 0, .mirror_x = 0, .mirror_y = 0},
    };
    return esp_lcd_touch_new_i2c_gt911(io, &config, &board->touch);
}

esp_err_t jcboard_init(jcboard_t *board)
{
    ESP_RETURN_ON_FALSE(board != NULL, ESP_ERR_INVALID_ARG, TAG, "board is null");
    ESP_RETURN_ON_ERROR(i2c_init(), TAG, "I2C init");
    ESP_RETURN_ON_ERROR(init_panel(board), TAG, "panel init");
    ESP_RETURN_ON_ERROR(reset_touch_and_enable_backlight(), TAG, "board reset sequence");
    ESP_RETURN_ON_ERROR(init_touch(board), TAG, "touch init");
    ESP_LOGI(TAG, "Waveshare RGB panel and GT911 ready");
    return ESP_OK;
}

uint16_t *jcboard_framebuffer(jcboard_t *board)
{
    return board == NULL ? NULL : board->framebuffer;
}

esp_err_t jcboard_present(jcboard_t *board)
{
    ESP_RETURN_ON_FALSE(board != NULL && board->panel != NULL &&
                            board->framebuffer != NULL &&
                            board->panel_framebuffers[0] != NULL &&
                            board->panel_framebuffers[1] != NULL,
                        ESP_ERR_INVALID_STATE, TAG, "board not initialized");
    uint16_t *panel_framebuffer =
        board->panel_framebuffers[board->next_panel_framebuffer];
    if (board->presented_once) {
        ESP_RETURN_ON_FALSE(
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100)) > 0,
            ESP_ERR_TIMEOUT, TAG, "wait for RGB frame completion");
    }
    memcpy(panel_framebuffer, board->framebuffer,
           JCBOARD_WIDTH * JCBOARD_HEIGHT * sizeof(*board->framebuffer));
    ESP_RETURN_ON_ERROR(
        esp_lcd_panel_draw_bitmap(board->panel, 0, 0, JCBOARD_WIDTH,
                                  JCBOARD_HEIGHT, panel_framebuffer),
        TAG, "present RGB panel frame buffer");
    board->next_panel_framebuffer ^= 1U;
    board->presented_once = true;
    (void)ulTaskNotifyTake(pdTRUE, 0);
    return ESP_OK;
}

bool jcboard_read_touch(jcboard_t *board, uint16_t *x, uint16_t *y)
{
    if (board == NULL || board->touch == NULL || x == NULL || y == NULL) {
        return false;
    }
    if (esp_lcd_touch_read_data(board->touch) != ESP_OK) {
        return false;
    }
    esp_lcd_touch_point_data_t point = {0};
    uint8_t count = 0;
    if (esp_lcd_touch_get_data(board->touch, &point, &count, 1) != ESP_OK || count == 0) {
        return false;
    }
    *x = point.x;
    *y = point.y;
    return true;
}
