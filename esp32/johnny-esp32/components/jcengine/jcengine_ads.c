#include "jcengine.h"

#include <stdbool.h>
#include <string.h>

#include "esp_check.h"

static const char *TAG = "jcengine_ads";

enum {
    ADS_MAX_INSTRUCTIONS = 2048,
    ADS_MAX_GOSUB_DEPTH = 8,
    ADS_MAX_RANDOM_OPS = 10,
    ADS_RANDOM_NOP = 2,
};

typedef struct {
    bool active;
    bool result;
    uint16_t pending_operator;
    bool operator_set;
} ads_condition_t;

typedef struct {
    jcengine_ads_action_type_t type;
    uint16_t slot;
    uint16_t tag;
    uint16_t plays;
    uint16_t weight;
} ads_random_op_t;

static uint16_t le16(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static bool opcode_arg_count(uint16_t opcode, size_t *arg_count)
{
    switch (opcode) {
    case 0x1070:
    case 0x1330:
    case 0x1350:
    case 0x1360:
    case 0x1370:
        *arg_count = 2;
        return true;
    case 0x2005:
        *arg_count = 4;
        return true;
    case 0x2010:
    case 0x4000:
        *arg_count = 3;
        return true;
    case 0x3020:
    case 0xf200:
        *arg_count = 1;
        return true;
    case 0x1420:
    case 0x1430:
    case 0x1510:
    case 0x1520:
    case 0x2014:
    case 0x3010:
    case 0x30ff:
    case 0xf010:
    case 0xfff0:
    case 0xffff:
        *arg_count = 0;
        return true;
    default:
        return false;
    }
}

static esp_err_t find_tag_range(const jcrez_ads_t *ads, uint16_t requested,
                                size_t *tag_offset, size_t *tag_end)
{
    size_t offset = 0;
    bool found = false;
    while (offset + 2 <= ads->bytecode_size) {
        size_t instruction_offset = offset;
        uint16_t opcode = le16(ads->bytecode + offset);
        offset += 2;
        size_t arg_count = 0;
        if (!opcode_arg_count(opcode, &arg_count)) {
            if (found) {
                if (tag_end != NULL) *tag_end = instruction_offset;
                return ESP_OK;
            }
            if (opcode == requested) {
                *tag_offset = offset;
                found = true;
            }
            continue;
        }
        ESP_RETURN_ON_FALSE(offset + arg_count * 2 <= ads->bytecode_size,
                            ESP_ERR_INVALID_SIZE, TAG, "truncated ADS arguments");
        offset += arg_count * 2;
    }
    if (found) {
        if (tag_end != NULL) *tag_end = ads->bytecode_size;
        return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
}

static esp_err_t find_tag(const jcrez_ads_t *ads, uint16_t requested,
                          size_t *tag_offset)
{
    return find_tag_range(ads, requested, tag_offset, NULL);
}

static void condition_add(ads_condition_t *condition, bool result)
{
    if (!condition->active) {
        condition->active = true;
        condition->result = result;
    } else if (condition->operator_set && condition->pending_operator == 0x1430) {
        condition->result = condition->result || result;
    } else {
        condition->result = condition->result && result;
    }
    condition->pending_operator = 0;
    condition->operator_set = false;
}

static bool condition_should_execute(const ads_condition_t *condition)
{
    return !condition->active || condition->result;
}

static jcengine_ads_scene_state_t *scene_state(jcengine_ads_scheduler_t *scheduler,
                                                uint16_t slot, uint16_t tag,
                                                bool create)
{
    for (size_t index = 0; index < scheduler->scene_count; ++index) {
        if (scheduler->scenes[index].slot == slot &&
            scheduler->scenes[index].tag == tag) {
            return &scheduler->scenes[index];
        }
    }
    if (!create || scheduler->scene_count >= JCENGINE_ADS_MAX_SCENES) {
        return NULL;
    }
    jcengine_ads_scene_state_t *state =
        &scheduler->scenes[scheduler->scene_count++];
    state->slot = slot;
    state->tag = tag;
    return state;
}

static esp_err_t emit_action(jcengine_ads_scheduler_t *scheduler,
                             jcengine_ads_action_type_t type, uint16_t slot,
                             uint16_t tag, uint16_t plays)
{
    jcengine_ads_scene_state_t *state = scene_state(scheduler, slot, tag,
                                                    type == JCENGINE_ADS_ADD_SCENE);
    if (type == JCENGINE_ADS_ADD_SCENE) {
        ESP_RETURN_ON_FALSE(state != NULL, ESP_ERR_NO_MEM, TAG,
                            "ADS scene-state limit reached");
        if (state->running) {
            return ESP_OK;
        }
        state->running = true;
        state->played = true;
    } else {
        if (state == NULL || !state->running) {
            return ESP_OK;
        }
        state->running = false;
    }
    ESP_RETURN_ON_FALSE(scheduler->action_count < JCENGINE_ADS_MAX_ACTIONS,
                        ESP_ERR_NO_MEM, TAG, "ADS action limit reached");
    scheduler->actions[scheduler->action_count++] = (jcengine_ads_action_t){
        .type = type,
        .slot = slot,
        .tag = tag,
        .plays = plays,
    };
    return ESP_OK;
}

static bool scene_running(jcengine_ads_scheduler_t *scheduler, uint16_t slot,
                          uint16_t tag)
{
    jcengine_ads_scene_state_t *state = scene_state(scheduler, slot, tag, false);
    return state != NULL && state->running;
}

static bool scene_played(jcengine_ads_scheduler_t *scheduler, uint16_t slot,
                         uint16_t tag)
{
    jcengine_ads_scene_state_t *state = scene_state(scheduler, slot, tag, false);
    return state != NULL && state->played;
}

static esp_err_t run_chunk(jcengine_ads_scheduler_t *scheduler, size_t offset,
                           bool triggered, unsigned depth)
{
    ESP_RETURN_ON_FALSE(depth <= ADS_MAX_GOSUB_DEPTH, ESP_ERR_INVALID_STATE,
                        TAG, "ADS GOSUB limit reached");
    ads_condition_t condition = {0};
    ads_random_op_t random_ops[ADS_MAX_RANDOM_OPS] = {0};
    size_t random_count = 0;
    bool in_random = false;
    bool continue_loop = true;
    size_t instruction_count = 0;
    if (triggered) {
        condition_add(&condition, true);
    }

    while (continue_loop && offset < scheduler->ads->bytecode_size) {
        ESP_RETURN_ON_FALSE(++instruction_count <= ADS_MAX_INSTRUCTIONS,
                            ESP_ERR_INVALID_STATE, TAG,
                            "ADS instruction limit reached");
        ESP_RETURN_ON_FALSE(offset + 2 <= scheduler->ads->bytecode_size,
                            ESP_ERR_INVALID_SIZE, TAG, "truncated ADS opcode");
        uint16_t opcode = le16(scheduler->ads->bytecode + offset);
        offset += 2;
        size_t arg_count = 0;
        if (!opcode_arg_count(opcode, &arg_count)) {
            continue;
        }
        ESP_RETURN_ON_FALSE(offset + arg_count * 2 <= scheduler->ads->bytecode_size,
                            ESP_ERR_INVALID_SIZE, TAG, "truncated ADS arguments");
        uint16_t args[4] = {0};
        for (size_t index = 0; index < arg_count; ++index) {
            args[index] = le16(scheduler->ads->bytecode + offset + index * 2);
        }
        offset += arg_count * 2;

        switch (opcode) {
        case 0x1330:
            condition_add(&condition,
                          !scene_played(scheduler, args[0], args[1]));
            break;
        case 0x1350:
            if (triggered && condition.operator_set &&
                condition.pending_operator == 0x1430) {
                condition_add(&condition, false);
            } else {
                continue_loop = false;
            }
            break;
        case 0x1360:
            condition_add(&condition,
                          !scene_running(scheduler, args[0], args[1]));
            break;
        case 0x1370:
            condition_add(&condition,
                          scene_running(scheduler, args[0], args[1]));
            break;
        case 0x1420:
        case 0x1430:
            condition.pending_operator = opcode;
            condition.operator_set = true;
            break;
        case 0x1510:
            if (condition_should_execute(&condition)) {
                continue_loop = false;
            } else {
                memset(&condition, 0, sizeof(condition));
                triggered = false;
            }
            break;
        case 0x2005:
            if (condition_should_execute(&condition)) {
                if (in_random) {
                    ESP_RETURN_ON_FALSE(random_count < ADS_MAX_RANDOM_OPS,
                                        ESP_ERR_NO_MEM, TAG,
                                        "ADS random-operation limit reached");
                    random_ops[random_count++] = (ads_random_op_t){
                        .type = JCENGINE_ADS_ADD_SCENE,
                        .slot = args[0], .tag = args[1], .plays = args[2],
                        .weight = args[3],
                    };
                } else {
                    ESP_RETURN_ON_ERROR(emit_action(scheduler,
                                                    JCENGINE_ADS_ADD_SCENE,
                                                    args[0], args[1], args[2]),
                                        TAG, "add ADS scene");
                }
            }
            break;
        case 0x2010:
            if (condition_should_execute(&condition)) {
                if (in_random) {
                    ESP_RETURN_ON_FALSE(random_count < ADS_MAX_RANDOM_OPS,
                                        ESP_ERR_NO_MEM, TAG,
                                        "ADS random-operation limit reached");
                    random_ops[random_count++] = (ads_random_op_t){
                        .type = JCENGINE_ADS_STOP_SCENE,
                        .slot = args[0], .tag = args[1], .weight = args[2],
                    };
                } else {
                    ESP_RETURN_ON_ERROR(emit_action(scheduler,
                                                    JCENGINE_ADS_STOP_SCENE,
                                                    args[0], args[1], 0),
                                        TAG, "stop ADS scene");
                }
            }
            break;
        case 0x3010:
            random_count = 0;
            in_random = true;
            break;
        case 0x3020:
            if (in_random && condition_should_execute(&condition)) {
                ESP_RETURN_ON_FALSE(random_count < ADS_MAX_RANDOM_OPS,
                                    ESP_ERR_NO_MEM, TAG,
                                    "ADS random-operation limit reached");
                random_ops[random_count++] = (ads_random_op_t){
                    .type = ADS_RANDOM_NOP,
                    .weight = args[0],
                };
            }
            break;
        case 0x30ff:
            if (condition_should_execute(&condition) && random_count > 0) {
                uint32_t total_weight = 0;
                for (size_t index = 0; index < random_count; ++index) {
                    total_weight += random_ops[index].weight;
                }
                ESP_RETURN_ON_FALSE(total_weight > 0, ESP_ERR_INVALID_ARG,
                                    TAG, "ADS random block has zero weight");
                uint32_t pick = scheduler->random_value % total_weight;
                scheduler->random_value = scheduler->random_value * 1664525U +
                                          1013904223U;
                size_t selected = 0;
                for (; selected < random_count; ++selected) {
                    if (pick < random_ops[selected].weight) break;
                    pick -= random_ops[selected].weight;
                }
                ESP_RETURN_ON_FALSE(selected < random_count,
                                    ESP_ERR_INVALID_RESPONSE, TAG,
                                    "ADS random selection failed");
                if (random_ops[selected].type == JCENGINE_ADS_ADD_SCENE ||
                    random_ops[selected].type == JCENGINE_ADS_STOP_SCENE) {
                    ESP_RETURN_ON_ERROR(
                        emit_action(scheduler, random_ops[selected].type,
                                    random_ops[selected].slot,
                                    random_ops[selected].tag,
                                    random_ops[selected].plays),
                        TAG, "apply ADS random action");
                }
            }
            in_random = false;
            random_count = 0;
            break;
        case 0xf200: {
            if (condition_should_execute(&condition)) {
                size_t subroutine = 0;
                ESP_RETURN_ON_ERROR(find_tag(scheduler->ads, args[0], &subroutine),
                                    TAG, "find ADS GOSUB tag");
                ESP_RETURN_ON_ERROR(run_chunk(scheduler, subroutine, false,
                                              depth + 1),
                                    TAG, "run ADS GOSUB");
            }
            break;
        }
        case 0xffff:
            if (!condition_should_execute(&condition)) {
                memset(&condition, 0, sizeof(condition));
            } else {
                scheduler->stop_requested = true;
                continue_loop = false;
            }
            break;
        default:
            break;
        }
    }
    return ESP_OK;
}

esp_err_t jcengine_ads_start(jcengine_ads_scheduler_t *scheduler,
                             const jcrez_ads_t *ads, uint16_t tag,
                             uint32_t random_value)
{
    ESP_RETURN_ON_FALSE(scheduler != NULL && ads != NULL &&
                            ads->bytecode != NULL,
                        ESP_ERR_INVALID_ARG, TAG, "invalid ADS scheduler");
    memset(scheduler, 0, sizeof(*scheduler));
    scheduler->ads = ads;
    scheduler->random_value = random_value;
    ESP_RETURN_ON_ERROR(find_tag_range(ads, tag, &scheduler->active_tag_offset,
                                      &scheduler->active_tag_end),
                        TAG, "find ADS start tag");
    return run_chunk(scheduler, scheduler->active_tag_offset, false, 0);
}

esp_err_t jcengine_ads_scene_finished(jcengine_ads_scheduler_t *scheduler,
                                      uint16_t slot, uint16_t tag)
{
    ESP_RETURN_ON_FALSE(scheduler != NULL && scheduler->ads != NULL,
                        ESP_ERR_INVALID_ARG, TAG, "invalid ADS scheduler");
    jcengine_ads_scene_state_t *state = scene_state(scheduler, slot, tag, false);
    ESP_RETURN_ON_FALSE(state != NULL && state->running,
                        ESP_ERR_INVALID_STATE, TAG,
                        "completed ADS scene was not running");
    state->running = false;
    scheduler->action_count = 0;
    if (scheduler->stop_requested) {
        return ESP_OK;
    }

    size_t offset = scheduler->active_tag_offset;
    size_t instruction_count = 0;
    while (offset < scheduler->active_tag_end) {
        ESP_RETURN_ON_FALSE(++instruction_count <= ADS_MAX_INSTRUCTIONS,
                            ESP_ERR_INVALID_STATE, TAG,
                            "ADS completion scan limit reached");
        ESP_RETURN_ON_FALSE(offset + 2 <= scheduler->active_tag_end,
                            ESP_ERR_INVALID_SIZE, TAG,
                            "truncated ADS completion opcode");
        uint16_t opcode = le16(scheduler->ads->bytecode + offset);
        offset += 2;
        size_t arg_count = 0;
        ESP_RETURN_ON_FALSE(opcode_arg_count(opcode, &arg_count),
                            ESP_ERR_INVALID_RESPONSE, TAG,
                            "unexpected ADS tag inside active range");
        ESP_RETURN_ON_FALSE(offset + arg_count * 2 <= scheduler->active_tag_end,
                            ESP_ERR_INVALID_SIZE, TAG,
                            "truncated ADS completion arguments");
        uint16_t first_arg = arg_count > 0
                                 ? le16(scheduler->ads->bytecode + offset)
                                 : 0;
        uint16_t second_arg = arg_count > 1
                                  ? le16(scheduler->ads->bytecode + offset + 2)
                                  : 0;
        offset += arg_count * 2;
        if (opcode == 0x1350 && first_arg == slot && second_arg == tag) {
            ESP_RETURN_ON_ERROR(run_chunk(scheduler, offset, true, 0),
                                TAG, "run ADS completion trigger");
        }
    }
    return ESP_OK;
}
