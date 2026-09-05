#include "jccontrol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"

enum { SETTINGS_SCHEMA = 4, BUG_SCHEMA = 2 };

typedef struct {
    jccontrol_command_t command;
    TaskHandle_t waiter;
} queued_command_t;

static QueueHandle_t command_queue;
static SemaphoreHandle_t submit_lock;
static SemaphoreHandle_t snapshot_lock;
static SemaphoreHandle_t bug_lock;
static jccontrol_snapshot_t current_snapshot;
static queued_command_t active_command;
static esp_err_t active_result;

static uint32_t prng(uint32_t *state)
{
    uint32_t value = *state == 0 ? UINT32_C(0x9e3779b9) : *state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
}

esp_err_t jccontrol_init(void)
{
    if (command_queue != NULL) return ESP_OK;
    command_queue = xQueueCreate(8, sizeof(queued_command_t));
    submit_lock = xSemaphoreCreateMutex();
    snapshot_lock = xSemaphoreCreateMutex();
    bug_lock = xSemaphoreCreateMutex();
    return command_queue != NULL && submit_lock != NULL && snapshot_lock != NULL &&
                   bug_lock != NULL
               ? ESP_OK
               : ESP_ERR_NO_MEM;
}

bool jccontrol_settings_valid(const jccontrol_settings_t *settings)
{
    return settings != NULL && settings->sky <= JCENGINE_SKY_CYCLE &&
           settings->holiday <= JCCONTROL_HOLIDAY_NEW_YEAR &&
           settings->playback_mode <= JCCONTROL_PLAYBACK_REVIEW &&
           settings->sidebar_mode <= JCCONTROL_SIDEBAR_REVIEW;
}

