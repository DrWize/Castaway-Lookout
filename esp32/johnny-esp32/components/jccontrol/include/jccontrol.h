#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "jcengine.h"

typedef enum {
    JCCONTROL_HOLIDAY_OFF = 0,
    JCCONTROL_HOLIDAY_AUTOMATIC,
    JCCONTROL_HOLIDAY_HALLOWEEN,
    JCCONTROL_HOLIDAY_ST_PATRICK,
    JCCONTROL_HOLIDAY_CHRISTMAS,
    JCCONTROL_HOLIDAY_NEW_YEAR,
} jccontrol_holiday_mode_t;

typedef struct {
    jcengine_sky_mode_t sky;
    jccontrol_holiday_mode_t holiday;
} jccontrol_settings_t;

typedef enum {
    JCCONTROL_COMMAND_SETTINGS,
    JCCONTROL_COMMAND_SCENE,
    JCCONTROL_COMMAND_RANDOM,
} jccontrol_command_type_t;

typedef struct {
    jccontrol_command_type_t type;
    bool has_sky;
    bool has_holiday;
    jccontrol_settings_t settings;
    size_t scene_index;
} jccontrol_command_t;

typedef struct {
    char hostname[32];
    char ip[16];
    int8_t rssi;
    bool wifi_connected;
    bool time_synced;
    uint32_t uptime_seconds;
    size_t scene_index;
    uint32_t frame;
    size_t shuffle_remaining;
    bool paused;
    jccontrol_settings_t settings;
    uint8_t effective_holiday;
    bool effective_night;
} jccontrol_snapshot_t;

typedef struct {
    uint8_t order[JCENGINE_STORY_SCENE_COUNT];
    size_t cursor;
    int last_scene;
} jccontrol_shuffle_t;

esp_err_t jccontrol_init(void);
esp_err_t jccontrol_load_settings(jccontrol_settings_t *settings);
esp_err_t jccontrol_store_settings(const jccontrol_settings_t *settings);
bool jccontrol_settings_valid(const jccontrol_settings_t *settings);
const char *jccontrol_sky_name(jcengine_sky_mode_t sky);
const char *jccontrol_holiday_name(jccontrol_holiday_mode_t holiday);
bool jccontrol_parse_sky(const char *value, jcengine_sky_mode_t *sky);
bool jccontrol_parse_holiday(const char *value,
                             jccontrol_holiday_mode_t *holiday);

void jccontrol_shuffle_reset(jccontrol_shuffle_t *shuffle, uint32_t seed);
size_t jccontrol_shuffle_next(jccontrol_shuffle_t *shuffle, uint32_t seed);
size_t jccontrol_shuffle_remaining(const jccontrol_shuffle_t *shuffle);

esp_err_t jccontrol_submit(const jccontrol_command_t *command,
                           uint32_t timeout_ms);
bool jccontrol_take(jccontrol_command_t *command);
void jccontrol_complete(esp_err_t result);
void jccontrol_publish(const jccontrol_snapshot_t *snapshot);
void jccontrol_snapshot(jccontrol_snapshot_t *snapshot);
