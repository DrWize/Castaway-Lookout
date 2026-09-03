#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "jcrez.h"

typedef enum {
    JCENGINE_TTM_STOPPED = 0,
    JCENGINE_TTM_RUNNING = 1,
    JCENGINE_TTM_FINISHED = 2,
} jcengine_ttm_status_t;

typedef struct {
    uint32_t offset;
    uint16_t opcode;
    uint8_t arg_count;
    uint16_t args[14];
    char string_arg[201];
} jcengine_ttm_command_t;

typedef esp_err_t (*jcengine_ttm_command_fn)(void *context,
                                             const jcengine_ttm_command_t *command);

typedef struct {
    const jcrez_ttm_t *ttm;
    jcengine_ttm_status_t status;
    /* scene_tag is the ADS-owned root identity. current_tag follows authored
       local/global markers without changing stop/completion ownership. */
    uint16_t scene_tag;
    uint16_t current_tag;
    uint32_t ip;
    uint32_t next_goto_offset;
    uint16_t delay;
    uint16_t timer;
    uint8_t selected_bmp_slot;
    uint8_t fg_color;
    uint8_t bg_color;
} jcengine_ttm_thread_t;

typedef enum {
    JCENGINE_ADS_ADD_SCENE = 0,
    JCENGINE_ADS_STOP_SCENE = 1,
} jcengine_ads_action_type_t;

typedef struct {
    jcengine_ads_action_type_t type;
    uint16_t slot;
    uint16_t tag;
    uint16_t plays;
} jcengine_ads_action_t;

enum {
    /* Played-state history is retained for ADS IF_NOT_PLAYED conditions.
       ACTIVITY.ADS tag 11 alone visits 28 distinct scenes. */
    JCENGINE_ADS_MAX_SCENES = 64,
    JCENGINE_ADS_MAX_ACTIONS = 16,
    JCENGINE_STORY_SCENE_COUNT = 63,
};

typedef struct {
    uint16_t slot;
    uint16_t tag;
    bool running;
    bool played;
} jcengine_ads_scene_state_t;

typedef struct {
    const jcrez_ads_t *ads;
    jcengine_ads_scene_state_t scenes[JCENGINE_ADS_MAX_SCENES];
    size_t scene_count;
    jcengine_ads_action_t actions[JCENGINE_ADS_MAX_ACTIONS];
    size_t action_count;
    size_t active_tag_offset;
    size_t active_tag_end;
    uint32_t random_value;
    bool stop_requested;
} jcengine_ads_scheduler_t;

typedef enum {
    JCENGINE_STORY_FINAL = 0x01,
    JCENGINE_STORY_FIRST = 0x02,
    JCENGINE_STORY_ISLAND = 0x04,
    JCENGINE_STORY_LEFT_ISLAND = 0x08,
    JCENGINE_STORY_VARPOS_OK = 0x10,
    JCENGINE_STORY_LOWTIDE_OK = 0x20,
    JCENGINE_STORY_NORAFT = 0x40,
    JCENGINE_STORY_HOLIDAY_NOK = 0x80,
} jcengine_story_flag_t;

typedef struct {
    const char *ads_name;
    uint16_t ads_tag;
    uint8_t spot_start;
    uint8_t heading_start;
    uint8_t spot_end;
    uint8_t heading_end;
    uint8_t day;
    uint8_t flags;
} jcengine_story_scene_t;

typedef struct {
    const char *entry_id;
    const char *category;
    const char *title;
    const char *mapped_ads;
    const char *mapped_ttm;
} jcengine_scene_menu_entry_t;

typedef struct {
    const char *entry_id;
    const char *title;
    uint8_t start_month;
    uint8_t start_day;
    uint8_t end_month;
    uint8_t end_day;
    uint8_t overlay_id;
} jcengine_special_day_t;

typedef enum {
    JCENGINE_SKY_AUTOMATIC = 0,
    JCENGINE_SKY_DAY = 1,
    JCENGINE_SKY_NIGHT = 2,
} jcengine_sky_mode_t;

typedef struct {
    bool valid;
    uint8_t hour;
    uint8_t month;
    uint8_t day;
    int32_t date_key;
} jcengine_local_time_t;

typedef struct {
    uint8_t sprite;
    int16_t x;
    int16_t y;
    int8_t speed;
} jcengine_cloud_t;

typedef struct {
    bool active;
    bool night;
    bool low_tide;
    uint8_t story_day;
    uint8_t raft_stage;
    uint8_t ocean_index;
    uint8_t holiday_id;
    int16_t offset_x;
    int16_t offset_y;
    uint8_t wave_phase;
    uint8_t wave_slot;
    uint8_t wave_phases[4];
    uint8_t cloud_count;
    int8_t cloud_direction;
    jcengine_cloud_t clouds[5];
    uint16_t wave_elapsed;
    uint16_t cloud_elapsed;
    uint32_t seed;
} jcengine_island_state_t;

esp_err_t jcengine_ttm_find_tag(const jcrez_ttm_t *ttm, uint16_t tag,
                                uint32_t *offset);
esp_err_t jcengine_ttm_start(jcengine_ttm_thread_t *thread,
                             const jcrez_ttm_t *ttm, uint16_t tag);
esp_err_t jcengine_ttm_advance(jcengine_ttm_thread_t *thread,
                               jcengine_ttm_command_fn command_fn, void *context);
esp_err_t jcengine_ttm_tick(jcengine_ttm_thread_t *thread, uint16_t elapsed_centiseconds,
                            jcengine_ttm_command_fn command_fn, void *context,
                            bool *advanced);
void jcengine_ttm_apply_pending_jump(jcengine_ttm_thread_t *thread);

esp_err_t jcengine_ads_start(jcengine_ads_scheduler_t *scheduler,
                             const jcrez_ads_t *ads, uint16_t tag,
                             uint32_t random_value);
esp_err_t jcengine_ads_scene_finished(jcengine_ads_scheduler_t *scheduler,
                                      uint16_t slot, uint16_t tag);

size_t jcengine_story_scene_count(void);
const jcengine_story_scene_t *jcengine_story_scene(size_t index);
bool jcengine_story_scene_eligible(const jcengine_story_scene_t *scene,
                                   uint8_t wanted_flags,
                                   uint8_t unwanted_flags,
                                   uint8_t current_day);
esp_err_t jcengine_story_pick_in_order(uint8_t wanted_flags,
                                       uint8_t unwanted_flags,
                                       uint8_t current_day, size_t *cursor,
                                       const jcengine_story_scene_t **scene);
const char *jcengine_scene_title(const char *ads_name, uint16_t ads_tag);
size_t jcengine_scene_menu_count(void);
const jcengine_scene_menu_entry_t *jcengine_scene_menu_entry(size_t index);
size_t jcengine_special_day_count(void);
const jcengine_special_day_t *jcengine_special_day(size_t index);
const jcengine_special_day_t *jcengine_special_day_for_date(uint8_t month,
                                                            uint8_t day);
uint32_t jcengine_story_event_seed(const char *ads_name, uint16_t ads_tag,
                                   uint8_t story_day);
void jcengine_island_initialize(jcengine_island_state_t *state,
                                const jcengine_story_scene_t *scene,
                                uint8_t story_day,
                                const jcengine_local_time_t *local_time,
                                jcengine_sky_mode_t sky_mode,
                                uint32_t deterministic_seed);
bool jcengine_island_tick(jcengine_island_state_t *state,
                          uint16_t elapsed_centiseconds);
bool jcengine_story_day_update(uint8_t *story_day, int32_t *last_date_key,
                               const jcengine_local_time_t *local_time);
