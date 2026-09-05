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

typedef enum {
    JCCONTROL_PLAYBACK_NORMAL = 0,
    JCCONTROL_PLAYBACK_REVIEW,
} jccontrol_playback_mode_t;

typedef enum {
    JCCONTROL_SIDEBAR_OFF = 0,
    JCCONTROL_SIDEBAR_CLOCK,
    JCCONTROL_SIDEBAR_REVIEW,
} jccontrol_sidebar_mode_t;

typedef struct {
    jcengine_sky_mode_t sky;
    jccontrol_holiday_mode_t holiday;
    jccontrol_playback_mode_t playback_mode;
    jccontrol_sidebar_mode_t sidebar_mode;
} jccontrol_settings_t;

typedef enum {
    JCCONTROL_COMMAND_SETTINGS,
    JCCONTROL_COMMAND_SCENE,
    JCCONTROL_COMMAND_RANDOM,
    JCCONTROL_COMMAND_REVIEW_OK,
    JCCONTROL_COMMAND_REVIEW_BUG,
    JCCONTROL_COMMAND_REVIEW_PREVIOUS,
    JCCONTROL_COMMAND_REVIEW_NEXT,
} jccontrol_command_type_t;

typedef struct {
    jccontrol_command_type_t type;
    bool has_sky;
    bool has_holiday;
    bool has_playback_mode;
    bool has_sidebar_mode;
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
    uint8_t cycle_position;
    uint32_t cycle_block;
    uint8_t story_day;
    int16_t island_x;
    int16_t island_y;
    bool low_tide;
    uint8_t raft_stage;
    uint64_t catalog_fingerprint;
    int32_t runtime_error;
    char firmware_version[32];
} jccontrol_snapshot_t;

typedef struct {
    bool present;
    size_t scene_index;
    uint32_t frame;
    jccontrol_settings_t settings;
    bool effective_night;
    uint8_t effective_holiday;
    uint8_t cycle_position;
    uint32_t cycle_block;
    uint8_t story_day;
    int16_t island_x;
    int16_t island_y;
    bool low_tide;
    uint8_t raft_stage;
    uint64_t catalog_fingerprint;
    int32_t runtime_error;
    int64_t captured_at;
    uint32_t uptime_seconds;
    char firmware_version[32];
} jccontrol_bug_record_t;

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
const char *jccontrol_playback_mode_name(jccontrol_playback_mode_t mode);
const char *jccontrol_sidebar_mode_name(jccontrol_sidebar_mode_t mode);
bool jccontrol_parse_sky(const char *value, jcengine_sky_mode_t *sky);
bool jccontrol_parse_holiday(const char *value,
                             jccontrol_holiday_mode_t *holiday);
bool jccontrol_parse_playback_mode(const char *value,
                                   jccontrol_playback_mode_t *mode);
bool jccontrol_parse_sidebar_mode(const char *value,
                                  jccontrol_sidebar_mode_t *mode);

esp_err_t jccontrol_bug_capture(const jccontrol_snapshot_t *snapshot);
esp_err_t jccontrol_bug_resolve(size_t scene_index);
esp_err_t jccontrol_bug_clear(void);
size_t jccontrol_bug_count(void);
esp_err_t jccontrol_bug_records(jccontrol_bug_record_t *records, size_t count);

void jccontrol_shuffle_reset(jccontrol_shuffle_t *shuffle, uint32_t seed);
size_t jccontrol_shuffle_next(jccontrol_shuffle_t *shuffle, uint32_t seed);
size_t jccontrol_shuffle_remaining(const jccontrol_shuffle_t *shuffle);

esp_err_t jccontrol_submit(const jccontrol_command_t *command,
                           uint32_t timeout_ms);
bool jccontrol_take(jccontrol_command_t *command);
void jccontrol_complete(esp_err_t result);
void jccontrol_publish(const jccontrol_snapshot_t *snapshot);
void jccontrol_snapshot(jccontrol_snapshot_t *snapshot);
