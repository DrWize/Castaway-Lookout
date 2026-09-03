#include "jcengine.h"

#include <string.h>

static uint32_t next_random(uint32_t *value)
{
    *value = *value * UINT32_C(1664525) + UINT32_C(1013904223);
    return *value;
}

static uint32_t bounded_random(uint32_t *value, uint32_t limit)
{
    return limit == 0 ? 0 : next_random(value) % limit;
}

uint32_t jcengine_story_event_seed(const char *ads_name, uint16_t ads_tag,
                                   uint8_t story_day)
{
    uint32_t hash = UINT32_C(2166136261);
    if (ads_name != NULL) {
        for (const unsigned char *cursor = (const unsigned char *)ads_name;
             *cursor != '\0'; ++cursor) {
            hash = (hash ^ *cursor) * UINT32_C(16777619);
        }
    }
    hash = (hash ^ (ads_tag & 0xffU)) * UINT32_C(16777619);
    hash = (hash ^ (ads_tag >> 8)) * UINT32_C(16777619);
    hash = (hash ^ story_day) * UINT32_C(16777619);
    return hash == 0 ? UINT32_C(0x6d2b79f5) : hash;
}

static bool select_night(const jcengine_local_time_t *local_time,
                         jcengine_sky_mode_t sky_mode)
{
    if (sky_mode == JCENGINE_SKY_DAY) return false;
    if (sky_mode == JCENGINE_SKY_NIGHT) return true;
    return local_time != NULL && local_time->valid &&
           (local_time->hour < 6 || local_time->hour >= 18);
}

static uint8_t raft_stage(uint8_t story_day, uint8_t flags)
{
    if ((flags & JCENGINE_STORY_NORAFT) != 0) return 0;
    if (story_day <= 2) return 1;
    if (story_day <= 5) return story_day - 1;
    return 5;
}

static void select_position(jcengine_island_state_t *state, uint8_t flags,
                            uint32_t *random_value)
{
    if ((flags & JCENGINE_STORY_VARPOS_OK) != 0) {
        if (bounded_random(random_value, 2) != 0) {
            state->offset_x = (int16_t)(-222 + (int)bounded_random(random_value, 109));
            state->offset_y = (int16_t)(-44 + (int)bounded_random(random_value, 128));
        } else if (bounded_random(random_value, 2) != 0) {
            state->offset_x = (int16_t)(-114 + (int)bounded_random(random_value, 134));
            state->offset_y = (int16_t)(-14 + (int)bounded_random(random_value, 99));
        } else {
            state->offset_x = (int16_t)(-114 + (int)bounded_random(random_value, 119));
            state->offset_y = (int16_t)(-73 + (int)bounded_random(random_value, 60));
        }
    } else if ((flags & JCENGINE_STORY_LEFT_ISLAND) != 0) {
        state->offset_x = -272;
    }
}

static void initialize_clouds(jcengine_island_state_t *state,
                              uint32_t *random_value)
{
    static const uint16_t MAX_X[] = {511, 448, 376};
    static const uint16_t MAX_Y[] = {99, 78, 59};
    state->cloud_count = (uint8_t)bounded_random(random_value, 6);
    state->cloud_direction = bounded_random(random_value, 2) == 0 ? -1 : 1;
    for (uint8_t index = 0; index < state->cloud_count; ++index) {
        jcengine_cloud_t *cloud = &state->clouds[index];
        cloud->sprite = (uint8_t)bounded_random(random_value, 3);
        cloud->x = (int16_t)bounded_random(random_value, MAX_X[cloud->sprite]);
        cloud->y = (int16_t)bounded_random(random_value, MAX_Y[cloud->sprite]);
        cloud->speed = (int8_t)(bounded_random(random_value, 2) + 1);
    }
}

static void advance_wave(jcengine_island_state_t *state)
{
    uint8_t wave_count = state->low_tide ? 4 : 3;
    state->wave_slot = (uint8_t)((state->wave_slot + 1) % wave_count);
    state->wave_phases[state->wave_slot] = state->wave_phase;
    if (state->wave_slot == 0) state->wave_phase ^= 1U;
}

void jcengine_island_initialize(jcengine_island_state_t *state,
                                const jcengine_story_scene_t *scene,
                                uint8_t story_day,
                                const jcengine_local_time_t *local_time,
                                jcengine_sky_mode_t sky_mode,
                                uint32_t deterministic_seed)
{
    if (state == NULL) return;
    memset(state, 0, sizeof(*state));
    if (scene == NULL || (scene->flags & JCENGINE_STORY_ISLAND) == 0) return;

    state->active = true;
    state->story_day = story_day >= 1 && story_day <= 11 ? story_day : 1;
    state->seed = deterministic_seed != 0
                      ? deterministic_seed
                      : jcengine_story_event_seed(scene->ads_name, scene->ads_tag,
                                                  state->story_day);
    uint32_t random_value = state->seed;
    state->night = select_night(local_time, sky_mode);
    state->ocean_index = (uint8_t)bounded_random(&random_value, 3);
    state->low_tide = (scene->flags & JCENGINE_STORY_LOWTIDE_OK) != 0 &&
                      bounded_random(&random_value, 2) != 0;
    select_position(state, scene->flags, &random_value);
    state->raft_stage = raft_stage(state->story_day, scene->flags);
    if ((scene->flags & JCENGINE_STORY_HOLIDAY_NOK) == 0 &&
        local_time != NULL && local_time->valid) {
        const jcengine_special_day_t *special =
            jcengine_special_day_for_date(local_time->month, local_time->day);
        if (special != NULL) state->holiday_id = special->overlay_id;
    }
    initialize_clouds(state, &random_value);
    /* Windows authors four initial draws into the persistent background. */
    for (uint8_t index = 0; index < 4; ++index) advance_wave(state);
}

bool jcengine_island_tick(jcengine_island_state_t *state,
                          uint16_t elapsed_centiseconds)
{
    if (state == NULL || !state->active || elapsed_centiseconds == 0) return false;
    bool changed = false;
    state->wave_elapsed = (uint16_t)(state->wave_elapsed + elapsed_centiseconds);
    while (state->wave_elapsed >= 8) {
        state->wave_elapsed -= 8;
        advance_wave(state);
        changed = true;
    }

    state->cloud_elapsed = (uint16_t)(state->cloud_elapsed + elapsed_centiseconds);
    while (state->cloud_elapsed >= 8) {
        state->cloud_elapsed -= 8;
        for (uint8_t index = 0; index < state->cloud_count; ++index) {
            jcengine_cloud_t *cloud = &state->clouds[index];
            cloud->x = (int16_t)(cloud->x - state->cloud_direction * cloud->speed);
            if (cloud->x > 640 + 264) cloud->x = -264;
            if (cloud->x < -264) cloud->x = 640 + 264;
        }
        changed = true;
    }
    return changed;
}

bool jcengine_story_day_update(uint8_t *story_day, int32_t *last_date_key,
                               const jcengine_local_time_t *local_time)
{
    if (story_day == NULL || last_date_key == NULL || local_time == NULL ||
        !local_time->valid) {
        return false;
    }
    if (*story_day < 1 || *story_day > 11) *story_day = 1;
    if (*last_date_key == 0) {
        *last_date_key = local_time->date_key;
        return true;
    }
    if (*last_date_key != local_time->date_key) {
        *story_day = *story_day == 11 ? 1 : (uint8_t)(*story_day + 1);
        *last_date_key = local_time->date_key;
        return true;
    }
    return false;
}