esp_err_t jccontrol_load_settings(jccontrol_settings_t *settings)
{
    if (settings == NULL) return ESP_ERR_INVALID_ARG;
    *settings = (jccontrol_settings_t){
        .sky = JCENGINE_SKY_AUTOMATIC,
        .holiday = JCCONTROL_HOLIDAY_AUTOMATIC,
        .playback_mode = JCCONTROL_PLAYBACK_NORMAL,
        .sidebar_mode = JCCONTROL_SIDEBAR_OFF,
    };
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open("jc_settings", NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (err != ESP_OK) return err;
    uint8_t schema = 0, sky = 0, holiday = 0, playback_mode = 0;
    uint8_t reviewer_visible = 0, sidebar_mode = JCCONTROL_SIDEBAR_OFF;
    bool valid = nvs_get_u8(handle, "schema", &schema) == ESP_OK &&
                 nvs_get_u8(handle, "sky", &sky) == ESP_OK &&
                 nvs_get_u8(handle, "holiday", &holiday) == ESP_OK;
    if (schema >= 2) {
        valid = valid &&
                nvs_get_u8(handle, "playback", &playback_mode) == ESP_OK;
    }
    if (schema == 3) {
        valid = valid &&
                nvs_get_u8(handle, "reviewer", &reviewer_visible) == ESP_OK &&
                reviewer_visible <= 1;
    }
    if (schema >= 4) {
        valid = valid &&
                nvs_get_u8(handle, "sidebar", &sidebar_mode) == ESP_OK;
    } else if (schema == 3) {
        sidebar_mode = reviewer_visible ? JCCONTROL_SIDEBAR_REVIEW
                                        : JCCONTROL_SIDEBAR_OFF;
    }
    nvs_close(handle);
    jccontrol_settings_t loaded = {.sky = sky,
                                   .holiday = holiday,
                                   .playback_mode = playback_mode,
                                   .sidebar_mode = sidebar_mode};
    if (valid && schema >= 1 && schema <= SETTINGS_SCHEMA &&
        jccontrol_settings_valid(&loaded)) {
        *settings = loaded;
    }
    return ESP_OK;
}

esp_err_t jccontrol_store_settings(const jccontrol_settings_t *settings)
{
    if (!jccontrol_settings_valid(settings)) return ESP_ERR_INVALID_ARG;
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open("jc_settings", NVS_READWRITE, &handle);
    if (err == ESP_OK) err = nvs_set_u8(handle, "schema", SETTINGS_SCHEMA);
    if (err == ESP_OK) err = nvs_set_u8(handle, "sky", settings->sky);
    if (err == ESP_OK) err = nvs_set_u8(handle, "holiday", settings->holiday);
    if (err == ESP_OK) {
        err = nvs_set_u8(handle, "playback", settings->playback_mode);
    }
    if (err == ESP_OK) {
        err = nvs_set_u8(handle, "sidebar", settings->sidebar_mode);
    }
    if (err == ESP_OK) err = nvs_commit(handle);
    if (handle != 0) nvs_close(handle);
    return err;
}

const char *jccontrol_sky_name(jcengine_sky_mode_t sky)
{
    static const char *const names[] = {"automatic", "day", "night", "cycle"};
    return sky <= JCENGINE_SKY_CYCLE ? names[sky] : "unknown";
}

const char *jccontrol_holiday_name(jccontrol_holiday_mode_t holiday)
{
    static const char *const names[] = {
        "off", "automatic", "halloween", "st_patrick", "christmas",
        "new_year",
    };
    return holiday <= JCCONTROL_HOLIDAY_NEW_YEAR ? names[holiday] : "unknown";
}

const char *jccontrol_playback_mode_name(jccontrol_playback_mode_t mode)
{
    static const char *const names[] = {"normal", "review"};
    return mode <= JCCONTROL_PLAYBACK_REVIEW ? names[mode] : "unknown";
}

const char *jccontrol_sidebar_mode_name(jccontrol_sidebar_mode_t mode)
{
    static const char *const names[] = {"off", "clock", "review"};
    return mode <= JCCONTROL_SIDEBAR_REVIEW ? names[mode] : "unknown";
}

bool jccontrol_parse_sky(const char *value, jcengine_sky_mode_t *sky)
{
    if (value == NULL || sky == NULL) return false;
    for (int index = 0; index <= JCENGINE_SKY_CYCLE; ++index) {
        if (strcmp(value, jccontrol_sky_name(index)) == 0) {
            *sky = index;
            return true;
        }
    }
    return false;
}

bool jccontrol_parse_playback_mode(const char *value,
                                   jccontrol_playback_mode_t *mode)
{
    if (value == NULL || mode == NULL) return false;
    for (int index = 0; index <= JCCONTROL_PLAYBACK_REVIEW; ++index) {
        if (strcmp(value, jccontrol_playback_mode_name(index)) == 0) {
            *mode = index;
            return true;
        }
    }
    return false;
}

bool jccontrol_parse_sidebar_mode(const char *value,
                                  jccontrol_sidebar_mode_t *mode)
{
    if (value == NULL || mode == NULL) return false;
    for (int index = 0; index <= JCCONTROL_SIDEBAR_REVIEW; ++index) {
        if (strcmp(value, jccontrol_sidebar_mode_name(index)) == 0) {
            *mode = index;
            return true;
        }
    }
    return false;
}

static esp_err_t load_bug_records_locked(jccontrol_bug_record_t *records,
                                         size_t count)
{
    memset(records, 0, count * sizeof(*records));
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open("jc_bug", NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (err != ESP_OK) return err;
    uint8_t schema = 0;
    size_t size = count * sizeof(*records);
    bool valid = nvs_get_u8(handle, "schema", &schema) == ESP_OK &&
                 schema == BUG_SCHEMA &&
                 nvs_get_blob(handle, "records", records, &size) == ESP_OK &&
                 size == count * sizeof(*records);
    nvs_close(handle);
    if (!valid) memset(records, 0, count * sizeof(*records));
    return ESP_OK;
}

static esp_err_t store_bug_records_locked(const jccontrol_bug_record_t *records,
                                          size_t count)
{
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open("jc_bug", NVS_READWRITE, &handle);
    if (err == ESP_OK) err = nvs_set_u8(handle, "schema", BUG_SCHEMA);
    if (err == ESP_OK) {
        err = nvs_set_blob(handle, "records", records,
                           count * sizeof(*records));
    }
    if (err == ESP_OK) err = nvs_commit(handle);
    if (handle != 0) nvs_close(handle);
    return err;
}

esp_err_t jccontrol_bug_records(jccontrol_bug_record_t *records, size_t count)
{
    if (records == NULL || count != JCENGINE_STORY_SCENE_COUNT ||
        bug_lock == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(bug_lock, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    esp_err_t err = load_bug_records_locked(records, count);
    xSemaphoreGive(bug_lock);
    return err;
}

esp_err_t jccontrol_bug_capture(const jccontrol_snapshot_t *snapshot)
{
    if (snapshot == NULL || snapshot->scene_index >= JCENGINE_STORY_SCENE_COUNT ||
        bug_lock == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    jccontrol_bug_record_t *records = calloc(JCENGINE_STORY_SCENE_COUNT,
                                             sizeof(*records));
    if (records == NULL) return ESP_ERR_NO_MEM;
    if (xSemaphoreTake(bug_lock, pdMS_TO_TICKS(1000)) != pdTRUE) {
        free(records);
        return ESP_ERR_TIMEOUT;
    }
    esp_err_t err = load_bug_records_locked(records, JCENGINE_STORY_SCENE_COUNT);
    if (err == ESP_OK) {
        jccontrol_bug_record_t *record = &records[snapshot->scene_index];
        *record = (jccontrol_bug_record_t){
            .present = true,
            .scene_index = snapshot->scene_index,
            .frame = snapshot->frame,
            .settings = snapshot->settings,
            .effective_night = snapshot->effective_night,
            .effective_holiday = snapshot->effective_holiday,
            .cycle_position = snapshot->cycle_position,
            .cycle_block = snapshot->cycle_block,
            .story_day = snapshot->story_day,
            .island_x = snapshot->island_x,
            .island_y = snapshot->island_y,
            .low_tide = snapshot->low_tide,
            .raft_stage = snapshot->raft_stage,
            .catalog_fingerprint = snapshot->catalog_fingerprint,
            .runtime_error = snapshot->runtime_error,
            .captured_at = (int64_t)time(NULL),
            .uptime_seconds = (uint32_t)(esp_timer_get_time() / 1000000),
        };
        snprintf(record->firmware_version, sizeof(record->firmware_version),
                 "%s", snapshot->firmware_version);
        err = store_bug_records_locked(records, JCENGINE_STORY_SCENE_COUNT);
    }
    xSemaphoreGive(bug_lock);
    free(records);
    return err;
}

esp_err_t jccontrol_bug_resolve(size_t scene_index)
{
    if (scene_index >= JCENGINE_STORY_SCENE_COUNT || bug_lock == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    jccontrol_bug_record_t *records = calloc(JCENGINE_STORY_SCENE_COUNT,
                                             sizeof(*records));
    if (records == NULL) return ESP_ERR_NO_MEM;
    if (xSemaphoreTake(bug_lock, pdMS_TO_TICKS(1000)) != pdTRUE) {
        free(records);
        return ESP_ERR_TIMEOUT;
    }
    esp_err_t err = load_bug_records_locked(records, JCENGINE_STORY_SCENE_COUNT);
    if (err == ESP_OK) {
        memset(&records[scene_index], 0, sizeof(records[scene_index]));
        err = store_bug_records_locked(records, JCENGINE_STORY_SCENE_COUNT);
    }
    xSemaphoreGive(bug_lock);
    free(records);
    return err;
}

esp_err_t jccontrol_bug_clear(void)
{
    if (bug_lock == NULL) return ESP_ERR_INVALID_STATE;
    jccontrol_bug_record_t *records = calloc(JCENGINE_STORY_SCENE_COUNT,
                                             sizeof(*records));
    if (records == NULL) return ESP_ERR_NO_MEM;
    if (xSemaphoreTake(bug_lock, pdMS_TO_TICKS(1000)) != pdTRUE) {
        free(records);
        return ESP_ERR_TIMEOUT;
    }
    esp_err_t err = store_bug_records_locked(records, JCENGINE_STORY_SCENE_COUNT);
    xSemaphoreGive(bug_lock);
    free(records);
    return err;
}

size_t jccontrol_bug_count(void)
{
    jccontrol_bug_record_t *records = calloc(JCENGINE_STORY_SCENE_COUNT,
                                             sizeof(*records));
    if (records == NULL ||
        jccontrol_bug_records(records, JCENGINE_STORY_SCENE_COUNT) != ESP_OK) {
        free(records);
        return 0;
    }
    size_t count = 0;
    for (size_t index = 0; index < JCENGINE_STORY_SCENE_COUNT; ++index) {
        if (records[index].present) ++count;
    }
    free(records);
    return count;
}

bool jccontrol_parse_holiday(const char *value,
                             jccontrol_holiday_mode_t *holiday)
{
    if (value == NULL || holiday == NULL) return false;
    for (int index = 0; index <= JCCONTROL_HOLIDAY_NEW_YEAR; ++index) {
        if (strcmp(value, jccontrol_holiday_name(index)) == 0) {
            *holiday = index;
            return true;
        }
    }
    return false;
}

void jccontrol_shuffle_reset(jccontrol_shuffle_t *shuffle, uint32_t seed)
{
    if (shuffle == NULL) return;
    for (size_t index = 0; index < JCENGINE_STORY_SCENE_COUNT; ++index) {
        shuffle->order[index] = (uint8_t)index;
    }
    uint32_t state = seed == 0 ? esp_random() : seed;
    for (size_t count = JCENGINE_STORY_SCENE_COUNT; count > 1; --count) {
        size_t other = prng(&state) % count;
        uint8_t temporary = shuffle->order[count - 1];
        shuffle->order[count - 1] = shuffle->order[other];
        shuffle->order[other] = temporary;
    }
    if (shuffle->last_scene >= 0 &&
        shuffle->order[0] == (uint8_t)shuffle->last_scene) {
        uint8_t temporary = shuffle->order[0];
        shuffle->order[0] = shuffle->order[1];
        shuffle->order[1] = temporary;
    }
    shuffle->cursor = 0;
}

size_t jccontrol_shuffle_next(jccontrol_shuffle_t *shuffle, uint32_t seed)
{
    if (shuffle == NULL) return 0;
    if (shuffle->cursor >= JCENGINE_STORY_SCENE_COUNT) {
        jccontrol_shuffle_reset(shuffle, seed);
    }
    size_t scene = shuffle->order[shuffle->cursor++];
    shuffle->last_scene = (int)scene;
    return scene;
}

size_t jccontrol_shuffle_remaining(const jccontrol_shuffle_t *shuffle)
{
    return shuffle == NULL || shuffle->cursor >= JCENGINE_STORY_SCENE_COUNT
               ? 0
               : JCENGINE_STORY_SCENE_COUNT - shuffle->cursor;
}

esp_err_t jccontrol_submit(const jccontrol_command_t *command,
                           uint32_t timeout_ms)
{
    if (command == NULL || command_queue == NULL) return ESP_ERR_INVALID_STATE;
    TickType_t wait = pdMS_TO_TICKS(timeout_ms);
    if (xSemaphoreTake(submit_lock, wait) != pdTRUE) return ESP_ERR_TIMEOUT;
    queued_command_t queued = {.command = *command,
                               .waiter = xTaskGetCurrentTaskHandle()};
    if (xQueueSend(command_queue, &queued, 0) != pdTRUE) {
        xSemaphoreGive(submit_lock);
        return ESP_ERR_NO_MEM;
    }
    uint32_t notification = 0;
    BaseType_t notified = xTaskNotifyWait(0, UINT32_MAX, &notification, wait);
    esp_err_t result = notified == pdTRUE ? active_result : ESP_ERR_TIMEOUT;
    xSemaphoreGive(submit_lock);
    return result;
}

bool jccontrol_take(jccontrol_command_t *command)
{
    if (command == NULL || command_queue == NULL ||
        xQueueReceive(command_queue, &active_command, 0) != pdTRUE) {
        return false;
    }
    *command = active_command.command;
    return true;
}

void jccontrol_complete(esp_err_t result)
{
    active_result = result;
    if (active_command.waiter != NULL) {
        xTaskNotify(active_command.waiter, 1, eSetValueWithOverwrite);
    }
    memset(&active_command, 0, sizeof(active_command));
}

void jccontrol_publish(const jccontrol_snapshot_t *snapshot)
{
    if (snapshot == NULL || snapshot_lock == NULL) return;
    if (xSemaphoreTake(snapshot_lock, pdMS_TO_TICKS(20)) == pdTRUE) {
        current_snapshot = *snapshot;
        xSemaphoreGive(snapshot_lock);
    }
}

void jccontrol_snapshot(jccontrol_snapshot_t *snapshot)
{
    if (snapshot == NULL) return;
    memset(snapshot, 0, sizeof(*snapshot));
    if (snapshot_lock != NULL &&
        xSemaphoreTake(snapshot_lock, pdMS_TO_TICKS(20)) == pdTRUE) {
        *snapshot = current_snapshot;
        xSemaphoreGive(snapshot_lock);
    }
}
