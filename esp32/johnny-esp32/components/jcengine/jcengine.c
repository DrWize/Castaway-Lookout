#include "jcengine.h"

#include <stdbool.h>
#include <string.h>

#include "esp_check.h"

static const char *TAG = "jcengine";

static uint16_t le16(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

esp_err_t jcengine_ttm_find_tag(const jcrez_ttm_t *ttm, uint16_t tag,
                                uint32_t *offset)
{
    ESP_RETURN_ON_FALSE(ttm != NULL && offset != NULL, ESP_ERR_INVALID_ARG,
                        TAG, "invalid TTM tag lookup");
    for (size_t index = 0; index < ttm->bookmark_count; ++index) {
        if (ttm->bookmarks[index].id == tag) {
            *offset = ttm->bookmarks[index].offset;
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t jcengine_ttm_start(jcengine_ttm_thread_t *thread,
                             const jcrez_ttm_t *ttm, uint16_t tag)
{
    ESP_RETURN_ON_FALSE(thread != NULL && ttm != NULL && ttm->bytecode != NULL,
                        ESP_ERR_INVALID_ARG, TAG, "invalid TTM thread");
    memset(thread, 0, sizeof(*thread));
    thread->ttm = ttm;
    thread->status = JCENGINE_TTM_RUNNING;
    thread->scene_tag = tag;
    thread->current_tag = tag;
    thread->delay = 4;
    thread->fg_color = 0x0f;
    thread->bg_color = 0x0f;
    return tag == 0 ? ESP_OK : jcengine_ttm_find_tag(ttm, tag, &thread->ip);
}

esp_err_t jcengine_ttm_advance(jcengine_ttm_thread_t *thread,
                               jcengine_ttm_command_fn command_fn, void *context)
{
    ESP_RETURN_ON_FALSE(thread != NULL && thread->ttm != NULL &&
                        thread->status == JCENGINE_TTM_RUNNING,
                        ESP_ERR_INVALID_STATE, TAG, "TTM thread is not running");
    const uint8_t *data = thread->ttm->bytecode;
    const size_t size = thread->ttm->bytecode_size;
    size_t offset = thread->ip;
    bool continue_loop = true;
    size_t instruction_count = 0;
    while (continue_loop) {
        ESP_RETURN_ON_FALSE(++instruction_count <= 2048, ESP_ERR_INVALID_STATE,
                            TAG, "TTM instruction limit reached");
        ESP_RETURN_ON_FALSE(offset + 2 <= size, ESP_ERR_INVALID_SIZE,
                            TAG, "truncated TTM opcode");
        jcengine_ttm_command_t command = {.offset = (uint32_t)offset};
        command.opcode = le16(data + offset);
        offset += 2;
        command.arg_count = command.opcode & 0x0f;
        if (command.arg_count == 0x0f) {
            size_t length = 0;
            while (offset < size && data[offset] != 0 &&
                   length + 1 < sizeof(command.string_arg)) {
                command.string_arg[length++] = (char)data[offset++];
            }
            ESP_RETURN_ON_FALSE(offset < size && data[offset] == 0,
                                ESP_ERR_INVALID_SIZE, TAG, "invalid TTM string");
            ++offset;
            if (offset & 1U) ++offset;
            ESP_RETURN_ON_FALSE(offset <= size, ESP_ERR_INVALID_SIZE,
                                TAG, "invalid TTM string padding");
        } else {
            ESP_RETURN_ON_FALSE(command.arg_count <= 14 &&
                                offset + (size_t)command.arg_count * 2U <= size,
                                ESP_ERR_INVALID_SIZE, TAG, "invalid TTM arguments");
            for (size_t index = 0; index < command.arg_count; ++index) {
                command.args[index] = le16(data + offset + index * 2U);
            }
            offset += (size_t)command.arg_count * 2U;
        }
        if (command_fn != NULL) {
            ESP_RETURN_ON_ERROR(command_fn(context, &command), TAG, "TTM command sink");
        }
        switch (command.opcode) {
        case 0x0110:
            thread->status = JCENGINE_TTM_FINISHED;
            if (thread->timer == 0) {
                thread->timer = thread->delay;
            }
            continue_loop = false;
            break;
        case 0x0ff0:
            continue_loop = false;
            break;
        case 0x1021:
            thread->delay = command.args[0] > 4 ? command.args[0] : 4;
            thread->timer = thread->delay;
            break;
        case 0x1051:
            thread->selected_bmp_slot = (uint8_t)command.args[0];
            break;
        case 0x1101:
        case 0x1111:
            thread->current_tag = command.args[0];
            break;
        case 0x1201:
            ESP_RETURN_ON_ERROR(jcengine_ttm_find_tag(thread->ttm, command.args[0],
                                                      &thread->next_goto_offset),
                                TAG, "TTM GOTO tag");
            break;
        case 0x2002:
            thread->fg_color = (uint8_t)command.args[0];
            thread->bg_color = (uint8_t)command.args[1];
            break;
        case 0x2022:
            thread->delay = (command.args[0] + command.args[1]) / 2U;
            thread->timer = thread->delay;
            break;
        default:
            break;
        }
        if (offset >= size) {
            thread->status = JCENGINE_TTM_FINISHED;
            if (thread->timer == 0) {
                thread->timer = thread->delay;
            }
            continue_loop = false;
        }
    }
    thread->ip = (uint32_t)offset;
    return ESP_OK;
}

esp_err_t jcengine_ttm_tick(jcengine_ttm_thread_t *thread, uint16_t elapsed_centiseconds,
                            jcengine_ttm_command_fn command_fn, void *context,
                            bool *advanced)
{
    ESP_RETURN_ON_FALSE(thread != NULL && advanced != NULL, ESP_ERR_INVALID_ARG,
                        TAG, "invalid TTM tick");
    *advanced = false;
    if (thread->status == JCENGINE_TTM_FINISHED) {
        if (thread->timer > elapsed_centiseconds) {
            thread->timer -= elapsed_centiseconds;
        } else {
            thread->timer = 0;
        }
        return ESP_OK;
    }
    ESP_RETURN_ON_FALSE(thread->status == JCENGINE_TTM_RUNNING,
                        ESP_ERR_INVALID_STATE, TAG, "TTM thread is not running");
    uint16_t remaining = elapsed_centiseconds;
    size_t advance_count = 0;
    while (thread->status == JCENGINE_TTM_RUNNING) {
        if (thread->timer > remaining) {
            thread->timer -= remaining;
            return ESP_OK;
        }
        remaining -= thread->timer;
        thread->timer = 0;
        jcengine_ttm_apply_pending_jump(thread);
        ESP_RETURN_ON_FALSE(++advance_count <= 64, ESP_ERR_INVALID_STATE,
                            TAG, "TTM tick advance limit reached");
        ESP_RETURN_ON_ERROR(jcengine_ttm_advance(thread, command_fn, context),
                            TAG, "advance TTM tick");
        *advanced = true;
        if (thread->status == JCENGINE_TTM_FINISHED) {
            if (thread->timer > remaining) {
                thread->timer -= remaining;
            } else {
                thread->timer = 0;
            }
            return ESP_OK;
        }
        if (thread->timer == 0) {
            thread->timer = thread->delay;
        }
        if (remaining == 0 || thread->timer > remaining) {
            thread->timer -= remaining;
            return ESP_OK;
        }
    }
    return ESP_OK;
}

void jcengine_ttm_apply_pending_jump(jcengine_ttm_thread_t *thread)
{
    if (thread != NULL && thread->next_goto_offset != 0) {
        thread->ip = thread->next_goto_offset;
        thread->next_goto_offset = 0;
    }
}
