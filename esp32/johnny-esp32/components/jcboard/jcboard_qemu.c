#include "jcboard.h"

#include "esp_check.h"
#include "esp_lcd_qemu_rgb.h"
#include "esp_log.h"

static const char *TAG = "jcboard";
static uint32_t diagnostic_frame;

#if !CONFIG_JOHNNY_QEMU_HEADLESS
static const uint8_t DIGITS[10][5] = {
    {0x7, 0x5, 0x5, 0x5, 0x7}, {0x2, 0x6, 0x2, 0x2, 0x7},
    {0x7, 0x1, 0x7, 0x4, 0x7}, {0x7, 0x1, 0x7, 0x1, 0x7},
    {0x5, 0x5, 0x7, 0x1, 0x1}, {0x7, 0x4, 0x7, 0x1, 0x7},
    {0x7, 0x4, 0x7, 0x5, 0x7}, {0x7, 0x1, 0x1, 0x1, 0x1},
    {0x7, 0x5, 0x7, 0x5, 0x7}, {0x7, 0x5, 0x7, 0x1, 0x7},
};

static void draw_diagnostic_frame(uint16_t *framebuffer, uint32_t frame)
{
    enum { X = 4, Y = 4, SCALE = 3, DIGIT_WIDTH = 3, DIGIT_COUNT = 4 };
    for (size_t y = Y - 2; y < Y + 5 * SCALE + 2; ++y) {
        for (size_t x = X - 2;
             x < X + DIGIT_COUNT * (DIGIT_WIDTH + 1) * SCALE; ++x) {
            framebuffer[y * JCBOARD_WIDTH + x] = 0x0000;
        }
    }
    uint32_t divisor = 1000;
    for (size_t digit_index = 0; digit_index < DIGIT_COUNT; ++digit_index) {
        uint8_t digit = (uint8_t)((frame / divisor) % 10);
        for (size_t row = 0; row < 5; ++row) {
            for (size_t column = 0; column < DIGIT_WIDTH; ++column) {
                if ((DIGITS[digit][row] & (1U << (2U - column))) == 0) {
                    continue;
                }
                size_t origin_x = X +
                    digit_index * (DIGIT_WIDTH + 1) * SCALE + column * SCALE;
                size_t origin_y = Y + row * SCALE;
                for (size_t dy = 0; dy < SCALE; ++dy) {
                    for (size_t dx = 0; dx < SCALE; ++dx) {
                        framebuffer[(origin_y + dy) * JCBOARD_WIDTH +
                                    origin_x + dx] = 0xffff;
                    }
                }
            }
        }
        divisor /= 10;
    }
}
#endif

esp_err_t jcboard_init(jcboard_t *board)
{
    ESP_RETURN_ON_FALSE(board != NULL, ESP_ERR_INVALID_ARG, TAG, "board is null");
    const esp_lcd_rgb_qemu_config_t config = {
        .width = JCBOARD_WIDTH,
        .height = JCBOARD_HEIGHT,
        .bpp = RGB_QEMU_BPP_16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_rgb_qemu(&config, &board->panel),
                        TAG, "create QEMU RGB panel");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(board->panel), TAG, "reset QEMU RGB panel");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(board->panel), TAG, "initialize QEMU RGB panel");
    ESP_RETURN_ON_ERROR(
        esp_lcd_rgb_qemu_get_frame_buffer(
            board->panel, (void **)&board->framebuffer),
        TAG, "get QEMU dedicated framebuffer");
    ESP_RETURN_ON_FALSE(board->framebuffer != NULL, ESP_ERR_NO_MEM,
                        TAG, "QEMU dedicated framebuffer is null");
    diagnostic_frame = 0;
    ESP_LOGI(TAG, "QEMU RGB565 panel ready; physical I2C, CH422G and GT911 bypassed");
    return ESP_OK;
}

uint16_t *jcboard_framebuffer(jcboard_t *board)
{
    return board == NULL ? NULL : board->framebuffer;
}

esp_err_t jcboard_present(jcboard_t *board)
{
    ESP_RETURN_ON_FALSE(board != NULL && board->panel != NULL && board->framebuffer != NULL,
                        ESP_ERR_INVALID_STATE, TAG, "board not initialized");
#if CONFIG_JOHNNY_QEMU_HEADLESS
    return ESP_OK;
#else
    uint32_t frame = diagnostic_frame++;
    draw_diagnostic_frame(board->framebuffer, frame);
    ESP_RETURN_ON_ERROR(
        esp_lcd_panel_draw_bitmap(board->panel, 0, 0, JCBOARD_WIDTH,
                                  JCBOARD_HEIGHT, board->framebuffer),
        TAG, "refresh QEMU diagnostic frame");
    return ESP_OK;
#endif
}

bool jcboard_read_touch(jcboard_t *board, uint16_t *x, uint16_t *y)
{
    (void)board;
    (void)x;
    (void)y;
    return false;
}
