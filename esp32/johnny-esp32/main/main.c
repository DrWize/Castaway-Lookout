#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "esp_chip_info.h"
#include "esp_app_desc.h"
#include "esp_check.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "jcboard.h"
#include "jccontrol.h"
#include "jcengine.h"
#include "jcgfx.h"
#include "jcnet.h"
#include "jcrez.h"
#include "mbedtls/sha256.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "johnny";
#ifndef CONFIG_JOHNNY_BOARD_TEST
static const char EXPECTED_INTRO_SHA256[] =
    "6eb9bed1fa948b537652cc4f37da9f4733e828d174a52900dfea98932eafaea1";
static const char EXPECTED_RIGHT_RGB565_SHA256[] =
    "be6b74850333a3b2e9f1ecfadbfea7ada47d5b17b0e92b4ba09ff2f02e25ee40";
static const char EXPECTED_MJAMBWLK_SHA256[] =
    "e59d27c19b38134b92f31a73572a6f373a7198cd71053e2d36590a7d763c3344";
static const char EXPECTED_ACTIVITY_ADS_SHA256[] =
    "61f45e0c9c092a9eecf0d3344b41d232376e3ecd997fd64934a80cf2779cbab9";
static const char EXPECTED_STAND_ADS_SHA256[] =
    "ae2af07205b1f276ba3f5e80f86d8c1da82d49333154d0d672459f3da0cc0680";
static const char EXPECTED_MJ_AMB_BMP_SHA256[] =
    "c7150307b7cb3b008c46522ab4aeaedf9be9e4504c03ec318660d54912d307ac";
static const char EXPECTED_ACTIVITY_LAYER_SHA256[] =
    "0085c76f798dd4d065caba2fcd294281a3c18a2b1b7664ef8e15767bfad1db30";
static const char EXPECTED_ACTIVITY_FINAL_SHA256[] =
    "a68119006596c3a6402f4fadadc945df10fb32481d2996b7d8b81cdb1c8be4f1";
static const char *const EXPECTED_AMBIENT_FRAME_SHA256[] = {
    "c073f88c2dfeb087d09d86097833704c2d3169f375794076d2c1c51071cb275a",
    "11ddb76221a6a2ae88c2cae76a76d56bbcb1167ecb33a0f3e4f6cf392d9a5bf8",
    "9bd67bfb39cd0ff6cf9829c05888de77a86c781045b5b3e0646d31e9e4ad32bd",
    "6eba0b3f3a2a57197c1f1543c47d9b1c5f2dfa568aac2afc02432cfa18eaffc8",
    "9bd67bfb39cd0ff6cf9829c05888de77a86c781045b5b3e0646d31e9e4ad32bd",
    "6eba0b3f3a2a57197c1f1543c47d9b1c5f2dfa568aac2afc02432cfa18eaffc8",
    "9bd67bfb39cd0ff6cf9829c05888de77a86c781045b5b3e0646d31e9e4ad32bd",
    "2f28d021124373f4ca67c43666c05071dabe7ff93561ee27d4998d4aa5aecfb0",
    "8c4834d84d432f3880ffadbfa1392bd91d54038c91269b8e86488b207178901a",
};

enum {
    TTM_BITMAP_SLOTS = 10,
    TTM_RUNTIME_RESOURCE_SLOTS = 8,
    TTM_RUNTIME_THREADS = 10,
    /* Geometry support makes GJNAT1 tag 34 the canonical high-water mark at
       44 retained primitives.  Mermaid tag 108 remains bounded at 19 after
       DRAW_GETPUT replacement, so its fix does not depend on this capacity. */
    TTM_LAYER_DRAW_COMMANDS = 48,
    TTM_COMPLETION_PROBE_ENTRIES = 64,
    LOGIC_PERIOD_CENTISECONDS = 10,
    CENTISECOND_US = 10000,
    PRESENTATION_FRAMES_PER_SECOND = 30,
    PRESENTATION_PERIOD_MS = 1000 / PRESENTATION_FRAMES_PER_SECOND,
    TTM_TRANSPARENT_RGB565 = 0xa815,
    REVIEW_SCHEMA_VERSION = 2,
    STORY_SCHEMA_VERSION = 1,
};

typedef struct {
    uint16_t opcodes[8];
    size_t count;
} ttm_probe_t;

typedef struct {
    uint16_t opcode;
    uint16_t args[4];
    uint8_t color;
    uint8_t background_color;
    uint16_t clip_x;
    uint16_t clip_y;
    uint16_t clip_width;
    uint16_t clip_height;
} ttm_layer_draw_t;

typedef struct ttm_runtime ttm_runtime_t;
typedef struct ttm_runtime_resource ttm_runtime_resource_t;

struct ttm_runtime_resource {
    uint16_t ads_slot;
    const jcrez_ttm_t *ttm;
    jcrez_bitmap_t bitmap_slots[TTM_BITMAP_SLOTS];
};

typedef struct {
    ttm_runtime_t *runtime;
    jcengine_ttm_thread_t engine;
    ttm_runtime_resource_t *resource;
    uint16_t ads_slot;
    uint16_t remaining_plays;
    uint16_t lifetime_remaining_cs;
    bool allocated;
    ttm_layer_draw_t draws[TTM_LAYER_DRAW_COMMANDS];
    size_t draw_count;
    uint16_t clip_x;
    uint16_t clip_y;
    uint16_t clip_width;
    uint16_t clip_height;
    bool missing_bitmap_reported;
} ttm_runtime_thread_t;

struct ttm_runtime {
    const jcrez_archive_t *archive;
    const jcrez_palette_t *palette;
    uint16_t *framebuffer;
    uint16_t *background_framebuffer;
    uint16_t *screen_framebuffer;
    uint16_t *stored_framebuffer;
    char active_screen[14];
    char active_screen_sha256[65];
    jcengine_island_state_t island;
    int16_t ttm_offset_x;
    int16_t ttm_offset_y;
    jcengine_local_time_t local_time;
    jcengine_sky_mode_t sky_mode;
    jccontrol_holiday_mode_t holiday_mode;
    uint8_t story_day;
    int32_t story_date_key;
    uint8_t cycle_position;
    uint32_t cycle_block;
    bool block_anchor_valid;
    bool block_anchor_refresh;
    int16_t block_anchor_x;
    int16_t block_anchor_y;
    jcrez_bitmap_t island_bitmap;
    jcrez_bitmap_t raft_bitmap;
    jcrez_bitmap_t holiday_bitmap;
    bool island_assets_loaded;
    bool island_dynamic_background;
    size_t stored_pixel_peak;
    size_t store_area_count;
    size_t draw_screen_count;
    size_t line_draw_count;
    size_t draw_command_peak;
    bool sandcastle_clip_retained;
    bool honor_scene_lifetimes;
    ttm_runtime_resource_t resources[TTM_RUNTIME_RESOURCE_SLOTS];
    size_t resource_count;
    ttm_runtime_thread_t threads[TTM_RUNTIME_THREADS];
};

typedef struct {
    uint16_t slots[TTM_COMPLETION_PROBE_ENTRIES];
    uint16_t tags[TTM_COMPLETION_PROBE_ENTRIES];
    size_t count;
    uint64_t sequence_hash;
} ttm_completion_probe_t;

typedef struct {
    jcrez_ads_t ads;
    jcrez_ttm_t ttms[TTM_RUNTIME_RESOURCE_SLOTS];
    size_t ttm_count;
} live_ads_resources_t;

typedef struct {
    const char *ads_name;
    uint16_t ads_tag;
    size_t completion_count;
    uint64_t completion_hash;
    bool stop_requested;
    const char *framebuffer_sha256;
    size_t draw_command_peak;
    size_t store_area_count;
    uint32_t checkpoint_frame;
    const char *checkpoint_sha256;
} live_story_fixture_t;

static const live_story_fixture_t VALIDATED_STORY_FIXTURES[] = {
    {"ACTIVITY.ADS", 5, 12,
     UINT64_C(0x1ad4a9c4da1c5890), true,
     "3ad23e5185673fb005ea6b97e05b7774717d60c5223b0c8efb2319d451a6059f",
     0, 0, 0, ""},
    {"ACTIVITY.ADS", 6, 8,
     UINT64_C(0x915d4f4942d43d59), false,
     "d5bb5d995cb29312223ef8db956721c1bd92eaf8c57f52d013614898512c70cb",
     0, 0, 0, ""},
    {"ACTIVITY.ADS", 9, 11,
     UINT64_C(0x4545227af5fef6f9), true,
     "f9ae986715a68ef01a6ecc45306da3096ad74b3814a78301d29017f221ef6bf1",
     0, 0, 0, ""},
    {"BUILDING.ADS", 4, 13,
     UINT64_C(0xdc2d0b9a2c9fb92f), true,
     "7d01cb3e7f06a1f94b7faf868429c939cefb54d85cbc5ce738cc866045e66c3b",
     0, 0, 0, ""},
    {"BUILDING.ADS", 2, 229,
     UINT64_C(0x0a28a08c648c4106), true,
     "c9f1bee0b88bbfebe42548f99b05a7e584bbb985945d90ba656aff93d1692a58",
     0, 0, 0, ""},
    {"MARY.ADS", 2, 6, UINT64_C(0xf260bcfd7aceb82c), true,
     "8daf761ddbc521d36006ffc908a19782ab698fe3cb2ef84351695a6edc233ff6",
     35, 0, 356,
     "a7c01d90a09066b595264daaa1002a1eb4a84fe39b98a4fdf68f0119f054384e"},
    {"VISITOR.ADS", 3, 6, UINT64_C(0x0dad8160f527881c), true,
     "7c5131014f34a82d4b6dae3cb75e10ec064e2b312eee078c99d7cdcf9a9dddc4",
     3, 28, 211,
     "7448ff5b3fdb77866838057c53da2fd8d2c6a7b8b63432f95f359e93e516bdc7"},
    {"VISITOR.ADS", 5, 7, UINT64_C(0xd966344fd1d608f5), true,
     "7af0ddbf09adeb8d6e9b1ffd7d7b8d75c880f7eedc0b4e4fa4071244ae88ed77",
     3, 0, 2,
     "b4563885c77843e0e2b77cedb3665e732b4de654805447cb34573e6c5f065bfb"},
};

enum {
    VALIDATED_STORY_FIXTURE_COUNT =
        sizeof(VALIDATED_STORY_FIXTURES) /
        sizeof(VALIDATED_STORY_FIXTURES[0]),
    LIVE_STORY_COUNT = JCENGINE_STORY_SCENE_COUNT,
};

typedef struct {
    size_t slice_index;
    const jcengine_story_scene_t *scene;
    char title[32];
    bool paused;
    uint32_t elapsed_centiseconds;
    uint32_t displayed_frame;
    int64_t animation_clock_us;
    int64_t animation_remainder_us;
    int64_t started_us;
    ttm_completion_probe_t probe;
    bool verified[LIVE_STORY_COUNT];
    jcgfx_validation_review_t reviews[LIVE_STORY_COUNT];
    int32_t failure_errors[LIVE_STORY_COUNT];
    bool review_complete;
#if !CONFIG_JOHNNY_REVIEW_ONLY
    jccontrol_shuffle_t shuffle;
    jccontrol_playback_mode_t playback_mode;
    jccontrol_sidebar_mode_t sidebar_mode;
#endif
#if CONFIG_JOHNNY_REVIEW_ONLY
    uint64_t review_pass_pending;
    bool all_resolved;
#endif
} live_story_state_t;

static esp_err_t sha256_hex(const void *data, size_t size, char output[65]);
static esp_err_t compose_ttm_runtime(ttm_runtime_t *runtime);

static void clear_stored_framebuffer(ttm_runtime_t *runtime)
{
    if (runtime == NULL || runtime->stored_framebuffer == NULL) return;
    for (size_t index = 0; index < JCBOARD_WIDTH * JCBOARD_HEIGHT; ++index) {
        runtime->stored_framebuffer[index] = TTM_TRANSPARENT_RGB565;
    }
}

static esp_err_t load_runtime_screen(ttm_runtime_t *runtime, const char *name)
{
    ESP_RETURN_ON_FALSE(runtime != NULL && runtime->archive != NULL &&
                            runtime->palette != NULL &&
                            runtime->background_framebuffer != NULL &&
                            runtime->screen_framebuffer != NULL && name != NULL &&
                            name[0] != '\0',
                        ESP_ERR_INVALID_ARG, TAG, "invalid runtime screen");
    jcrez_screen_t screen = {0};
    ESP_RETURN_ON_ERROR(jcrez_load_screen(runtime->archive, name, &screen), TAG,
                        "load runtime SCR");
    esp_err_t err = jcgfx_render_screen(
        runtime->screen_framebuffer, JCBOARD_WIDTH, JCBOARD_HEIGHT,
        JCGFX_LAYOUT_RIGHT, runtime->palette, &screen);
    if (err == ESP_OK) {
        memcpy(runtime->background_framebuffer, runtime->screen_framebuffer,
               JCBOARD_WIDTH * JCBOARD_HEIGHT * sizeof(*runtime->background_framebuffer));
        memcpy(runtime->active_screen, name, sizeof(runtime->active_screen) - 1);
        runtime->active_screen[sizeof(runtime->active_screen) - 1] = '\0';
        err = sha256_hex(runtime->background_framebuffer,
                         JCBOARD_WIDTH * JCBOARD_HEIGHT *
                             sizeof(*runtime->background_framebuffer),
                         runtime->active_screen_sha256);
    }
    jcrez_release_screen(&screen);
    if (err != ESP_OK) return err;
    clear_stored_framebuffer(runtime);
    ESP_LOGI(TAG, "ENGINE: SCR %s framebuffer_sha256=%s",
             runtime->active_screen, runtime->active_screen_sha256);
    return ESP_OK;
}

static esp_err_t ensure_island_assets(ttm_runtime_t *runtime)
{
    if (runtime->island_assets_loaded) return ESP_OK;
    esp_err_t err = jcrez_load_bitmap(runtime->archive, "BACKGRND.BMP",
                                      &runtime->island_bitmap);
    if (err == ESP_OK) {
        err = jcrez_load_bitmap(runtime->archive, "MRAFT.BMP",
                                &runtime->raft_bitmap);
    }
    if (err == ESP_OK) {
        err = jcrez_load_bitmap(runtime->archive, "HOLIDAY.BMP",
                                &runtime->holiday_bitmap);
    }
    if (err != ESP_OK) {
        jcrez_release_bitmap(&runtime->holiday_bitmap);
        jcrez_release_bitmap(&runtime->raft_bitmap);
        jcrez_release_bitmap(&runtime->island_bitmap);
        return err;
    }
    runtime->island_assets_loaded = true;
    return ESP_OK;
}

static esp_err_t draw_runtime_sprite(ttm_runtime_t *runtime,
                                     uint16_t *destination,
                                     const jcrez_bitmap_t *bitmap,
                                     size_t sprite, int x, int y, bool flip)
{
    return jcgfx_draw_bitmap_sprite(destination, JCBOARD_WIDTH, JCBOARD_HEIGHT,
                                    JCGFX_LAYOUT_RIGHT, runtime->palette,
                                    bitmap, sprite, x, y, flip);
}

static esp_err_t render_island_background(ttm_runtime_t *runtime)
{
    if (!runtime->island.active) return ESP_OK;
    ESP_RETURN_ON_FALSE(runtime->screen_framebuffer != NULL,
                        ESP_ERR_INVALID_STATE, TAG, "island SCR is unavailable");
    memcpy(runtime->background_framebuffer, runtime->screen_framebuffer,
           JCBOARD_WIDTH * JCBOARD_HEIGHT * sizeof(*runtime->background_framebuffer));
    int dx = runtime->island.offset_x;
    int dy = runtime->island.offset_y;
    if (runtime->island.raft_stage > 0) {
        int raft_x = runtime->island.low_tide ? 529 : 512;
        int raft_y = runtime->island.low_tide ? 281 : 266;
        ESP_RETURN_ON_ERROR(draw_runtime_sprite(
                                runtime, runtime->background_framebuffer,
                                &runtime->raft_bitmap,
                                runtime->island.raft_stage - 1,
                                raft_x + dx, raft_y + dy, false),
                            TAG, "draw island raft");
    }
    static const struct { uint8_t sprite; int16_t x; int16_t y; } ISLAND[] = {
        {0, 288, 279}, {13, 442, 148}, {12, 365, 122}, {14, 396, 279},
    };
    for (size_t index = 0; index < sizeof(ISLAND) / sizeof(ISLAND[0]); ++index) {
        ESP_RETURN_ON_ERROR(draw_runtime_sprite(
                                runtime, runtime->background_framebuffer,
                                &runtime->island_bitmap, ISLAND[index].sprite,
                                ISLAND[index].x + dx, ISLAND[index].y + dy, false),
                            TAG, "draw island base");
    }
    if (runtime->island.low_tide) {
        ESP_RETURN_ON_ERROR(draw_runtime_sprite(runtime, runtime->background_framebuffer,
                                                &runtime->island_bitmap, 1,
                                                249 + dx, 303 + dy, false),
                            TAG, "draw low-tide shore");
        ESP_RETURN_ON_ERROR(draw_runtime_sprite(runtime, runtime->background_framebuffer,
                                                &runtime->island_bitmap, 2,
                                                150 + dx, 328 + dy, false),
                            TAG, "draw low-tide rock");
    }
    static const uint8_t HIGH_BASE[] = {3, 6, 9};
    static const int16_t HIGH_X[] = {270, 364, 518};
    static const int16_t HIGH_Y[] = {306, 319, 303};
    static const uint8_t LOW_BASE[] = {39, 30, 33, 36};
    static const int16_t LOW_X[] = {129, 233, 367, 558};
    static const int16_t LOW_Y[] = {340, 323, 356, 323};
    uint8_t wave_count = runtime->island.low_tide ? 4 : 3;
    for (uint8_t slot = 0; slot < wave_count; ++slot) {
        uint8_t sprite = runtime->island.low_tide
                             ? (uint8_t)(LOW_BASE[slot] + runtime->island.wave_phases[slot])
                             : (uint8_t)(HIGH_BASE[slot] + runtime->island.wave_phases[slot]);
        int wave_x = runtime->island.low_tide ? LOW_X[slot] : HIGH_X[slot];
        int wave_y = runtime->island.low_tide ? LOW_Y[slot] : HIGH_Y[slot];
        ESP_RETURN_ON_ERROR(draw_runtime_sprite(
                                runtime, runtime->background_framebuffer,
                                &runtime->island_bitmap, sprite,
                                wave_x + dx, wave_y + dy, false),
                            TAG, "draw island wave");
    }
    return ESP_OK;
}

static void select_ttm_origin(const jcengine_island_state_t *island,
                              const jcengine_story_scene_t *scene,
                              int16_t *offset_x, int16_t *offset_y)
{
    if (island == NULL || scene == NULL || offset_x == NULL ||
        offset_y == NULL || !island->active) {
        if (offset_x != NULL) *offset_x = 0;
        if (offset_y != NULL) *offset_y = 0;
        return;
    }
    *offset_x = island->offset_x;
    *offset_y = island->offset_y;
    if ((scene->flags & JCENGINE_STORY_LEFT_ISLAND) != 0) {
        /* The desktop engine shifts the TTM origin back to the right after
           moving the island left, keeping Johnny aligned with the shore. */
        *offset_x += 272;
    }
}

static jcengine_sky_mode_t effective_sky_mode(const ttm_runtime_t *runtime)
{
    if (runtime->sky_mode != JCENGINE_SKY_CYCLE) return runtime->sky_mode;
    return (runtime->cycle_block & 1U) == 0 ? JCENGINE_SKY_DAY
                                            : JCENGINE_SKY_NIGHT;
}

static int16_t clamp_island_step(int16_t candidate, int16_t previous,
                                 int16_t limit)
{
    if (candidate < previous - limit) return previous - limit;
    if (candidate > previous + limit) return previous + limit;
    return candidate;
}

static void apply_block_island_anchor(
    ttm_runtime_t *runtime, const jcengine_story_scene_t *scene)
{
    if (!runtime->island.active ||
        (scene->flags & JCENGINE_STORY_VARPOS_OK) == 0) {
        return;
    }
    if (!runtime->block_anchor_valid) {
        runtime->block_anchor_x = runtime->island.offset_x;
        runtime->block_anchor_y = runtime->island.offset_y;
        runtime->block_anchor_valid = true;
        runtime->block_anchor_refresh = false;
    } else if (runtime->block_anchor_refresh) {
        runtime->block_anchor_x = clamp_island_step(
            runtime->island.offset_x, runtime->block_anchor_x, 64);
        runtime->block_anchor_y = clamp_island_step(
            runtime->island.offset_y, runtime->block_anchor_y, 32);
        runtime->block_anchor_refresh = false;
    }
    runtime->island.offset_x = runtime->block_anchor_x;
    runtime->island.offset_y = runtime->block_anchor_y;
}

static void advance_story_block(ttm_runtime_t *runtime)
{
    if (++runtime->cycle_position < 10) return;
    runtime->cycle_position = 0;
    ++runtime->cycle_block;
    runtime->block_anchor_refresh = true;
}

static esp_err_t initialize_runtime_island(
    ttm_runtime_t *runtime, const jcengine_story_scene_t *scene)
{
    ESP_RETURN_ON_FALSE(runtime != NULL && scene != NULL, ESP_ERR_INVALID_ARG,
                        TAG, "invalid island event");
    uint32_t seed = jcengine_story_event_seed(
        scene->ads_name, scene->ads_tag,
        runtime->story_day >= 1 ? runtime->story_day : 1);
    jcengine_island_initialize(&runtime->island, scene, runtime->story_day,
                               &runtime->local_time,
                               effective_sky_mode(runtime), seed);
    apply_block_island_anchor(runtime, scene);
    if ((scene->flags & JCENGINE_STORY_HOLIDAY_NOK) != 0 ||
        runtime->holiday_mode == JCCONTROL_HOLIDAY_OFF) {
        runtime->island.holiday_id = 0;
    } else if (runtime->holiday_mode >= JCCONTROL_HOLIDAY_HALLOWEEN) {
        runtime->island.holiday_id =
            (uint8_t)(runtime->holiday_mode - JCCONTROL_HOLIDAY_AUTOMATIC);
    }
    select_ttm_origin(&runtime->island, scene, &runtime->ttm_offset_x,
                      &runtime->ttm_offset_y);
    if (!runtime->island.active) {
        runtime->island_dynamic_background = false;
        memset(runtime->screen_framebuffer, 0,
               JCBOARD_WIDTH * JCBOARD_HEIGHT * sizeof(*runtime->screen_framebuffer));
        memset(runtime->background_framebuffer, 0,
               JCBOARD_WIDTH * JCBOARD_HEIGHT * sizeof(*runtime->background_framebuffer));
        runtime->active_screen[0] = '\0';
        runtime->active_screen_sha256[0] = '\0';
        clear_stored_framebuffer(runtime);
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(ensure_island_assets(runtime), TAG, "load island assets");
    runtime->island_dynamic_background = true;
    char screen_name[14] = {0};
    if (runtime->island.night) {
        memcpy(screen_name, "NIGHT.SCR", sizeof("NIGHT.SCR"));
    } else {
        snprintf(screen_name, sizeof(screen_name), "OCEAN0%u.SCR",
                 runtime->island.ocean_index);
    }
    ESP_RETURN_ON_ERROR(load_runtime_screen(runtime, screen_name), TAG,
                        "load island SCR");
    ESP_RETURN_ON_ERROR(render_island_background(runtime), TAG,
                        "render island background");
    ESP_LOGI(TAG,
             "ENGINE: island screen=%s day=%u night=%u tide=%u raft=%u offset=%d,%d ttm_offset=%d,%d clouds=%u holiday=%u seed=%08" PRIx32,
             runtime->active_screen, runtime->island.story_day,
             runtime->island.night ? 1U : 0U,
             runtime->island.low_tide ? 1U : 0U,
             runtime->island.raft_stage, runtime->island.offset_x,
             runtime->island.offset_y, runtime->ttm_offset_x,
             runtime->ttm_offset_y, runtime->island.cloud_count,
             runtime->island.holiday_id, runtime->island.seed);
    return ESP_OK;
}

static bool runtime_night(const ttm_runtime_t *runtime)
{
    if (runtime->sky_mode == JCENGINE_SKY_CYCLE) {
        return (runtime->cycle_block & 1U) != 0;
    }
    if (runtime->sky_mode == JCENGINE_SKY_DAY) return false;
    if (runtime->sky_mode == JCENGINE_SKY_NIGHT) return true;
    return runtime->local_time.valid &&
           (runtime->local_time.hour < 6 || runtime->local_time.hour >= 18);
}

static esp_err_t refresh_runtime_theme(
    ttm_runtime_t *runtime, const jcengine_story_scene_t *scene)
{
    ESP_RETURN_ON_FALSE(runtime != NULL && scene != NULL, ESP_ERR_INVALID_ARG,
                        TAG, "invalid theme refresh");
    if (!runtime->island.active) return ESP_OK;
    bool night = runtime_night(runtime);
    if (night != runtime->island.night) {
        runtime->island.night = night;
        char screen_name[14] = {0};
        if (night) {
            memcpy(screen_name, "NIGHT.SCR", sizeof("NIGHT.SCR"));
        } else {
            snprintf(screen_name, sizeof(screen_name), "OCEAN0%u.SCR",
                     runtime->island.ocean_index);
        }
        ESP_RETURN_ON_ERROR(load_runtime_screen(runtime, screen_name), TAG,
                            "switch active sky");
    }
    if ((scene->flags & JCENGINE_STORY_HOLIDAY_NOK) != 0 ||
        runtime->holiday_mode == JCCONTROL_HOLIDAY_OFF) {
        runtime->island.holiday_id = 0;
    } else if (runtime->holiday_mode == JCCONTROL_HOLIDAY_AUTOMATIC) {
        const jcengine_special_day_t *special =
            runtime->local_time.valid
                ? jcengine_special_day_for_date(runtime->local_time.month,
                                                runtime->local_time.day)
                : NULL;
        runtime->island.holiday_id = special == NULL ? 0 : special->overlay_id;
    } else {
        runtime->island.holiday_id =
            (uint8_t)(runtime->holiday_mode - JCCONTROL_HOLIDAY_AUTOMATIC);
    }
    ESP_RETURN_ON_ERROR(render_island_background(runtime), TAG,
                        "render changed theme");
    return compose_ttm_runtime(runtime);
}

static void clear_stored_region(ttm_runtime_t *runtime, size_t x, size_t y,
                                size_t width, size_t height)
{
    if (runtime == NULL || runtime->stored_framebuffer == NULL ||
        x >= JCBOARD_WIDTH || y >= JCBOARD_HEIGHT) {
        return;
    }
    size_t x_end = x + width < JCBOARD_WIDTH ? x + width : JCBOARD_WIDTH;
    size_t y_end = y + height < JCBOARD_HEIGHT ? y + height : JCBOARD_HEIGHT;
    for (size_t py = y; py < y_end; ++py) {
        for (size_t px = x; px < x_end; ++px) {
            runtime->stored_framebuffer[py * JCBOARD_WIDTH + px] =
                TTM_TRANSPARENT_RGB565;
        }
    }
}

static bool map_ttm_region_to_panel(const ttm_runtime_t *runtime, int32_t x,
                                    int32_t y, size_t width, size_t height,
                                    size_t *panel_x, size_t *panel_y,
                                    size_t *panel_width, size_t *panel_height)
{
    if (runtime == NULL || panel_x == NULL || panel_y == NULL ||
        panel_width == NULL || panel_height == NULL || width == 0 ||
        height == 0) {
        return false;
    }
    int32_t left = x + runtime->ttm_offset_x;
    int32_t top = y + runtime->ttm_offset_y;
    int32_t right = left + (int32_t)width;
    int32_t bottom = top + (int32_t)height;
    if (left < 0) left = 0;
    if (top < 0) top = 0;
    if (right > 640) right = 640;
    if (bottom > JCBOARD_HEIGHT) bottom = JCBOARD_HEIGHT;
    if (left >= right || top >= bottom) return false;
    *panel_x = (size_t)left;
    *panel_y = (size_t)top;
    *panel_width = (size_t)(right - left);
    *panel_height = (size_t)(bottom - top);
    return true;
}

static void reset_ttm_thread_clip(ttm_runtime_thread_t *thread)
{
    if (thread == NULL) return;
    thread->clip_x = 0;
    thread->clip_y = 0;
    thread->clip_width = 640;
    thread->clip_height = JCBOARD_HEIGHT;
}

static bool intersect_regions(size_t ax, size_t ay, size_t aw, size_t ah,
                              size_t bx, size_t by, size_t bw, size_t bh,
                              size_t *x, size_t *y, size_t *width,
                              size_t *height)
{
    size_t left = ax > bx ? ax : bx;
    size_t top = ay > by ? ay : by;
    size_t a_right = ax + aw;
    size_t b_right = bx + bw;
    size_t a_bottom = ay + ah;
    size_t b_bottom = by + bh;
    size_t right = a_right < b_right ? a_right : b_right;
    size_t bottom = a_bottom < b_bottom ? a_bottom : b_bottom;
    if (left >= right || top >= bottom) return false;
    if (x != NULL) *x = left;
    if (y != NULL) *y = top;
    if (width != NULL) *width = right - left;
    if (height != NULL) *height = bottom - top;
    return true;
}

static bool ttm_draw_bounds(const ttm_runtime_thread_t *thread,
                            const ttm_layer_draw_t *draw, size_t *x,
                            size_t *y, size_t *width, size_t *height)
{
    if (thread == NULL || thread->runtime == NULL || draw == NULL) return false;
    int32_t left = 0;
    int32_t top = 0;
    int32_t right = 0;
    int32_t bottom = 0;
    if (draw->opcode == 0xa0a4) {
        int32_t x1 = (int16_t)draw->args[0] + thread->runtime->ttm_offset_x;
        int32_t y1 = (int16_t)draw->args[1] + thread->runtime->ttm_offset_y;
        int32_t x2 = (int16_t)draw->args[2] + thread->runtime->ttm_offset_x;
        int32_t y2 = (int16_t)draw->args[3] + thread->runtime->ttm_offset_y;
        left = x1 < x2 ? x1 : x2;
        top = y1 < y2 ? y1 : y2;
        right = (x1 > x2 ? x1 : x2) + 1;
        bottom = (y1 > y2 ? y1 : y2) + 1;
    } else if (draw->opcode == 0xa104) {
        left = (int16_t)draw->args[0] + thread->runtime->ttm_offset_x;
        top = (int16_t)draw->args[1] + thread->runtime->ttm_offset_y;
        right = left + draw->args[2];
        bottom = top + draw->args[3];
    } else if (draw->opcode == 0xa404) {
        int32_t radius = draw->args[2];
        int32_t center_x =
            (int16_t)draw->args[0] + thread->runtime->ttm_offset_x;
        int32_t center_y =
            (int16_t)draw->args[1] + thread->runtime->ttm_offset_y;
        left = center_x - radius;
        top = center_y - radius;
        right = center_x + radius + 2;
        bottom = center_y + radius + 2;
    } else {
        uint16_t slot = draw->args[3];
        if (thread->resource == NULL || slot >= TTM_BITMAP_SLOTS ||
            draw->args[2] >= thread->resource->bitmap_slots[slot].sprite_count) {
            return false;
        }
        const jcrez_bitmap_sprite_t *sprite =
            &thread->resource->bitmap_slots[slot].sprites[draw->args[2]];
        left = (int16_t)draw->args[0] + thread->runtime->ttm_offset_x;
        top = (int16_t)draw->args[1] + thread->runtime->ttm_offset_y;
        right = left + sprite->width;
        bottom = top + sprite->height;
    }
    if (left < 0) left = 0;
    if (top < 0) top = 0;
    if (right > 640) right = 640;
    if (bottom > JCBOARD_HEIGHT) bottom = JCBOARD_HEIGHT;
    if (left >= right || top >= bottom) return false;
    if (x != NULL) *x = (size_t)left;
    if (y != NULL) *y = (size_t)top;
    if (width != NULL) *width = (size_t)(right - left);
    if (height != NULL) *height = (size_t)(bottom - top);
    return true;
}

static void clear_ttm_thread_clip(ttm_runtime_thread_t *thread)
{
    if (thread == NULL || thread->draw_count == 0) return;
    size_t retained = 0;
    for (size_t index = 0; index < thread->draw_count; ++index) {
        size_t x = 0;
        size_t y = 0;
        size_t width = 0;
        size_t height = 0;
        if (ttm_draw_bounds(thread, &thread->draws[index], &x, &y, &width,
                            &height) &&
            intersect_regions(x, y, width, height, thread->clip_x,
                              thread->clip_y, thread->clip_width,
                              thread->clip_height, NULL, NULL, NULL, NULL)) {
            continue;
        }
        if (retained != index) thread->draws[retained] = thread->draws[index];
        const ttm_layer_draw_t *draw = &thread->draws[retained];
        if (thread->resource != NULL &&
            strcmp(thread->resource->ttm->name, "MJSAND.TTM") == 0 &&
            draw->opcode == 0xa524 && draw->args[0] == 203 &&
            draw->args[1] == 301 && draw->args[2] == 12 &&
            draw->args[3] == 4) {
            thread->runtime->sandcastle_clip_retained = true;
        }
        ++retained;
    }
    thread->draw_count = retained;
}

static esp_err_t draw_thread_region(ttm_runtime_thread_t *thread,
                                    uint16_t *destination, size_t x, size_t y,
                                    size_t width, size_t height)
{
    ESP_RETURN_ON_FALSE(thread != NULL && thread->runtime != NULL &&
                            destination != NULL,
                        ESP_ERR_INVALID_ARG, TAG, "invalid TTM region draw");
    size_t x_end = x + width < JCBOARD_WIDTH ? x + width : JCBOARD_WIDTH;
    size_t y_end = y + height < JCBOARD_HEIGHT ? y + height : JCBOARD_HEIGHT;
    if (x >= x_end || y >= y_end) return ESP_OK;
    for (size_t draw_index = 0; draw_index < thread->draw_count; ++draw_index) {
        const ttm_layer_draw_t *draw = &thread->draws[draw_index];
        size_t clip_x = 0;
        size_t clip_y = 0;
        size_t clip_width = 0;
        size_t clip_height = 0;
        if (!intersect_regions(x, y, x_end - x, y_end - y, draw->clip_x,
                               draw->clip_y, draw->clip_width,
                               draw->clip_height, &clip_x, &clip_y,
                               &clip_width, &clip_height)) {
            continue;
        }
        if (draw->opcode == 0xa0a4) {
            ESP_RETURN_ON_ERROR(
                jcgfx_draw_line_clipped(
                    destination, JCBOARD_WIDTH, JCBOARD_HEIGHT,
                    JCGFX_LAYOUT_RIGHT, thread->runtime->palette, draw->color,
                    (int16_t)draw->args[0] + thread->runtime->ttm_offset_x,
                    (int16_t)draw->args[1] + thread->runtime->ttm_offset_y,
                    (int16_t)draw->args[2] + thread->runtime->ttm_offset_x,
                    (int16_t)draw->args[3] + thread->runtime->ttm_offset_y,
                    clip_x, clip_y, clip_width, clip_height),
                TAG, "draw TTM stored-area line");
            continue;
        }
        if (draw->opcode == 0xa104) {
            ESP_RETURN_ON_ERROR(
                jcgfx_draw_rect_clipped(
                    destination, JCBOARD_WIDTH, JCBOARD_HEIGHT,
                    JCGFX_LAYOUT_RIGHT, thread->runtime->palette, draw->color,
                    (int16_t)draw->args[0] + thread->runtime->ttm_offset_x,
                    (int16_t)draw->args[1] + thread->runtime->ttm_offset_y,
                    draw->args[2], draw->args[3], clip_x, clip_y, clip_width,
                    clip_height),
                TAG, "draw TTM stored-area rectangle");
            continue;
        }
        if (draw->opcode == 0xa404) {
            ESP_RETURN_ON_ERROR(
                jcgfx_draw_circle_clipped(
                    destination, JCBOARD_WIDTH, JCBOARD_HEIGHT,
                    JCGFX_LAYOUT_RIGHT, thread->runtime->palette, draw->color,
                    draw->background_color,
                    (int16_t)draw->args[0] + thread->runtime->ttm_offset_x,
                    (int16_t)draw->args[1] + thread->runtime->ttm_offset_y,
                    draw->args[2], draw->args[3], clip_x, clip_y, clip_width,
                    clip_height),
                TAG, "draw TTM stored-area circle");
            continue;
        }
        uint16_t slot = draw->args[3];
        if (thread->resource == NULL || slot >= TTM_BITMAP_SLOTS ||
            thread->resource->bitmap_slots[slot].packed_pixels == NULL) {
            if (!thread->missing_bitmap_reported) {
                ESP_LOGW(TAG,
                         "TTM draw skipped missing bitmap resource=%s slot=%u",
                         thread->resource == NULL
                             ? "UNKNOWN"
                             : thread->resource->ttm->name,
                         slot);
                thread->missing_bitmap_reported = true;
            }
            continue;
        }
        if (draw->args[2] >=
            thread->resource->bitmap_slots[slot].sprite_count) {
            continue;
        }
        ESP_RETURN_ON_ERROR(
            jcgfx_draw_bitmap_sprite_clipped(
                destination, JCBOARD_WIDTH, JCBOARD_HEIGHT,
                JCGFX_LAYOUT_RIGHT, thread->runtime->palette,
                &thread->resource->bitmap_slots[slot], draw->args[2],
                (int16_t)draw->args[0] + thread->runtime->ttm_offset_x,
                (int16_t)draw->args[1] + thread->runtime->ttm_offset_y,
                draw->opcode == 0xa524, clip_x, clip_y, clip_width,
                clip_height),
            TAG, "draw TTM stored-area sprite");
    }
    return ESP_OK;
}

static void update_stored_pixel_peak(ttm_runtime_t *runtime)
{
    size_t count = 0;
    for (size_t index = 0; index < JCBOARD_WIDTH * JCBOARD_HEIGHT; ++index) {
        if (runtime->stored_framebuffer[index] != TTM_TRANSPARENT_RGB565) {
            ++count;
        }
    }
    if (count > runtime->stored_pixel_peak) runtime->stored_pixel_peak = count;
}

static esp_err_t verify_story_selector(void)
{
    ESP_RETURN_ON_FALSE(
        jcengine_story_scene_count() == JCENGINE_STORY_SCENE_COUNT,
                        ESP_ERR_INVALID_SIZE, TAG,
                        "story catalog count changed");
    const jcengine_story_scene_t *scene = jcengine_story_scene(0);
    ESP_RETURN_ON_FALSE(scene != NULL &&
                            strcmp(scene->ads_name, "ACTIVITY.ADS") == 0 &&
                            scene->ads_tag == 1 && scene->day == 0,
                        ESP_ERR_INVALID_RESPONSE, TAG,
                        "story catalog first scene changed");
    ESP_RETURN_ON_FALSE(
        jcengine_story_scene(JCENGINE_STORY_SCENE_COUNT) == NULL,
                        ESP_ERR_INVALID_RESPONSE, TAG,
                        "story catalog bounds check failed");
#if CONFIG_JC_SCENE_NAMES
    ESP_RETURN_ON_FALSE(jcengine_scene_menu_count() == JCENGINE_STORY_SCENE_COUNT,
                        ESP_ERR_INVALID_SIZE, TAG,
                        "generated scene menu count changed");
    for (size_t index = 0; index < JCENGINE_STORY_SCENE_COUNT; ++index) {
        scene = jcengine_story_scene(index);
        ESP_RETURN_ON_FALSE(
            scene != NULL &&
                jcengine_scene_title(scene->ads_name, scene->ads_tag) != NULL,
            ESP_ERR_NOT_FOUND, TAG, "story scene friendly title missing");
    }
    ESP_RETURN_ON_FALSE(
        strcmp(jcengine_scene_title("ACTIVITY.ADS", 5), "RAIN DANCE") == 0 &&
            jcengine_scene_title("UNKNOWN.ADS", 1) == NULL,
        ESP_ERR_INVALID_RESPONSE, TAG, "generated scene title fixture changed");
#endif
    const jcengine_special_day_t *special_day =
        jcengine_special_day_for_date(12, 31);
    ESP_RETURN_ON_FALSE(
        jcengine_special_day_count() == 4 && special_day != NULL &&
            special_day->overlay_id == 4 &&
            jcengine_special_day_for_date(1, 1) == special_day &&
            jcengine_special_day_for_date(1, 2) == NULL,
        ESP_ERR_INVALID_RESPONSE, TAG, "Special Day date fixtures changed");

    size_t cursor = 0;
    ESP_RETURN_ON_ERROR(
        jcengine_story_pick_in_order(0, JCENGINE_STORY_FINAL, 1,
                                     &cursor, &scene),
        TAG, "pick non-final story scene");
    ESP_RETURN_ON_FALSE(strcmp(scene->ads_name, "ACTIVITY.ADS") == 0 &&
                            scene->ads_tag == 4 && cursor == 5,
                        ESP_ERR_INVALID_RESPONSE, TAG,
                        "story flag filter fixture changed");

    cursor = 25;
    ESP_RETURN_ON_ERROR(
        jcengine_story_pick_in_order(JCENGINE_STORY_FIRST, 0, 10,
                                     &cursor, &scene),
        TAG, "pick day-specific story scene");
    ESP_RETURN_ON_FALSE(strcmp(scene->ads_name, "JOHNNY.ADS") == 0 &&
                            scene->ads_tag == 6 && scene->day == 10 &&
                            cursor == 31,
                        ESP_ERR_INVALID_RESPONSE, TAG,
                        "story day filter fixture changed");

    cursor = 62;
    ESP_RETURN_ON_ERROR(
        jcengine_story_pick_in_order(0, JCENGINE_STORY_FINAL, 1,
                                     &cursor, &scene),
        TAG, "pick story scene at catalog end");
    ESP_RETURN_ON_FALSE(strcmp(scene->ads_name, "WALKSTUF.ADS") == 0 &&
                            scene->ads_tag == 3 && cursor == 0,
                        ESP_ERR_INVALID_RESPONSE, TAG,
                        "story cursor wrap fixture changed");
    ESP_RETURN_ON_ERROR(
        jcengine_story_pick_in_order(0, JCENGINE_STORY_FINAL, 1,
                                     &cursor, &scene),
        TAG, "pick story scene after cursor wrap");
    ESP_RETURN_ON_FALSE(strcmp(scene->ads_name, "ACTIVITY.ADS") == 0 &&
                            scene->ads_tag == 4 && cursor == 5,
                        ESP_ERR_INVALID_RESPONSE, TAG,
                        "story cursor continuation fixture changed");

    cursor = 17;
    scene = jcengine_story_scene(0);
    ESP_RETURN_ON_FALSE(
        jcengine_story_pick_in_order(JCENGINE_STORY_NORAFT |
                                         JCENGINE_STORY_HOLIDAY_NOK,
                                     0, 1, &cursor, &scene) ==
                ESP_ERR_NOT_FOUND &&
            scene == NULL && cursor == 17,
        ESP_ERR_INVALID_RESPONSE, TAG,
        "story no-match fixture changed");
    return ESP_OK;
}

static esp_err_t verify_web_control_foundation(void)
{
    jccontrol_shuffle_t shuffle = {.last_scene = -1};
    bool seen[LIVE_STORY_COUNT] = {0};
    jccontrol_shuffle_reset(&shuffle, UINT32_C(0x12345678));
    for (size_t index = 0; index < LIVE_STORY_COUNT; ++index) {
        size_t scene = jccontrol_shuffle_next(&shuffle, 0);
        ESP_RETURN_ON_FALSE(scene < LIVE_STORY_COUNT && !seen[scene],
                            ESP_ERR_INVALID_RESPONSE, TAG,
                            "shuffle cycle repeated a scene");
        seen[scene] = true;
    }
    int previous = shuffle.last_scene;
    jccontrol_shuffle_reset(&shuffle, UINT32_C(0x87654321));
    ESP_RETURN_ON_FALSE(
        shuffle.order[0] != previous &&
            jccontrol_shuffle_remaining(&shuffle) == LIVE_STORY_COUNT,
        ESP_ERR_INVALID_RESPONSE, TAG, "shuffle boundary repeated a scene");
    jcengine_sky_mode_t sky = JCENGINE_SKY_AUTOMATIC;
    jccontrol_holiday_mode_t holiday = JCCONTROL_HOLIDAY_OFF;
    jccontrol_playback_mode_t playback_mode = JCCONTROL_PLAYBACK_NORMAL;
    ESP_RETURN_ON_FALSE(
        jccontrol_parse_sky("night", &sky) &&
            sky == JCENGINE_SKY_NIGHT &&
            jccontrol_parse_sky("cycle", &sky) &&
            sky == JCENGINE_SKY_CYCLE &&
            jccontrol_parse_holiday("new_year", &holiday) &&
            holiday == JCCONTROL_HOLIDAY_NEW_YEAR &&
            jccontrol_parse_playback_mode("review", &playback_mode) &&
            playback_mode == JCCONTROL_PLAYBACK_REVIEW &&
            !jccontrol_parse_sky("invalid", &sky) &&
            !jccontrol_parse_holiday("easter", &holiday) &&
            !jccontrol_parse_playback_mode("invalid", &playback_mode),
        ESP_ERR_INVALID_RESPONSE, TAG, "web setting validation changed");

    ttm_runtime_t *block = heap_caps_calloc(
        1, sizeof(*block), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(block != NULL, ESP_ERR_NO_MEM, TAG,
                        "allocate story-block fixture");
    block->sky_mode = JCENGINE_SKY_CYCLE;
    ESP_RETURN_ON_FALSE(!runtime_night(block), ESP_ERR_INVALID_RESPONSE, TAG,
                        "Cycle did not start in Day");
    for (size_t index = 0; index < 10; ++index) advance_story_block(block);
    ESP_RETURN_ON_FALSE(block->cycle_position == 0 && block->cycle_block == 1 &&
                            runtime_night(block),
                        ESP_ERR_INVALID_RESPONSE, TAG,
                        "Cycle did not enter Night after ten scenes");
    for (size_t index = 0; index < 10; ++index) advance_story_block(block);
    ESP_RETURN_ON_FALSE(block->cycle_position == 0 && block->cycle_block == 2 &&
                            !runtime_night(block),
                        ESP_ERR_INVALID_RESPONSE, TAG,
                        "Cycle did not return to Day after twenty scenes");

    jcengine_story_scene_t variable = {
        .flags = JCENGINE_STORY_ISLAND | JCENGINE_STORY_VARPOS_OK,
    };
    block->island.active = true;
    block->island.offset_x = -200;
    block->island.offset_y = -40;
    apply_block_island_anchor(block, &variable);
    block->block_anchor_refresh = true;
    block->island.offset_x = 20;
    block->island.offset_y = 84;
    apply_block_island_anchor(block, &variable);
    ESP_RETURN_ON_FALSE(block->island.offset_x == -136 &&
                            block->island.offset_y == -8,
                        ESP_ERR_INVALID_RESPONSE, TAG,
                        "block island movement bounds changed");
    heap_caps_free(block);
    return ESP_OK;
}

static esp_err_t verify_island_lifecycle(void)
{
    const jcengine_story_scene_t scene = {
        .ads_name = "FIXTURE.ADS",
        .ads_tag = 7,
        .flags = JCENGINE_STORY_ISLAND | JCENGINE_STORY_LOWTIDE_OK |
                 JCENGINE_STORY_VARPOS_OK,
    };
    jcengine_local_time_t time = {
        .valid = true, .hour = 5, .month = 12, .day = 24,
        .date_key = 20261224,
    };
    jcengine_island_state_t first = {0};
    jcengine_island_state_t replay = {0};
    uint32_t fixture_seed = jcengine_story_event_seed(scene.ads_name,
                                                      scene.ads_tag, 6);
    jcengine_island_initialize(&first, &scene, 6, &time,
                               JCENGINE_SKY_AUTOMATIC, fixture_seed);
    jcengine_island_initialize(&replay, &scene, 6, &time,
                               JCENGINE_SKY_AUTOMATIC, fixture_seed);
    ESP_RETURN_ON_FALSE(
        first.active && first.night && first.raft_stage == 5 &&
            first.holiday_id == 3 && memcmp(&first, &replay, sizeof(first)) == 0,
        ESP_ERR_INVALID_RESPONSE, TAG,
        "deterministic island initialization fixture changed");

    int16_t ttm_x = 0;
    int16_t ttm_y = 0;
    select_ttm_origin(&first, &scene, &ttm_x, &ttm_y);
    ESP_RETURN_ON_FALSE(ttm_x == first.offset_x && ttm_y == first.offset_y,
                        ESP_ERR_INVALID_RESPONSE, TAG,
                        "ordinary island TTM origin fixture changed");
    jcengine_story_scene_t left_scene = scene;
    left_scene.flags |= JCENGINE_STORY_LEFT_ISLAND;
    select_ttm_origin(&first, &left_scene, &ttm_x, &ttm_y);
    ESP_RETURN_ON_FALSE(ttm_x == first.offset_x + 272 &&
                            ttm_y == first.offset_y,
                        ESP_ERR_INVALID_RESPONSE, TAG,
                        "left-island TTM origin fixture changed");

    time.hour = 6;
    jcengine_island_initialize(&replay, &scene, 3, &time,
                               JCENGINE_SKY_AUTOMATIC, 0);
    ESP_RETURN_ON_FALSE(!replay.night && replay.raft_stage == 2,
                        ESP_ERR_INVALID_RESPONSE, TAG,
                        "06:00 daytime or raft-stage fixture changed");
    time.hour = 18;
    jcengine_island_initialize(&replay, &scene, 5, &time,
                               JCENGINE_SKY_AUTOMATIC, 0);
    ESP_RETURN_ON_FALSE(replay.night && replay.raft_stage == 4,
                        ESP_ERR_INVALID_RESPONSE, TAG,
                        "18:00 nighttime or raft-stage fixture changed");
    time.valid = false;
    jcengine_island_initialize(&replay, &scene, 1, &time,
                               JCENGINE_SKY_AUTOMATIC, 0);
    ESP_RETURN_ON_FALSE(!replay.night && replay.ocean_index < 3,
                        ESP_ERR_INVALID_RESPONSE, TAG,
                        "invalid-clock daytime fallback changed");
    jcengine_island_initialize(&replay, &scene, 1, &time,
                               JCENGINE_SKY_NIGHT, 0);
    ESP_RETURN_ON_FALSE(replay.night, ESP_ERR_INVALID_RESPONSE, TAG,
                        "forced-night sky mode changed");

    jcengine_story_scene_t suppressed = scene;
    suppressed.flags = JCENGINE_STORY_ISLAND | JCENGINE_STORY_NORAFT |
                       JCENGINE_STORY_HOLIDAY_NOK |
                       JCENGINE_STORY_LEFT_ISLAND;
    time.valid = true;
    time.hour = 12;
    jcengine_island_initialize(&replay, &suppressed, 11, &time,
                               JCENGINE_SKY_AUTOMATIC, 0);
    ESP_RETURN_ON_FALSE(replay.raft_stage == 0 && replay.holiday_id == 0 &&
                            !replay.low_tide && replay.offset_x == -272 &&
                            replay.offset_y == 0,
                        ESP_ERR_INVALID_RESPONSE, TAG,
                        "island suppression/placement fixture changed");

    const jcengine_story_scene_t *coconut_plane = jcengine_story_scene(59);
    ESP_RETURN_ON_FALSE(
        coconut_plane != NULL &&
            strcmp(coconut_plane->ads_name, "VISITOR.ADS") == 0 &&
            coconut_plane->ads_tag == 5 &&
            (coconut_plane->flags & JCENGINE_STORY_LEFT_ISLAND) != 0 &&
            (coconut_plane->flags & JCENGINE_STORY_VARPOS_OK) == 0,
        ESP_ERR_INVALID_RESPONSE, TAG,
        "coconut-plane catalog placement fixture changed");
    jcengine_island_initialize(&replay, coconut_plane, 1, &time,
                               JCENGINE_SKY_AUTOMATIC, 0);
    select_ttm_origin(&replay, coconut_plane, &ttm_x, &ttm_y);
    ESP_RETURN_ON_FALSE(replay.offset_x == -272 && replay.offset_y == 0 &&
                            ttm_x == 0 && ttm_y == 0,
                        ESP_ERR_INVALID_RESPONSE, TAG,
                        "coconut-plane left-island origin fixture changed");

    replay.active = true;
    replay.low_tide = false;
    replay.wave_slot = 1;
    replay.wave_phase = 1;
    replay.wave_phases[0] = 0;
    replay.wave_phases[1] = 1;
    replay.wave_phases[2] = 0;
    replay.cloud_count = 1;
    replay.cloud_direction = 1;
    replay.clouds[0].x = 10;
    replay.clouds[0].speed = 2;
    ESP_RETURN_ON_FALSE(jcengine_island_tick(&replay, 8) &&
                            replay.wave_slot == 2 &&
                            replay.wave_phases[2] == 1 &&
                            replay.clouds[0].x == 8,
                        ESP_ERR_INVALID_RESPONSE, TAG,
                        "island wave/cloud cadence fixture changed");

    uint8_t story_day = 1;
    int32_t last_date = 0;
    time.date_key = 20260901;
    ESP_RETURN_ON_FALSE(jcengine_story_day_update(&story_day, &last_date, &time) &&
                            story_day == 1 && last_date == 20260901 &&
                            !jcengine_story_day_update(&story_day, &last_date, &time),
                        ESP_ERR_INVALID_RESPONSE, TAG,
                        "story-day initialization fixture changed");
    time.date_key = 20260902;
    ESP_RETURN_ON_FALSE(jcengine_story_day_update(&story_day, &last_date, &time) &&
                            story_day == 2,
                        ESP_ERR_INVALID_RESPONSE, TAG,
                        "story-day increment fixture changed");
    story_day = 11;
    time.date_key = 20260903;
    ESP_RETURN_ON_FALSE(jcengine_story_day_update(&story_day, &last_date, &time) &&
                            story_day == 1,
                        ESP_ERR_INVALID_RESPONSE, TAG,
                        "story-day wrap fixture changed");
    time.valid = false;
    time.date_key = 20260904;
    ESP_RETURN_ON_FALSE(!jcengine_story_day_update(&story_day, &last_date, &time) &&
                            story_day == 1 && last_date == 20260903,
                        ESP_ERR_INVALID_RESPONSE, TAG,
                        "invalid-clock story-day fixture changed");
    return ESP_OK;
}

static esp_err_t collect_ttm_probe(void *context, const jcengine_ttm_command_t *command)
{
    ttm_probe_t *probe = context;
    if (probe->count < sizeof(probe->opcodes) / sizeof(probe->opcodes[0])) {
        probe->opcodes[probe->count] = command->opcode;
    }
    ++probe->count;
    return ESP_OK;
}

static esp_err_t run_ttm_command(void *context, const jcengine_ttm_command_t *command)
{
    ttm_runtime_thread_t *thread = context;
    if (thread == NULL || thread->runtime == NULL || command == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    ttm_runtime_t *runtime = thread->runtime;
    switch (command->opcode) {
    case 0xa601:
        /* DRAW_GETPUT clears only the active TTM clip. Retain authored pixels
           outside it, such as the left half of MJSAND's sandcastle while the
           launch animation refreshes the right side of the layer. */
        clear_ttm_thread_clip(thread);
        return ESP_OK;
    case 0x4004: {
        size_t width = command->args[2] > command->args[0]
                           ? command->args[2] - command->args[0]
                           : 0;
        size_t height = command->args[3] > command->args[1]
                            ? command->args[3] - command->args[1]
                            : 0;
        size_t x = 0;
        size_t y = 0;
        if (map_ttm_region_to_panel(runtime, (int16_t)command->args[0],
                                    (int16_t)command->args[1], width, height,
                                    &x, &y, &width, &height)) {
            thread->clip_x = (uint16_t)x;
            thread->clip_y = (uint16_t)y;
            thread->clip_width = (uint16_t)width;
            thread->clip_height = (uint16_t)height;
        } else {
            thread->clip_width = 0;
            thread->clip_height = 0;
        }
        return ESP_OK;
    }
    case 0x4204: {
        ESP_RETURN_ON_FALSE(runtime->stored_framebuffer != NULL,
                            ESP_ERR_INVALID_STATE, TAG,
                            "TTM stored-area buffer is unavailable");
        size_t width = (size_t)command->args[2] + 2U;
        size_t x = 0;
        size_t y = 0;
        size_t height = 0;
        if (map_ttm_region_to_panel(
                runtime, (int16_t)command->args[0],
                (int16_t)command->args[1], width, command->args[3], &x, &y,
                &width, &height)) {
            clear_stored_region(runtime, x, y, width, height);
            ESP_RETURN_ON_ERROR(
                draw_thread_region(thread, runtime->stored_framebuffer, x, y,
                                   width, height),
                TAG, "store TTM layer area");
        }
        ++runtime->store_area_count;
        update_stored_pixel_peak(runtime);
        return ESP_OK;
    }
    case 0xb606:
        if (command->args[4] == 2 && command->args[5] == 1) {
            ESP_RETURN_ON_FALSE(runtime->stored_framebuffer != NULL,
                                ESP_ERR_INVALID_STATE, TAG,
                                "TTM stored-area buffer is unavailable");
            size_t x = 0;
            size_t y = 0;
            size_t width = 0;
            size_t height = 0;
            if (map_ttm_region_to_panel(
                    runtime, (int16_t)command->args[0],
                    (int16_t)command->args[1], command->args[2],
                    command->args[3], &x, &y, &width, &height)) {
                /* DRAW_SCREEN alpha-composites the composition buffer into
                   stored area. Transparent source pixels preserve saved art;
                   MJSAND relies on this after clearing its temporary layer. */
                ESP_RETURN_ON_ERROR(
                    draw_thread_region(thread, runtime->stored_framebuffer, x,
                                       y, width, height),
                    TAG, "copy TTM layer to stored area");
            }
            ++runtime->draw_screen_count;
            update_stored_pixel_peak(runtime);
        }
        return ESP_OK;
    case 0xa0a4:
    case 0xa104:
    case 0xa404:
    case 0xa504:
    case 0xa524: {
        ttm_layer_draw_t draw = {
            .opcode = command->opcode,
            .args = {command->args[0], command->args[1],
                     command->args[2], command->args[3]},
            .color = thread->engine.fg_color,
            .background_color = thread->engine.bg_color,
            .clip_x = thread->clip_x,
            .clip_y = thread->clip_y,
            .clip_width = thread->clip_width,
            .clip_height = thread->clip_height,
        };
        size_t x = 0;
        size_t y = 0;
        size_t width = 0;
        size_t height = 0;
        bool has_bounds = ttm_draw_bounds(thread, &draw, &x, &y, &width,
                                          &height);
        if (has_bounds &&
            !intersect_regions(x, y, width, height, draw.clip_x, draw.clip_y,
                               draw.clip_width, draw.clip_height, NULL, NULL,
                               NULL, NULL)) {
            return ESP_OK;
        }
        if (thread->draw_count >= TTM_LAYER_DRAW_COMMANDS) {
            ESP_LOGE(TAG,
                     "TTM layer draw limit reached resource=%s tag=%u count=%u",
                     thread->resource == NULL ? "UNKNOWN"
                                              : thread->resource->ttm->name,
                     thread->engine.current_tag,
                     (unsigned)thread->draw_count);
            return ESP_ERR_NO_MEM;
        }
        thread->draws[thread->draw_count++] = draw;
        if (thread->draw_count > runtime->draw_command_peak) {
            runtime->draw_command_peak = thread->draw_count;
        }
        if (command->opcode == 0xa0a4) ++runtime->line_draw_count;
        return ESP_OK;
    }
    case 0xf02f: {
        uint8_t slot = thread->engine.selected_bmp_slot;
        if (slot >= TTM_BITMAP_SLOTS || command->string_arg[0] == '\0') {
            return ESP_ERR_INVALID_ARG;
        }
        ESP_RETURN_ON_FALSE(thread->resource != NULL, ESP_ERR_INVALID_STATE,
                            TAG, "TTM resource is not assigned");
        jcrez_release_bitmap(&thread->resource->bitmap_slots[slot]);
        return jcrez_load_bitmap(
            runtime->archive, command->string_arg,
            &thread->resource->bitmap_slots[slot]);
    }
    case 0xf01f:
        ESP_RETURN_ON_FALSE(command->string_arg[0] != '\0', ESP_ERR_INVALID_ARG,
                            TAG, "TTM SCR name is empty");
        ESP_RETURN_ON_ERROR(load_runtime_screen(runtime, command->string_arg),
                            TAG, "execute TTM LOAD_SCREEN");
        /* The authored fixed screen replaces the dynamically assembled island
           background, while scene placement/cloud/holiday state remains live. */
        runtime->island_dynamic_background = false;
        return ESP_OK;
    default:
        return ESP_OK;
    }
}

static esp_err_t compose_ttm_runtime(ttm_runtime_t *runtime)
{
    ESP_RETURN_ON_FALSE(runtime != NULL && runtime->framebuffer != NULL &&
                            runtime->background_framebuffer != NULL,
                        ESP_ERR_INVALID_ARG, TAG, "invalid TTM compositor");
    memcpy(runtime->framebuffer, runtime->background_framebuffer,
           JCBOARD_WIDTH * JCBOARD_HEIGHT * sizeof(*runtime->framebuffer));
    if (runtime->island.active && runtime->island_assets_loaded) {
        for (uint8_t index = 0; index < runtime->island.cloud_count; ++index) {
            const jcengine_cloud_t *cloud = &runtime->island.clouds[index];
            ESP_RETURN_ON_ERROR(
                draw_runtime_sprite(runtime, runtime->framebuffer,
                                    &runtime->island_bitmap,
                                    (size_t)(15 + cloud->sprite), cloud->x,
                                    cloud->y,
                                    runtime->island.cloud_direction <= 0),
                TAG, "compose island cloud");
        }
    }
    if (runtime->stored_framebuffer != NULL) {
        for (size_t index = 0; index < JCBOARD_WIDTH * JCBOARD_HEIGHT; ++index) {
            if (runtime->stored_framebuffer[index] != TTM_TRANSPARENT_RGB565) {
                runtime->framebuffer[index] = runtime->stored_framebuffer[index];
            }
        }
    }
    for (size_t thread_index = 0; thread_index < TTM_RUNTIME_THREADS; ++thread_index) {
        ttm_runtime_thread_t *thread = &runtime->threads[thread_index];
        if (!thread->allocated) {
            continue;
        }
        for (size_t draw_index = 0; draw_index < thread->draw_count; ++draw_index) {
            const ttm_layer_draw_t *draw = &thread->draws[draw_index];
            if (draw->opcode == 0xa0a4) {
                ESP_RETURN_ON_ERROR(
                    jcgfx_draw_line_clipped(
                        runtime->framebuffer, JCBOARD_WIDTH, JCBOARD_HEIGHT,
                        JCGFX_LAYOUT_RIGHT, runtime->palette, draw->color,
                        (int16_t)draw->args[0] + runtime->ttm_offset_x,
                        (int16_t)draw->args[1] + runtime->ttm_offset_y,
                        (int16_t)draw->args[2] + runtime->ttm_offset_x,
                        (int16_t)draw->args[3] + runtime->ttm_offset_y,
                        draw->clip_x, draw->clip_y, draw->clip_width,
                        draw->clip_height),
                    TAG, "compose TTM layer line");
                continue;
            }
            if (draw->opcode == 0xa104) {
                ESP_RETURN_ON_ERROR(
                    jcgfx_draw_rect_clipped(
                        runtime->framebuffer, JCBOARD_WIDTH, JCBOARD_HEIGHT,
                        JCGFX_LAYOUT_RIGHT, runtime->palette, draw->color,
                        (int16_t)draw->args[0] + runtime->ttm_offset_x,
                        (int16_t)draw->args[1] + runtime->ttm_offset_y,
                        draw->args[2], draw->args[3], draw->clip_x,
                        draw->clip_y, draw->clip_width, draw->clip_height),
                    TAG, "compose TTM layer rectangle");
                continue;
            }
            if (draw->opcode == 0xa404) {
                ESP_RETURN_ON_ERROR(
                    jcgfx_draw_circle_clipped(
                        runtime->framebuffer, JCBOARD_WIDTH, JCBOARD_HEIGHT,
                        JCGFX_LAYOUT_RIGHT, runtime->palette, draw->color,
                        draw->background_color,
                        (int16_t)draw->args[0] + runtime->ttm_offset_x,
                        (int16_t)draw->args[1] + runtime->ttm_offset_y,
                        draw->args[2], draw->args[3], draw->clip_x,
                        draw->clip_y, draw->clip_width, draw->clip_height),
                    TAG, "compose TTM layer circle");
                continue;
            }
            uint16_t slot = draw->args[3];
            if (thread->resource == NULL || slot >= TTM_BITMAP_SLOTS ||
                thread->resource->bitmap_slots[slot].packed_pixels == NULL) {
                if (!thread->missing_bitmap_reported) {
                    ESP_LOGW(
                        TAG,
                        "TTM draw skipped missing bitmap resource=%s slot=%u",
                        thread->resource == NULL
                            ? "UNKNOWN"
                            : thread->resource->ttm->name,
                        slot);
                    thread->missing_bitmap_reported = true;
                }
                continue;
            }
            if (draw->args[2] >=
                thread->resource->bitmap_slots[slot].sprite_count) {
                ESP_LOGW(TAG,
                         "TTM sprite index=%u outside slot=%u count=%u; "
                         "desktop-compatible no-op",
                         draw->args[2], slot,
                         (unsigned)thread->resource
                             ->bitmap_slots[slot].sprite_count);
                continue;
            }
            ESP_RETURN_ON_ERROR(
                jcgfx_draw_bitmap_sprite_clipped(
                    runtime->framebuffer, JCBOARD_WIDTH, JCBOARD_HEIGHT,
                    JCGFX_LAYOUT_RIGHT, runtime->palette,
                    &thread->resource->bitmap_slots[slot], draw->args[2],
                    (int16_t)draw->args[0] + runtime->ttm_offset_x,
                    (int16_t)draw->args[1] + runtime->ttm_offset_y,
                    draw->opcode == 0xa524, draw->clip_x, draw->clip_y,
                    draw->clip_width, draw->clip_height),
                TAG, "compose TTM layer sprite");
        }
    }
    if (runtime->island.active && runtime->island.holiday_id > 0 &&
        runtime->island.holiday_id <= 4 && runtime->island_assets_loaded) {
        static const int16_t HOLIDAY_X[] = {410, 333, 404, 361};
        static const int16_t HOLIDAY_Y[] = {298, 286, 267, 155};
        size_t holiday = runtime->island.holiday_id - 1;
        ESP_RETURN_ON_ERROR(
            draw_runtime_sprite(runtime, runtime->framebuffer,
                                &runtime->holiday_bitmap, holiday,
                                HOLIDAY_X[holiday] + runtime->island.offset_x,
                                HOLIDAY_Y[holiday] + runtime->island.offset_y,
                                false),
            TAG, "compose holiday overlay");
    }
    return ESP_OK;
}

static esp_err_t register_ttm_runtime_resource(ttm_runtime_t *runtime,
                                               uint16_t ads_slot,
                                               const jcrez_ttm_t *ttm)
{
    ESP_RETURN_ON_FALSE(runtime != NULL && ttm != NULL && ttm->bytecode != NULL,
                        ESP_ERR_INVALID_ARG, TAG, "invalid TTM runtime resource");
    for (size_t index = 0; index < runtime->resource_count; ++index) {
        if (runtime->resources[index].ads_slot == ads_slot) {
            runtime->resources[index].ttm = ttm;
            return ESP_OK;
        }
    }
    ESP_RETURN_ON_FALSE(runtime->resource_count < TTM_RUNTIME_RESOURCE_SLOTS,
                        ESP_ERR_NO_MEM, TAG, "TTM runtime resource limit reached");
    runtime->resources[runtime->resource_count++] = (ttm_runtime_resource_t){
        .ads_slot = ads_slot,
        .ttm = ttm,
    };
    return ESP_OK;
}

static ttm_runtime_resource_t *find_ttm_runtime_resource(
    ttm_runtime_t *runtime, uint16_t ads_slot)
{
    if (runtime == NULL) return NULL;
    for (size_t index = 0; index < runtime->resource_count; ++index) {
        if (runtime->resources[index].ads_slot == ads_slot) {
            return &runtime->resources[index];
        }
    }
    return NULL;
}

static esp_err_t add_ttm_runtime_scene(ttm_runtime_t *runtime, uint16_t ads_slot,
                                        uint16_t tag, uint16_t plays,
                                        ttm_runtime_thread_t **created_thread)
{
    ttm_runtime_resource_t *resource =
        find_ttm_runtime_resource(runtime, ads_slot);
    ESP_RETURN_ON_FALSE(runtime != NULL && resource != NULL,
                        ESP_ERR_INVALID_ARG, TAG, "invalid TTM runtime");
    for (size_t index = 0; index < TTM_RUNTIME_THREADS; ++index) {
        ttm_runtime_thread_t *thread = &runtime->threads[index];
        if (thread->allocated && thread->ads_slot == ads_slot &&
            thread->engine.scene_tag == tag &&
            thread->engine.status == JCENGINE_TTM_RUNNING) {
            if (created_thread != NULL) {
                *created_thread = thread;
            }
            return ESP_OK;
        }
    }
    for (size_t index = 0; index < TTM_RUNTIME_THREADS; ++index) {
        ttm_runtime_thread_t *thread = &runtime->threads[index];
        if (thread->allocated) {
            continue;
        }
        memset(thread, 0, sizeof(*thread));
        thread->runtime = runtime;
        thread->resource = resource;
        thread->ads_slot = ads_slot;
        int16_t timing = (int16_t)plays;
        thread->remaining_plays = timing > 0 ? (uint16_t)(timing - 1) : 0;
        thread->lifetime_remaining_cs =
            runtime->honor_scene_lifetimes && timing < 0
                ? (uint16_t)(-(int32_t)timing)
                : 0;
        thread->allocated = true;
        reset_ttm_thread_clip(thread);
        ESP_RETURN_ON_ERROR(jcengine_ttm_start(&thread->engine, resource->ttm,
                                               tag),
                            TAG, "start TTM runtime scene");
        ESP_RETURN_ON_ERROR(jcengine_ttm_advance(&thread->engine, run_ttm_command,
                                                 thread),
                            TAG, "advance TTM runtime scene");
        if (created_thread != NULL) {
            *created_thread = thread;
        }
        return ESP_OK;
    }
    return ESP_ERR_NO_MEM;
}

static esp_err_t apply_ads_actions(ttm_runtime_t *runtime,
                                    jcengine_ads_scheduler_t *scheduler)
{
    ESP_RETURN_ON_FALSE(runtime != NULL && scheduler != NULL,
                        ESP_ERR_INVALID_ARG, TAG, "invalid ADS runtime actions");
    for (size_t action_index = 0; action_index < scheduler->action_count;
         ++action_index) {
        const jcengine_ads_action_t *action = &scheduler->actions[action_index];
        if (action->type == JCENGINE_ADS_ADD_SCENE) {
            ESP_RETURN_ON_ERROR(add_ttm_runtime_scene(runtime, action->slot,
                                                       action->tag, action->plays,
                                                       NULL),
                                TAG, "materialize ADS scene");
        } else {
            for (size_t index = 0; index < TTM_RUNTIME_THREADS; ++index) {
                ttm_runtime_thread_t *thread = &runtime->threads[index];
                if (thread->allocated && thread->ads_slot == action->slot &&
                    thread->engine.scene_tag == action->tag) {
                    memset(thread, 0, sizeof(*thread));
                }
            }
        }
    }
    scheduler->action_count = 0;
    return ESP_OK;
}

static bool ttm_runtime_has_threads(const ttm_runtime_t *runtime)
{
    for (size_t index = 0; index < TTM_RUNTIME_THREADS; ++index) {
        if (runtime->threads[index].allocated) return true;
    }
    return false;
}

static esp_err_t tick_ads_runtime(ttm_runtime_t *runtime,
                                  jcengine_ads_scheduler_t *scheduler,
                                  uint16_t elapsed_centiseconds,
                                  bool compose_changes,
                                  ttm_completion_probe_t *probe,
                                  bool *changed, bool *event_complete)
{
    ESP_RETURN_ON_FALSE(runtime != NULL && scheduler != NULL && changed != NULL &&
                            event_complete != NULL,
                        ESP_ERR_INVALID_ARG, TAG, "invalid ADS runtime tick");
    *changed = false;
    *event_complete = false;

    size_t completed_indexes[TTM_RUNTIME_THREADS] = {0};
    size_t completed_count = 0;
    for (size_t index = 0; index < TTM_RUNTIME_THREADS; ++index) {
        ttm_runtime_thread_t *thread = &runtime->threads[index];
        if (!thread->allocated) continue;
        bool advanced = false;
        bool lifetime_expired = false;
        if (thread->lifetime_remaining_cs > 0) {
            if (elapsed_centiseconds >= thread->lifetime_remaining_cs) {
                thread->lifetime_remaining_cs = 0;
                thread->engine.status = JCENGINE_TTM_FINISHED;
                thread->engine.timer = 0;
                lifetime_expired = true;
                *changed = true;
            } else {
                thread->lifetime_remaining_cs -= elapsed_centiseconds;
            }
        }
        if (!lifetime_expired &&
            (thread->engine.status == JCENGINE_TTM_RUNNING ||
            (thread->engine.status == JCENGINE_TTM_FINISHED &&
             thread->engine.timer > 0))) {
            ESP_RETURN_ON_ERROR(jcengine_ttm_tick(
                                    &thread->engine, elapsed_centiseconds,
                                    run_ttm_command, thread, &advanced),
                                TAG, "tick ADS-owned TTM scene");
            *changed = *changed || advanced;
        }
        if (thread->engine.status == JCENGINE_TTM_FINISHED &&
            thread->engine.timer == 0) {
            completed_indexes[completed_count++] = index;
        }
    }

    for (size_t completed = 0; completed < completed_count; ++completed) {
        ttm_runtime_thread_t *thread =
            &runtime->threads[completed_indexes[completed]];
        if (!thread->allocated ||
            thread->engine.status != JCENGINE_TTM_FINISHED) {
            continue;
        }
        if (thread->lifetime_remaining_cs > 0) {
            /* Negative ADD_SCENE timing keeps replaying the authored TTM until
               its scene lifetime expires; only then may ADS advance. */
            thread->draw_count = 0;
            reset_ttm_thread_clip(thread);
            ESP_RETURN_ON_ERROR(jcengine_ttm_start(
                                    &thread->engine, thread->engine.ttm,
                                    thread->engine.scene_tag),
                                TAG, "restart timed ADS scene");
            ESP_RETURN_ON_ERROR(jcengine_ttm_advance(
                                    &thread->engine, run_ttm_command, thread),
                                TAG, "advance timed ADS scene");
            *changed = true;
            continue;
        }
        if (thread->remaining_plays > 0) {
            --thread->remaining_plays;
            thread->draw_count = 0;
            reset_ttm_thread_clip(thread);
            ESP_RETURN_ON_ERROR(jcengine_ttm_start(
                                    &thread->engine, thread->engine.ttm,
                                    thread->engine.scene_tag),
                                TAG, "restart repeated ADS scene");
            ESP_RETURN_ON_ERROR(jcengine_ttm_advance(
                                    &thread->engine, run_ttm_command, thread),
                                TAG, "advance repeated ADS scene");
            *changed = true;
            continue;
        }

        uint16_t slot = thread->ads_slot;
        uint16_t tag = thread->engine.scene_tag;
        ESP_LOGD(TAG, "ENGINE: ADS-owned TTM completed slot=%u tag=%u",
                 slot, tag);
        if (probe != NULL) {
            if (probe->count < TTM_COMPLETION_PROBE_ENTRIES) {
                probe->slots[probe->count] = slot;
                probe->tags[probe->count] = tag;
            }
            probe->sequence_hash =
                (probe->sequence_hash ^ ((uint64_t)slot << 16) ^ tag) *
                UINT64_C(1099511628211);
            ++probe->count;
        }
        memset(thread, 0, sizeof(*thread));
        *changed = true;
        ESP_RETURN_ON_ERROR(jcengine_ads_scene_finished(scheduler, slot, tag),
                            TAG, "finish ADS-owned TTM scene");
        if (scheduler->action_count > 0) {
            ESP_RETURN_ON_ERROR(apply_ads_actions(runtime, scheduler),
                                TAG, "apply ADS completion actions");
            *changed = true;
        }
    }

    if (*changed && compose_changes) {
        ESP_RETURN_ON_ERROR(compose_ttm_runtime(runtime), TAG,
                            "compose ticked ADS runtime");
    }
    *event_complete = !ttm_runtime_has_threads(runtime);
    if (*event_complete) {
        clear_stored_framebuffer(runtime);
    }
    return ESP_OK;
}

static void release_ttm_runtime(ttm_runtime_t *runtime)
{
    for (size_t resource = 0; resource < runtime->resource_count; ++resource) {
        for (size_t slot = 0; slot < TTM_BITMAP_SLOTS; ++slot) {
            jcrez_release_bitmap(
                &runtime->resources[resource].bitmap_slots[slot]);
        }
    }
    memset(runtime->threads, 0, sizeof(runtime->threads));
    clear_stored_framebuffer(runtime);
    runtime->stored_pixel_peak = 0;
    runtime->store_area_count = 0;
    runtime->draw_screen_count = 0;
    runtime->line_draw_count = 0;
    runtime->draw_command_peak = 0;
    runtime->sandcastle_clip_retained = false;
}

static void release_live_ads_resources(ttm_runtime_t *runtime,
                                       live_ads_resources_t *resources)
{
    if (runtime == NULL || resources == NULL) return;
    release_ttm_runtime(runtime);
    for (size_t index = 0; index < resources->ttm_count; ++index) {
        jcrez_release_ttm(&resources->ttms[index]);
    }
    jcrez_release_ads(&resources->ads);
    memset(runtime->resources, 0, sizeof(runtime->resources));
    runtime->resource_count = 0;
    memset(resources, 0, sizeof(*resources));
}

static esp_err_t preload_live_ttm_dependencies(
    ttm_runtime_t *runtime, uint16_t ads_slot, const char *ttm_name)
{
    ttm_runtime_resource_t *resource =
        find_ttm_runtime_resource(runtime, ads_slot);
    ESP_RETURN_ON_FALSE(resource != NULL && ttm_name != NULL,
                        ESP_ERR_INVALID_ARG, TAG,
                        "invalid live TTM dependency preload");
    if (strcmp(ttm_name, "GJGULIVR.TTM") == 0) {
        return jcrez_load_bitmap(runtime->archive, "LILIPUTS.BMP",
                                 &resource->bitmap_slots[1]);
    }
    if (strcmp(ttm_name, "GJLILIPU.TTM") == 0) {
        ESP_RETURN_ON_ERROR(jcrez_load_bitmap(runtime->archive, "STNDLAY.BMP",
                                              &resource->bitmap_slots[2]),
                            TAG, "preload GJLILIPU standing sprites");
        return jcrez_load_bitmap(runtime->archive, "SLEEP.BMP",
                                 &resource->bitmap_slots[3]);
    }
    if (strcmp(ttm_name, "GJVIS5.TTM") == 0) {
        /* The canonical VISITOR set establishes GJVIS5.BMP in slot 1 from
           GJVIS6 before GJVIS5 tag 8 references its off-left entry frame.
           Resource-owned slots intentionally prevent unrelated TTM clobbering,
           so seed that declared cross-resource dependency explicitly. */
        return jcrez_load_bitmap(runtime->archive, "GJVIS5.BMP",
                                 &resource->bitmap_slots[1]);
    }
    return ESP_OK;
}

static esp_err_t load_live_ads_resources(ttm_runtime_t *runtime,
                                         live_ads_resources_t *resources,
                                         const char *ads_name)
{
    ESP_RETURN_ON_FALSE(runtime != NULL && runtime->archive != NULL &&
                            resources != NULL && ads_name != NULL,
                        ESP_ERR_INVALID_ARG, TAG, "invalid live ADS resources");
    if (resources->ads.bytecode != NULL &&
        strcmp(resources->ads.name, ads_name) == 0) {
        release_ttm_runtime(runtime);
        for (size_t index = 0; index < resources->ttm_count; ++index) {
            ESP_RETURN_ON_ERROR(
                preload_live_ttm_dependencies(
                    runtime, resources->ads.resources[index].slot,
                    resources->ttms[index].name),
                TAG, "restore cached live TTM dependencies");
        }
        return ESP_OK;
    }

    release_live_ads_resources(runtime, resources);
    esp_err_t err = jcrez_load_ads(runtime->archive, ads_name, &resources->ads);
    if (err != ESP_OK) return err;
    if (resources->ads.resource_count > TTM_RUNTIME_RESOURCE_SLOTS) {
        release_live_ads_resources(runtime, resources);
        return ESP_ERR_NO_MEM;
    }
    for (size_t index = 0; index < resources->ads.resource_count; ++index) {
        err = jcrez_load_ttm(runtime->archive,
                             resources->ads.resources[index].name,
                             &resources->ttms[index]);
        if (err != ESP_OK) {
            release_live_ads_resources(runtime, resources);
            return err;
        }
        ++resources->ttm_count;
        err = register_ttm_runtime_resource(
            runtime, resources->ads.resources[index].slot,
            &resources->ttms[index]);
        if (err != ESP_OK) {
            release_live_ads_resources(runtime, resources);
            return err;
        }
        err = preload_live_ttm_dependencies(
            runtime, resources->ads.resources[index].slot,
            resources->ttms[index].name);
        if (err != ESP_OK) {
            release_live_ads_resources(runtime, resources);
            return err;
        }
    }
    ESP_LOGI(TAG, "ENGINE: live ADS %s loaded resources=%u",
             resources->ads.name, (unsigned)resources->ads.resource_count);
    return ESP_OK;
}

static esp_err_t start_live_story_event(
    ttm_runtime_t *runtime, live_ads_resources_t *resources,
    jcengine_ads_scheduler_t *scheduler,
    const jcengine_story_scene_t *scene)
{
    ESP_RETURN_ON_FALSE(scene != NULL, ESP_ERR_INVALID_ARG, TAG,
                        "live story scene is null");
    ESP_RETURN_ON_ERROR(initialize_runtime_island(runtime, scene), TAG,
                        "initialize story island");
    ESP_RETURN_ON_ERROR(load_live_ads_resources(runtime, resources,
                                                 scene->ads_name),
                        TAG, "load live story ADS resources");
    ESP_RETURN_ON_ERROR(jcengine_ads_start(scheduler, &resources->ads,
                                            scene->ads_tag, 0),
                        TAG, "start live story ADS event");
    ESP_RETURN_ON_ERROR(apply_ads_actions(runtime, scheduler), TAG,
                        "apply live story ADS actions");
    return compose_ttm_runtime(runtime);
}

static uint32_t displayed_frame_from_centiseconds(uint32_t centiseconds)
{
    return 1U + (uint32_t)(((uint64_t)centiseconds *
                            PRESENTATION_FRAMES_PER_SECOND) /
                           100U);
}

static uint32_t centiseconds_for_displayed_frame(uint32_t frame)
{
    if (frame <= 1U) return 0;
    return (uint32_t)((((uint64_t)(frame - 1U) * 100U) +
                       PRESENTATION_FRAMES_PER_SECOND - 1U) /
                      PRESENTATION_FRAMES_PER_SECOND);
}

static size_t wrap_live_story_index(size_t index, int delta)
{
    if (delta < 0) {
        return index == 0 ? LIVE_STORY_COUNT - 1 : index - 1;
    }
    return (index + 1) % LIVE_STORY_COUNT;
}

static bool record_live_story_review(live_story_state_t *state,
                                     jcgfx_validation_review_t review,
                                     size_t *next_index)
{
    if (state == NULL || next_index == NULL ||
        review == JCGFX_VALIDATION_REVIEW_UNREVIEWED) {
        return false;
    }
    state->reviews[state->slice_index] = review;
    for (size_t step = 1; step <= LIVE_STORY_COUNT; ++step) {
        size_t candidate = (state->slice_index + step) % LIVE_STORY_COUNT;
        if (state->reviews[candidate] ==
            JCGFX_VALIDATION_REVIEW_UNREVIEWED) {
            state->review_complete = false;
            *next_index = candidate;
            return true;
        }
    }
    state->review_complete = true;
    state->paused = true;
    *next_index = state->slice_index;
    return false;
}

static const char *live_story_review_name(jcgfx_validation_review_t review)
{
    switch (review) {
    case JCGFX_VALIDATION_REVIEW_OK:
        return "OK";
    case JCGFX_VALIDATION_REVIEW_NEEDS_REVIEW:
        return "REVIEW";
    default:
        return "UNREVIEWED";
    }
}

static uint64_t live_story_catalog_fingerprint(void)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    for (size_t index = 0; index < LIVE_STORY_COUNT; ++index) {
        const jcengine_story_scene_t *scene = jcengine_story_scene(index);
        if (scene == NULL) return 0;
        for (const unsigned char *text =
                 (const unsigned char *)scene->ads_name;
             *text != '\0'; ++text) {
            hash = (hash ^ *text) * UINT64_C(1099511628211);
        }
        hash = (hash ^ 0xffU) * UINT64_C(1099511628211);
        hash = (hash ^ (scene->ads_tag & 0xffU)) * UINT64_C(1099511628211);
        hash = (hash ^ (scene->ads_tag >> 8)) * UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t live_story_valid_mask(void)
{
    return (UINT64_C(1) << LIVE_STORY_COUNT) - 1U;
}

static void live_story_review_masks(const live_story_state_t *state,
                                    uint64_t *ok_mask,
                                    uint64_t *review_mask)
{
    *ok_mask = 0;
    *review_mask = 0;
    for (size_t index = 0; index < LIVE_STORY_COUNT; ++index) {
        if (state->reviews[index] == JCGFX_VALIDATION_REVIEW_OK) {
            *ok_mask |= UINT64_C(1) << index;
        } else if (state->reviews[index] ==
                   JCGFX_VALIDATION_REVIEW_NEEDS_REVIEW) {
            *review_mask |= UINT64_C(1) << index;
        }
    }
}

static void apply_live_story_review_masks(live_story_state_t *state,
                                          uint64_t ok_mask,
                                          uint64_t review_mask)
{
    for (size_t index = 0; index < LIVE_STORY_COUNT; ++index) {
        uint64_t bit = UINT64_C(1) << index;
        state->reviews[index] = (ok_mask & bit) != 0
                                    ? JCGFX_VALIDATION_REVIEW_OK
                                : (review_mask & bit) != 0
                                    ? JCGFX_VALIDATION_REVIEW_NEEDS_REVIEW
                                    : JCGFX_VALIDATION_REVIEW_UNREVIEWED;
    }
    state->review_complete =
        (ok_mask | review_mask) == live_story_valid_mask();
    state->paused = state->review_complete;
}

static esp_err_t store_live_story_reviews(const live_story_state_t *state)
{
    uint64_t ok_mask = 0;
    uint64_t review_mask = 0;
    live_story_review_masks(state, &ok_mask, &review_mask);
    nvs_handle_t handle = 0;
    ESP_RETURN_ON_ERROR(nvs_open("jc_review", NVS_READWRITE, &handle), TAG,
                        "open review NVS");
    esp_err_t err = nvs_set_u8(handle, "schema", REVIEW_SCHEMA_VERSION);
    if (err == ESP_OK) {
        err = nvs_set_u64(handle, "catalog",
                          live_story_catalog_fingerprint());
    }
    if (err == ESP_OK) err = nvs_set_u64(handle, "ok", ok_mask);
    if (err == ESP_OK) err = nvs_set_u64(handle, "review", review_mask);
    if (err == ESP_OK) {
        err = nvs_set_blob(handle, "errors", state->failure_errors,
                           sizeof(state->failure_errors));
    }
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}

static esp_err_t load_live_story_reviews(live_story_state_t *state)
{
    ESP_RETURN_ON_FALSE(state != NULL, ESP_ERR_INVALID_ARG, TAG,
                        "review state is null");
    ESP_RETURN_ON_ERROR(nvs_flash_init(), TAG, "initialize NVS");
    nvs_handle_t handle = 0;
    ESP_RETURN_ON_ERROR(nvs_open("jc_review", NVS_READWRITE, &handle), TAG,
                        "open review NVS");
    uint8_t schema = 0;
    uint64_t catalog = 0;
    uint64_t ok_mask = 0;
    uint64_t review_mask = 0;
    size_t errors_size = sizeof(state->failure_errors);
    esp_err_t schema_err = nvs_get_u8(handle, "schema", &schema);
    esp_err_t catalog_err = nvs_get_u64(handle, "catalog", &catalog);
    esp_err_t ok_err = nvs_get_u64(handle, "ok", &ok_mask);
    esp_err_t review_err = nvs_get_u64(handle, "review", &review_mask);
    esp_err_t errors_err = nvs_get_blob(handle, "errors",
                                        state->failure_errors, &errors_size);
    nvs_close(handle);

    uint64_t valid_mask = live_story_valid_mask();
    bool valid = schema_err == ESP_OK && catalog_err == ESP_OK &&
                 ok_err == ESP_OK && review_err == ESP_OK &&
                 errors_err == ESP_OK &&
                 errors_size == sizeof(state->failure_errors) &&
                 schema == REVIEW_SCHEMA_VERSION &&
                 catalog == live_story_catalog_fingerprint() &&
                 (ok_mask & review_mask) == 0 &&
                 ((ok_mask | review_mask) & ~valid_mask) == 0;
    if (!valid) {
        ESP_LOGW(TAG,
                 "REVIEW: no compatible saved review; starting clean schema=%u catalog=%016" PRIx64,
                 (unsigned)REVIEW_SCHEMA_VERSION,
                 live_story_catalog_fingerprint());
        memset(state->reviews, 0, sizeof(state->reviews));
        memset(state->failure_errors, 0, sizeof(state->failure_errors));
        state->review_complete = false;
        state->paused = false;
        return store_live_story_reviews(state);
    }
    apply_live_story_review_masks(state, ok_mask, review_mask);
    ESP_LOGI(TAG,
             "REVIEW: restored persistent decisions ok=%u review=%u complete=%u catalog=%016" PRIx64,
             (unsigned)__builtin_popcountll(ok_mask),
             (unsigned)__builtin_popcountll(review_mask),
             state->review_complete ? 1U : 0U, catalog);
    return ESP_OK;
}

static jcengine_local_time_t current_local_time(void)
{
    jcengine_local_time_t result = {0};
    time_t now = time(NULL);
    struct tm local = {0};
    if (now <= 0 || localtime_r(&now, &local) == NULL ||
        local.tm_year + 1900 < 2024) {
        return result;
    }
    result.valid = true;
    result.hour = (uint8_t)local.tm_hour;
    result.month = (uint8_t)(local.tm_mon + 1);
    result.day = (uint8_t)local.tm_mday;
    result.date_key = (local.tm_year + 1900) * 10000 +
                      (local.tm_mon + 1) * 100 + local.tm_mday;
    return result;
}

static esp_err_t load_story_progress(ttm_runtime_t *runtime)
{
    ESP_RETURN_ON_FALSE(runtime != NULL, ESP_ERR_INVALID_ARG, TAG,
                        "story runtime is null");
    uint8_t schema = 0;
    uint8_t day = 1;
    int32_t date_key = 0;
    nvs_handle_t handle = 0;
    ESP_RETURN_ON_ERROR(nvs_open("jc_story", NVS_READWRITE, &handle), TAG,
                        "open story NVS");
    esp_err_t schema_err = nvs_get_u8(handle, "schema", &schema);
    bool reset = schema_err != ESP_OK || schema != STORY_SCHEMA_VERSION;
    if (!reset) {
        if (nvs_get_u8(handle, "day", &day) != ESP_OK || day < 1 || day > 11) {
            day = 1;
        }
        if (nvs_get_i32(handle, "date", &date_key) != ESP_OK) date_key = 0;
    } else {
        schema = STORY_SCHEMA_VERSION;
        day = 1;
        date_key = 0;
    }

    runtime->local_time = current_local_time();
    bool changed = reset;
    changed = jcengine_story_day_update(&day, &date_key,
                                        &runtime->local_time) || changed;
    if (changed) {
        esp_err_t err = nvs_set_u8(handle, "schema", STORY_SCHEMA_VERSION);
        if (err == ESP_OK) err = nvs_set_u8(handle, "day", day);
        if (err == ESP_OK) err = nvs_set_i32(handle, "date", date_key);
        if (err == ESP_OK) err = nvs_commit(handle);
        if (err != ESP_OK) {
            nvs_close(handle);
            return err;
        }
    }
    nvs_close(handle);
    runtime->story_day = day;
    runtime->story_date_key = date_key;
    runtime->sky_mode = JCENGINE_SKY_AUTOMATIC;
    runtime->holiday_mode = JCCONTROL_HOLIDAY_AUTOMATIC;
    ESP_LOGI(TAG, "STORY: day=%u date=%" PRId32 " clock_valid=%u hour=%u",
             day, date_key, runtime->local_time.valid ? 1U : 0U,
             runtime->local_time.hour);
    return ESP_OK;
}

static bool find_unreviewed_scene(const live_story_state_t *state,
                                  size_t start, size_t *index)
{
    if (state == NULL || index == NULL) return false;
    for (size_t step = 0; step < LIVE_STORY_COUNT; ++step) {
        size_t candidate = (start + step) % LIVE_STORY_COUNT;
        if (state->reviews[candidate] ==
            JCGFX_VALIDATION_REVIEW_UNREVIEWED) {
            *index = candidate;
            return true;
        }
    }
    return false;
}

static uint64_t live_story_review_mask(const live_story_state_t *state)
{
    uint64_t ok_mask = 0;
    uint64_t review_mask = 0;
    live_story_review_masks(state, &ok_mask, &review_mask);
    return review_mask;
}

static bool find_review_scene(const live_story_state_t *state, size_t start,
                              int direction, bool include_start,
                              size_t *index)
{
    if (state == NULL || index == NULL || (direction != -1 && direction != 1)) {
        return false;
    }
    for (size_t step = include_start ? 0 : 1; step < LIVE_STORY_COUNT + 1;
         ++step) {
        size_t offset = step % LIVE_STORY_COUNT;
        size_t candidate = direction > 0
                               ? (start + offset) % LIVE_STORY_COUNT
                               : (start + LIVE_STORY_COUNT - offset) %
                                     LIVE_STORY_COUNT;
        if (state->reviews[candidate] ==
            JCGFX_VALIDATION_REVIEW_NEEDS_REVIEW) {
            *index = candidate;
            return true;
        }
    }
    return false;
}

static bool find_first_review_scene_named(const live_story_state_t *state,
                                          const char *ads_name,
                                          size_t *index)
{
    if (state == NULL || ads_name == NULL || index == NULL) return false;
    for (size_t candidate = 0; candidate < LIVE_STORY_COUNT; ++candidate) {
        const jcengine_story_scene_t *scene = jcengine_story_scene(candidate);
        if (state->reviews[candidate] ==
                JCGFX_VALIDATION_REVIEW_NEEDS_REVIEW &&
            scene != NULL && strcmp(scene->ads_name, ads_name) == 0) {
            *index = candidate;
            return true;
        }
    }
    return false;
}

#if CONFIG_JOHNNY_REVIEW_ONLY
static bool is_stand_review_loop_scene(const jcengine_story_scene_t *scene)
{
    return scene != NULL && strcmp(scene->ads_name, "STAND.ADS") == 0 &&
           scene->ads_tag >= 1 && scene->ads_tag <= 12;
}

static bool find_next_stand_review_loop_scene(
    const live_story_state_t *state, const jcengine_story_scene_t *current,
    size_t *index)
{
    if (state == NULL || !is_stand_review_loop_scene(current) ||
        index == NULL) {
        return false;
    }
    for (uint16_t step = 1; step <= 12; ++step) {
        uint16_t wanted_tag = (uint16_t)(((current->ads_tag - 1 + step) % 12) + 1);
        for (size_t candidate = 0; candidate < LIVE_STORY_COUNT; ++candidate) {
            const jcengine_story_scene_t *scene =
                jcengine_story_scene(candidate);
            if (state->reviews[candidate] ==
                    JCGFX_VALIDATION_REVIEW_NEEDS_REVIEW &&
                is_stand_review_loop_scene(scene) &&
                scene->ads_tag == wanted_tag) {
                *index = candidate;
                return true;
            }
        }
    }
    return false;
}
#endif

static bool review_shortlist_position(const live_story_state_t *state,
                                      size_t index, size_t *position,
                                      size_t *count)
{
    if (state == NULL || index >= LIVE_STORY_COUNT || position == NULL ||
        count == NULL) {
        return false;
    }
    *position = 0;
    *count = 0;
    bool found = false;
    for (size_t candidate = 0; candidate < LIVE_STORY_COUNT; ++candidate) {
        if (state->reviews[candidate] !=
            JCGFX_VALIDATION_REVIEW_NEEDS_REVIEW) {
            continue;
        }
        if (candidate == index) {
            *position = *count;
            found = true;
        }
        ++*count;
    }
    return found;
}

static esp_err_t persist_live_story_review(
    live_story_state_t *state, size_t index,
    jcgfx_validation_review_t review, esp_err_t failure_error)
{
    ESP_RETURN_ON_FALSE(state != NULL && index < LIVE_STORY_COUNT &&
                            review != JCGFX_VALIDATION_REVIEW_UNREVIEWED,
                        ESP_ERR_INVALID_ARG, TAG,
                        "invalid persistent review decision");
    jcgfx_validation_review_t previous = state->reviews[index];
    int32_t previous_error = state->failure_errors[index];
    state->reviews[index] = review;
    state->failure_errors[index] = failure_error;
    uint64_t ok_mask = 0;
    uint64_t review_mask = 0;
    live_story_review_masks(state, &ok_mask, &review_mask);
    state->review_complete =
        (ok_mask | review_mask) == live_story_valid_mask();
    state->paused = state->review_complete;
    esp_err_t err = store_live_story_reviews(state);
    if (err != ESP_OK) {
        state->reviews[index] = previous;
        state->failure_errors[index] = previous_error;
        live_story_review_masks(state, &ok_mask, &review_mask);
        state->review_complete =
            (ok_mask | review_mask) == live_story_valid_mask();
    }
    return err;
}

static const live_story_fixture_t *find_validated_story_fixture(
    const jcengine_story_scene_t *scene)
{
    if (scene == NULL) return NULL;
    for (size_t index = 0; index < VALIDATED_STORY_FIXTURE_COUNT; ++index) {
        const live_story_fixture_t *fixture =
            &VALIDATED_STORY_FIXTURES[index];
        if (strcmp(scene->ads_name, fixture->ads_name) == 0 &&
            scene->ads_tag == fixture->ads_tag) {
            return fixture;
        }
    }
    return NULL;
}

static void format_live_story_title(const jcengine_story_scene_t *scene,
                                    char *title, size_t title_size)
{
    if (scene == NULL || title == NULL || title_size == 0) return;
    const char *friendly_title =
        jcengine_scene_title(scene->ads_name, scene->ads_tag);
    if (friendly_title != NULL) {
        snprintf(title, title_size, "%s", friendly_title);
        return;
    }
    const char *extension = strstr(scene->ads_name, ".ADS");
    int base_length = extension == NULL
                          ? (int)strlen(scene->ads_name)
                          : (int)(extension - scene->ads_name);
    snprintf(title, title_size, "%.*s EVENT %u", base_length,
             scene->ads_name, scene->ads_tag);
}

static const jcengine_story_scene_t *find_story_scene_by_identity(
    const char *ads_name, uint16_t ads_tag)
{
    for (size_t index = 0; index < LIVE_STORY_COUNT; ++index) {
        const jcengine_story_scene_t *scene = jcengine_story_scene(index);
        if (scene != NULL && strcmp(scene->ads_name, ads_name) == 0 &&
            scene->ads_tag == ads_tag) {
            return scene;
        }
    }
    return NULL;
}

static void log_live_story_review_summary(const live_story_state_t *state)
{
    size_t ok_count = 0;
    size_t review_count = 0;
    size_t unreviewed_count = 0;
    ESP_LOGI(TAG, "REVIEW: summary begin");
    for (size_t index = 0; index < LIVE_STORY_COUNT; ++index) {
        const jcengine_story_scene_t *scene = jcengine_story_scene(index);
        char title[32] = {0};
        format_live_story_title(scene, title, sizeof(title));
        if (state->reviews[index] == JCGFX_VALIDATION_REVIEW_OK) {
            ++ok_count;
        } else if (state->reviews[index] ==
                   JCGFX_VALIDATION_REVIEW_NEEDS_REVIEW) {
            ++review_count;
        } else {
            ++unreviewed_count;
        }
        ESP_LOGI(TAG,
                 "REVIEW: event=%u/%u title=%s %s tag=%u result=%s",
                 (unsigned)(index + 1), (unsigned)LIVE_STORY_COUNT,
                 title, scene->ads_name, scene->ads_tag,
                 live_story_review_name(state->reviews[index]));
    }
    ESP_LOGI(TAG, "REVIEW: summary end");
    printf("REVIEW_EXPORT_BEGIN schema=%u catalog=%016" PRIx64
           " ok=%u review=%u unreviewed=%u\n",
           (unsigned)REVIEW_SCHEMA_VERSION, live_story_catalog_fingerprint(),
           (unsigned)ok_count, (unsigned)review_count,
           (unsigned)unreviewed_count);
    printf("| Scene | Title | ADS | Tag | Result | Failure |\n");
    printf("| ---: | --- | --- | ---: | --- | --- |\n");
    for (size_t index = 0; index < LIVE_STORY_COUNT; ++index) {
        const jcengine_story_scene_t *scene = jcengine_story_scene(index);
        char title[32] = {0};
        format_live_story_title(scene, title, sizeof(title));
        const char *failure = state->failure_errors[index] == ESP_OK
                                  ? ""
                                  : esp_err_to_name(
                                        state->failure_errors[index]);
        printf("| %02u | %s | %s | %u | %s | %s |\n",
               (unsigned)(index + 1), title, scene->ads_name,
               (unsigned)scene->ads_tag,
               live_story_review_name(state->reviews[index]), failure);
    }
    printf("REVIEW_EXPORT_END\n");
    fflush(stdout);
}

static void log_review_shortlist(const live_story_state_t *state)
{
    uint64_t review_mask = live_story_review_mask(state);
    size_t review_count = (size_t)__builtin_popcountll(review_mask);
    printf("REVIEW_SHORTLIST_EXPORT_BEGIN schema=%u catalog=%016" PRIx64
           " review=%u\n",
           (unsigned)REVIEW_SCHEMA_VERSION, live_story_catalog_fingerprint(),
           (unsigned)review_count);
    printf("| Position | Scene | Title | ADS | Tag | Failure |\n");
    printf("| ---: | ---: | --- | --- | ---: | --- |\n");
    size_t position = 0;
    for (size_t index = 0; index < LIVE_STORY_COUNT; ++index) {
        if ((review_mask & (UINT64_C(1) << index)) == 0) continue;
        const jcengine_story_scene_t *scene = jcengine_story_scene(index);
        char title[32] = {0};
        format_live_story_title(scene, title, sizeof(title));
        const char *failure = state->failure_errors[index] == ESP_OK
                                  ? ""
                                  : esp_err_to_name(
                                        state->failure_errors[index]);
        printf("| %02u | %02u | %s | %s | %u | %s |\n",
               (unsigned)(++position), (unsigned)(index + 1), title,
               scene->ads_name, (unsigned)scene->ads_tag, failure);
    }
    printf("REVIEW_SHORTLIST_EXPORT_END\n");
    fflush(stdout);
}

#if CONFIG_JOHNNY_REVIEW_ONLY
static bool finish_review_only_decision(live_story_state_t *state,
                                        size_t reviewed_index,
                                        size_t *next_index)
{
    state->review_pass_pending &= ~(UINT64_C(1) << reviewed_index);
    uint64_t review_mask = live_story_review_mask(state);
    if (review_mask == 0) {
        state->all_resolved = true;
        state->review_complete = true;
        state->paused = true;
        state->review_pass_pending = 0;
        log_review_shortlist(state);
        return false;
    }
    state->all_resolved = false;
    state->review_complete = false;
    state->paused = false;
    if (state->review_pass_pending == 0) {
        log_review_shortlist(state);
        state->review_pass_pending = review_mask;
    }
    return find_review_scene(state, reviewed_index, 1, false, next_index);
}
#endif

static esp_err_t resolve_live_story_scene(
    size_t slice_index, const jcengine_story_scene_t **scene)
{
    ESP_RETURN_ON_FALSE(slice_index < LIVE_STORY_COUNT && scene != NULL,
                        ESP_ERR_INVALID_ARG, TAG,
                        "invalid live story slice index");
    const jcengine_story_scene_t *candidate =
        jcengine_story_scene(slice_index);
    ESP_RETURN_ON_FALSE(candidate != NULL, ESP_ERR_NOT_FOUND, TAG,
                        "live story catalog entry is missing");
    *scene = candidate;
    return ESP_OK;
}

static esp_err_t begin_live_story_event(
    ttm_runtime_t *runtime, live_ads_resources_t *resources,
    jcengine_ads_scheduler_t *scheduler, live_story_state_t *state,
    size_t slice_index)
{
    ESP_RETURN_ON_FALSE(state != NULL && slice_index < LIVE_STORY_COUNT,
                        ESP_ERR_INVALID_ARG, TAG,
                        "invalid live story start state");
    const jcengine_story_scene_t *scene = NULL;
    ESP_RETURN_ON_ERROR(resolve_live_story_scene(slice_index, &scene), TAG,
                        "select live story event");
    ESP_RETURN_ON_ERROR(start_live_story_event(runtime, resources, scheduler,
                                               scene),
                        TAG, "start selected live story event");
    state->slice_index = slice_index;
    state->scene = scene;
    format_live_story_title(scene, state->title, sizeof(state->title));
    state->paused = false;
    state->review_complete = false;
    state->elapsed_centiseconds = 0;
    state->displayed_frame = 1;
    state->animation_clock_us = esp_timer_get_time();
    state->animation_remainder_us = 0;
    state->started_us = state->animation_clock_us;
    memset(&state->probe, 0, sizeof(state->probe));
    ESP_LOGI(TAG,
             "ENGINE: story slice event=%u/%u %s tag=%u frame=1 live playback started",
             (unsigned)(slice_index + 1), (unsigned)LIVE_STORY_COUNT,
             scene->ads_name, scene->ads_tag);
    ESP_LOGI(TAG, "SCENE: %s (%s tag %u)", state->title, scene->ads_name,
             (unsigned)scene->ads_tag);
    return ESP_OK;
}

static esp_err_t begin_review_scene_or_continue(
    ttm_runtime_t *runtime, live_ads_resources_t *resources,
    jcengine_ads_scheduler_t *scheduler, live_story_state_t *state,
    size_t requested_index)
{
    size_t index = requested_index;
    for (size_t attempt = 0; attempt < LIVE_STORY_COUNT; ++attempt) {
        esp_err_t err = begin_live_story_event(runtime, resources, scheduler,
                                               state, index);
        if (err == ESP_OK) return ESP_OK;
        const jcengine_story_scene_t *scene = jcengine_story_scene(index);
        ESP_LOGE(TAG,
                 "REVIEW: scene=%u/%u %s tag=%u start failed error=%s; saved as REVIEW",
                 (unsigned)(index + 1), (unsigned)LIVE_STORY_COUNT,
                 scene == NULL ? "UNKNOWN" : scene->ads_name,
                 scene == NULL ? 0U : (unsigned)scene->ads_tag,
                 esp_err_to_name(err));
        ESP_RETURN_ON_ERROR(
            persist_live_story_review(
                state, index, JCGFX_VALIDATION_REVIEW_NEEDS_REVIEW, err),
            TAG, "persist automatic REVIEW result");
#if CONFIG_JOHNNY_REVIEW_ONLY
        if (!finish_review_only_decision(state, index, &index)) {
            ESP_LOGI(TAG, "REVIEW-ONLY: ALL RESOLVED");
            return ESP_OK;
        }
#else
        if (!find_unreviewed_scene(state, index + 1, &index)) {
            state->review_complete = true;
            state->paused = true;
            log_live_story_review_summary(state);
            return ESP_OK;
        }
#endif
    }
    return ESP_ERR_NOT_FOUND;
}

#if !CONFIG_JOHNNY_REVIEW_ONLY
static esp_err_t begin_random_scene_or_continue(
    ttm_runtime_t *runtime, live_ads_resources_t *resources,
    jcengine_ads_scheduler_t *scheduler, live_story_state_t *state)
{
    for (size_t attempt = 0; attempt < LIVE_STORY_COUNT; ++attempt) {
        size_t index = jccontrol_shuffle_next(&state->shuffle, esp_random());
        esp_err_t err = begin_live_story_event(runtime, resources, scheduler,
                                               state, index);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "PLAYBACK: random scene=%u remaining=%u",
                     (unsigned)(index + 1),
                     (unsigned)jccontrol_shuffle_remaining(&state->shuffle));
            return ESP_OK;
        }
        ESP_LOGE(TAG,
                 "PLAYBACK: scene=%u start failed error=%s; skipping",
                 (unsigned)(index + 1), esp_err_to_name(err));
    }
    return ESP_ERR_NOT_FOUND;
}
#endif

static void draw_live_story_sidebar(uint16_t *framebuffer,
                                    const live_story_state_t *state)
{
#if CONFIG_JOHNNY_REVIEW_ONLY
    if (state->all_resolved) {
        jcgfx_draw_validation_all_resolved(framebuffer, JCBOARD_WIDTH,
                                           JCBOARD_HEIGHT);
        return;
    }
    size_t position = 0;
    size_t count = 0;
    if (!review_shortlist_position(state, state->slice_index, &position,
                                   &count)) {
        jcgfx_draw_validation_all_resolved(framebuffer, JCBOARD_WIDTH,
                                           JCBOARD_HEIGHT);
        return;
    }
#else
    jcnet_status_t network = {0};
    jcnet_status(&network);
    if (!network.provisioned && network.setup_password[0] != '\0') {
        jcgfx_draw_setup_sidebar(framebuffer, JCBOARD_WIDTH, JCBOARD_HEIGHT,
                                 network.ap_ssid,
                                 network.setup_password);
        return;
    }
    if (state->sidebar_mode == JCCONTROL_SIDEBAR_OFF) {
        jcgfx_clear_sidebar(framebuffer, JCBOARD_WIDTH, JCBOARD_HEIGHT);
        return;
    }
    if (state->sidebar_mode == JCCONTROL_SIDEBAR_CLOCK) {
        jcnet_weather_status_t weather = {0};
        jcnet_weather_status(&weather);
        time_t now = time(NULL);
        time_t local_epoch = now + weather.utc_offset_seconds;
        struct tm local = {0};
        if (weather.available) {
            gmtime_r(&local_epoch, &local);
        } else {
            localtime_r(&now, &local);
        }
        struct tm weather_updated = {0};
        bool weather_updated_valid = false;
        if (weather.updated_at > 0) {
            time_t weather_updated_epoch =
                (time_t)weather.updated_at + weather.utc_offset_seconds;
            weather_updated_valid =
                gmtime_r(&weather_updated_epoch, &weather_updated) != NULL;
        }
        jcgfx_clock_weather_status_t clock = {
            .time_valid = network.time_synced && now > 1704067200,
            .weather_available = weather.available,
            .weather_stale = weather.stale,
            .weather_updated_valid = weather_updated_valid,
            .year = (uint16_t)(local.tm_year + 1900),
            .month = (uint8_t)(local.tm_mon + 1),
            .day = (uint8_t)local.tm_mday,
            .weekday = (uint8_t)local.tm_wday,
            .hour = (uint8_t)local.tm_hour,
            .minute = (uint8_t)local.tm_min,
            .weather_updated_hour = (uint8_t)weather_updated.tm_hour,
            .weather_updated_minute = (uint8_t)weather_updated.tm_min,
            .temperature_tenths = weather.temperature_tenths,
            .high_tenths = weather.high_tenths,
            .low_tenths = weather.low_tenths,
            .weather_code = weather.weather_code,
            .location = weather.configured ? weather.location : "SET LOCATION",
        };
        jcgfx_draw_clock_weather_sidebar(framebuffer, JCBOARD_WIDTH,
                                         JCBOARD_HEIGHT, &clock);
        return;
    }
    if (state->review_complete) {
        jcgfx_draw_validation_review_summary(
            framebuffer, JCBOARD_WIDTH, JCBOARD_HEIGHT, state->reviews,
            LIVE_STORY_COUNT);
        return;
    }
#endif
    const jcgfx_validation_sidebar_status_t status = {
#if CONFIG_JOHNNY_REVIEW_ONLY
        .scene_number = (uint8_t)(position + 1),
        .scene_count = (uint8_t)count,
#else
        .scene_number = (uint8_t)(state->slice_index + 1),
        .scene_count = LIVE_STORY_COUNT,
#endif
        .frame_number = state->displayed_frame,
        .paused = state->paused,
        .title = state->title,
        .ads_name = state->scene->ads_name,
        .ads_tag = state->scene->ads_tag,
        .review = state->reviews[state->slice_index],
    };
    jcgfx_draw_validation_sidebar(framebuffer, JCBOARD_WIDTH, JCBOARD_HEIGHT,
                                   &status);
}

#if !CONFIG_JOHNNY_REVIEW_ONLY
static void publish_live_status(const ttm_runtime_t *runtime,
                                const live_story_state_t *state)
{
    jccontrol_snapshot_t snapshot = {
        .scene_index = state->slice_index,
        .frame = state->displayed_frame,
        .shuffle_remaining = jccontrol_shuffle_remaining(&state->shuffle),
        .paused = state->paused,
        .settings = {
            .sky = runtime->sky_mode,
            .holiday = runtime->holiday_mode,
            .playback_mode = state->playback_mode,
            .sidebar_mode = state->sidebar_mode,
        },
        .effective_holiday = runtime->island.holiday_id,
        .effective_night = runtime_night(runtime),
        .cycle_position = runtime->cycle_position,
        .cycle_block = runtime->cycle_block,
        .story_day = runtime->story_day,
        .island_x = runtime->island.offset_x,
        .island_y = runtime->island.offset_y,
        .low_tide = runtime->island.low_tide,
        .raft_stage = runtime->island.raft_stage,
        .catalog_fingerprint = live_story_catalog_fingerprint(),
    };
    const esp_app_desc_t *app = esp_app_get_description();
    if (app != NULL) {
        snprintf(snapshot.firmware_version, sizeof(snapshot.firmware_version),
                 "%s", app->version);
    }
    jccontrol_publish(&snapshot);
}

static bool apply_live_control_commands(
    ttm_runtime_t *runtime, live_ads_resources_t *resources,
    jcengine_ads_scheduler_t *scheduler, live_story_state_t *state)
{
    bool changed = false;
    jccontrol_command_t command = {0};
    while (jccontrol_take(&command)) {
        esp_err_t err = ESP_OK;
        if (command.type == JCCONTROL_COMMAND_SETTINGS) {
            jccontrol_settings_t settings = {
                .sky = runtime->sky_mode,
                .holiday = runtime->holiday_mode,
                .playback_mode = state->playback_mode,
                .sidebar_mode = state->sidebar_mode,
            };
            if (command.has_sky) settings.sky = command.settings.sky;
            if (command.has_holiday) {
                settings.holiday = command.settings.holiday;
            }
            if (command.has_playback_mode) {
                settings.playback_mode = command.settings.playback_mode;
            }
            if (command.has_sidebar_mode) {
                settings.sidebar_mode = command.settings.sidebar_mode;
            }
            if (!jccontrol_settings_valid(&settings)) {
                err = ESP_ERR_INVALID_ARG;
            } else {
                jccontrol_playback_mode_t previous_mode = state->playback_mode;
                jcengine_sky_mode_t previous_sky = runtime->sky_mode;
                runtime->sky_mode = settings.sky;
                runtime->holiday_mode = settings.holiday;
                state->playback_mode = settings.playback_mode;
                state->sidebar_mode = settings.sidebar_mode;
                if (runtime->sky_mode == JCENGINE_SKY_CYCLE &&
                    previous_sky != JCENGINE_SKY_CYCLE) {
                    runtime->cycle_position = 0;
                    runtime->cycle_block = 0;
                    runtime->block_anchor_refresh = true;
                }
                runtime->local_time = current_local_time();
                err = refresh_runtime_theme(runtime, state->scene);
                if (err == ESP_OK && previous_mode != state->playback_mode) {
                    if (state->playback_mode == JCCONTROL_PLAYBACK_REVIEW) {
                        err = begin_live_story_event(runtime, resources,
                                                     scheduler, state, 0);
                    } else {
                        err = begin_random_scene_or_continue(
                            runtime, resources, scheduler, state);
                    }
                }
                if (err == ESP_OK) err = jccontrol_store_settings(&settings);
                if (err == ESP_OK) changed = true;
            }
        } else if (command.type == JCCONTROL_COMMAND_SCENE) {
            err = begin_live_story_event(runtime, resources, scheduler, state,
                                         command.scene_index);
            if (err == ESP_OK) changed = true;
        } else if (command.type == JCCONTROL_COMMAND_RANDOM) {
            state->playback_mode = JCCONTROL_PLAYBACK_NORMAL;
            jccontrol_settings_t settings = {
                .sky = runtime->sky_mode,
                .holiday = runtime->holiday_mode,
                .playback_mode = state->playback_mode,
                .sidebar_mode = state->sidebar_mode,
            };
            err = begin_random_scene_or_continue(runtime, resources, scheduler,
                                                  state);
            if (err == ESP_OK) err = jccontrol_store_settings(&settings);
            if (err == ESP_OK) changed = true;
        } else if (command.type == JCCONTROL_COMMAND_REVIEW_OK ||
                   command.type == JCCONTROL_COMMAND_REVIEW_BUG) {
            if (state->playback_mode != JCCONTROL_PLAYBACK_REVIEW) {
                err = ESP_ERR_INVALID_STATE;
            } else {
                if (command.type == JCCONTROL_COMMAND_REVIEW_BUG) {
                    publish_live_status(runtime, state);
                    jccontrol_snapshot_t snapshot = {0};
                    jccontrol_snapshot(&snapshot);
                    err = jccontrol_bug_capture(&snapshot);
                }
                if (err == ESP_OK) {
                    advance_story_block(runtime);
                    err = begin_live_story_event(
                        runtime, resources, scheduler, state,
                        (state->slice_index + 1) % LIVE_STORY_COUNT);
                }
                if (err == ESP_OK) changed = true;
            }
        } else if (command.type == JCCONTROL_COMMAND_REVIEW_PREVIOUS ||
                   command.type == JCCONTROL_COMMAND_REVIEW_NEXT) {
            if (state->playback_mode != JCCONTROL_PLAYBACK_REVIEW) {
                err = ESP_ERR_INVALID_STATE;
            } else {
                int direction = command.type == JCCONTROL_COMMAND_REVIEW_PREVIOUS
                                    ? -1
                                    : 1;
                err = begin_live_story_event(
                    runtime, resources, scheduler, state,
                    wrap_live_story_index(state->slice_index, direction));
                if (err == ESP_OK) changed = true;
            }
        } else {
            err = ESP_ERR_INVALID_ARG;
        }
        publish_live_status(runtime, state);
        jccontrol_complete(err);
    }
    return changed;
}
#endif

static esp_err_t replay_live_story_event(
    ttm_runtime_t *runtime, live_ads_resources_t *resources,
    jcengine_ads_scheduler_t *scheduler,
    const jcengine_story_scene_t *scene, uint32_t target_centiseconds,
    ttm_completion_probe_t *probe)
{
    ESP_RETURN_ON_ERROR(start_live_story_event(runtime, resources, scheduler,
                                               scene),
                        TAG, "restart live story event for rewind");
    bool island_background_changed = false;
    for (uint32_t elapsed = 0; elapsed < target_centiseconds; ++elapsed) {
        bool changed = false;
        bool complete = false;
        if (jcengine_island_tick(&runtime->island, 1)) {
            changed = true;
            if (runtime->island_dynamic_background) {
                island_background_changed = true;
            }
        }
        ESP_RETURN_ON_ERROR(tick_ads_runtime(runtime, scheduler, 1, false, probe,
                                             &changed, &complete),
                            TAG, "replay rewound live story event");
        if (complete) break;
    }
    if (island_background_changed) {
        ESP_RETURN_ON_ERROR(render_island_background(runtime), TAG,
                            "replay island background");
    }
    return compose_ttm_runtime(runtime);
}

static esp_err_t verify_ambient_frame(uint16_t *framebuffer, size_t frame_index)
{
    if (frame_index >= sizeof(EXPECTED_AMBIENT_FRAME_SHA256) /
                           sizeof(EXPECTED_AMBIENT_FRAME_SHA256[0])) {
        return ESP_ERR_INVALID_SIZE;
    }
    char actual[65] = {0};
    ESP_RETURN_ON_ERROR(sha256_hex(framebuffer,
                                   JCBOARD_WIDTH * JCBOARD_HEIGHT * sizeof(*framebuffer),
                                   actual), TAG, "hash ambient frame");
    ESP_LOGI(TAG, "ENGINE: MJAMBWLK tag=1 frame=%u framebuffer_sha256=%s",
             (unsigned)(frame_index + 1), actual);
    return strcmp(actual, EXPECTED_AMBIENT_FRAME_SHA256[frame_index]) == 0
               ? ESP_OK
               : ESP_ERR_INVALID_CRC;
}

static esp_err_t verify_delayed_pending_goto(void)
{
    static const uint8_t BYTECODE[] = {
        0x01, 0x12, 0x07, 0x00,
        0x21, 0x10, 0x0a, 0x00,
        0xf0, 0x0f,
        0x21, 0x10, 0x06, 0x00,
        0xf0, 0x0f,
    };
    jcrez_ttm_bookmark_t bookmark = {.id = 7, .offset = 10};
    jcrez_ttm_t ttm = {
        .bytecode = (uint8_t *)BYTECODE,
        .bytecode_size = sizeof(BYTECODE),
        .bookmarks = &bookmark,
        .bookmark_count = 1,
    };
    jcengine_ttm_thread_t thread = {0};
    bool advanced = false;
    ESP_RETURN_ON_ERROR(jcengine_ttm_start(&thread, &ttm, 0),
                        TAG, "start pending GOTO probe");
    ESP_RETURN_ON_ERROR(jcengine_ttm_advance(&thread, NULL, NULL),
                        TAG, "advance pending GOTO probe");
    ESP_RETURN_ON_FALSE(thread.ip == 10 && thread.timer == 10 &&
                            thread.next_goto_offset == 10,
                        ESP_ERR_INVALID_RESPONSE, TAG, "GOTO was not deferred");
    ESP_RETURN_ON_ERROR(jcengine_ttm_tick(&thread, 9, NULL, NULL, &advanced),
                        TAG, "tick pending GOTO probe");
    ESP_RETURN_ON_FALSE(!advanced && thread.timer == 1 &&
                            thread.next_goto_offset == 10,
                        ESP_ERR_INVALID_RESPONSE, TAG, "GOTO ran before delay expiry");
    ESP_RETURN_ON_ERROR(jcengine_ttm_tick(&thread, 1, NULL, NULL, &advanced),
                        TAG, "expire pending GOTO probe");
    ESP_RETURN_ON_FALSE(advanced && thread.ip == sizeof(BYTECODE) &&
                            thread.timer == 6 && thread.next_goto_offset == 0,
                        ESP_ERR_INVALID_RESPONSE, TAG, "GOTO did not run at delay expiry");
    return ESP_OK;
}

static esp_err_t verify_ttm_delay_reload(void)
{
    static const uint8_t BYTECODE[] = {
        0x21, 0x10, 0x08, 0x00,
        0xf0, 0x0f,
        0xf0, 0x0f,
        0x10, 0x01,
    };
    jcrez_ttm_t ttm = {
        .bytecode = (uint8_t *)BYTECODE,
        .bytecode_size = sizeof(BYTECODE),
    };
    jcengine_ttm_thread_t thread = {0};
    bool advanced = false;
    ESP_RETURN_ON_ERROR(jcengine_ttm_start(&thread, &ttm, 0),
                        TAG, "start delay reload probe");
    ESP_RETURN_ON_ERROR(jcengine_ttm_advance(&thread, NULL, NULL),
                        TAG, "advance delay reload probe");
    ESP_RETURN_ON_ERROR(jcengine_ttm_tick(&thread, 8, NULL, NULL, &advanced),
                        TAG, "tick delay reload probe");
    ESP_RETURN_ON_FALSE(advanced && thread.ip == 8 && thread.timer == 8,
                        ESP_ERR_INVALID_RESPONSE, TAG,
                        "TTM delay was not reloaded after UPDATE");
    ESP_RETURN_ON_ERROR(jcengine_ttm_tick(&thread, 8, NULL, NULL, &advanced),
                        TAG, "finish delay reload probe");
    ESP_RETURN_ON_FALSE(advanced && thread.status == JCENGINE_TTM_FINISHED &&
                            thread.timer == 8,
                        ESP_ERR_INVALID_RESPONSE, TAG,
                        "final TTM frame did not retain its delay");
    ESP_RETURN_ON_ERROR(jcengine_ttm_tick(&thread, 7, NULL, NULL, &advanced),
                        TAG, "hold final TTM frame");
    ESP_RETURN_ON_FALSE(!advanced && thread.timer == 1,
                        ESP_ERR_INVALID_RESPONSE, TAG,
                        "final TTM frame hold ended too early");
    ESP_RETURN_ON_ERROR(jcengine_ttm_tick(&thread, 1, NULL, NULL, &advanced),
                        TAG, "expire final TTM frame hold");
    ESP_RETURN_ON_FALSE(!advanced && thread.timer == 0,
                        ESP_ERR_INVALID_RESPONSE, TAG,
                        "final TTM frame hold did not expire");
    return ESP_OK;
}
#endif

static esp_err_t sha256_hex(const void *data, size_t size, char output[65])
{
    static const char HEX[] = "0123456789abcdef";
    uint8_t digest[32] = {0};
    if (mbedtls_sha256(data, size, digest, 0) != 0) {
        return ESP_FAIL;
    }
    for (size_t index = 0; index < sizeof(digest); ++index) {
        output[index * 2] = HEX[digest[index] >> 4];
        output[index * 2 + 1] = HEX[digest[index] & 0x0f];
    }
    output[64] = '\0';
    return ESP_OK;
}

static void log_hardware(void)
{
    esp_chip_info_t chip = {0};
    uint32_t flash_size = 0;
    esp_chip_info(&chip);
    ESP_ERROR_CHECK(esp_flash_get_size(NULL, &flash_size));
    ESP_LOGI(TAG, "firmware=%s idf=%s cores=%u revision=%u",
             "0.1.0-milestone", esp_get_idf_version(), chip.cores, chip.revision);
    ESP_LOGI(TAG, "flash=%" PRIu32 " bytes psram=%u bytes free_psram=%u bytes",
             flash_size, esp_psram_get_size(),
             heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    if (flash_size != 16U * 1024U * 1024U || esp_psram_get_size() != 8U * 1024U * 1024U) {
        ESP_LOGW(TAG, "hardware differs from planned N16R8 module");
    }
}

static void johnny_runtime_task(void *context)
{
    (void)context;
    log_hardware();

    jcboard_t board = {0};
    ESP_ERROR_CHECK(jcboard_init(&board));
    uint16_t *framebuffer = jcboard_framebuffer(&board);
    ESP_ERROR_CHECK(framebuffer == NULL ? ESP_ERR_NO_MEM : ESP_OK);

#if CONFIG_JOHNNY_BOARD_TEST
    jcgfx_draw_color_bars(framebuffer, JCBOARD_WIDTH, JCBOARD_HEIGHT);
    char framebuffer_sha256[65] = {0};
    ESP_ERROR_CHECK(sha256_hex(framebuffer,
                              JCBOARD_WIDTH * JCBOARD_HEIGHT * sizeof(*framebuffer),
                              framebuffer_sha256));
    ESP_LOGI(TAG, "board test ready: 800x480 color bars and touch cursor framebuffer_sha256=%s",
             framebuffer_sha256);
#else
    jcrez_archive_t archive = {0};
    jcrez_screen_t screen = {0};
    jcrez_palette_t palette = {0};
    ESP_ERROR_CHECK(jcrez_open(&archive));
    ESP_ERROR_CHECK(jcrez_load_palette(&archive, "JOHNCAST.PAL", &palette));
    ESP_ERROR_CHECK(jcrez_load_screen(&archive, "INTRO.SCR", &screen));

    int64_t started = esp_timer_get_time();
    ESP_ERROR_CHECK(jcgfx_render_screen(framebuffer, JCBOARD_WIDTH, JCBOARD_HEIGHT,
                                        JCGFX_LAYOUT_RIGHT, &palette, &screen));
    char framebuffer_sha256[65] = {0};
    ESP_ERROR_CHECK(sha256_hex(framebuffer,
                              JCBOARD_WIDTH * JCBOARD_HEIGHT * sizeof(*framebuffer),
                              framebuffer_sha256));
    ESP_LOGI(TAG,
             "SCENE: INTRO.SCR %ux%u decoded_sha256=%s framebuffer_sha256=%s render_us=%" PRId64,
             screen.width, screen.height, screen.sha256, framebuffer_sha256,
             esp_timer_get_time() - started);
    ESP_ERROR_CHECK(strcmp(screen.sha256, EXPECTED_INTRO_SHA256) == 0 &&
                    strcmp(framebuffer_sha256, EXPECTED_RIGHT_RGB565_SHA256) == 0
                        ? ESP_OK
                        : ESP_ERR_INVALID_CRC);
    ESP_LOGI(TAG, "VERIFY: INTRO.SCR packed and Right-layout RGB565 hashes PASS");
    ESP_ERROR_CHECK(verify_story_selector());
    ESP_LOGI(TAG,
             "VERIFY: story catalog count, bounds, day/flag filters and "
             "in-order cursor wrap PASS");
    ESP_ERROR_CHECK(verify_web_control_foundation());
    ESP_LOGI(TAG,
             "VERIFY: 63-scene shuffle boundary and web setting validation PASS");
    ESP_ERROR_CHECK(verify_island_lifecycle());
    ESP_LOGI(TAG,
             "VERIFY: deterministic island, clock, flag, animation and story-day fixtures PASS");
    ESP_ERROR_CHECK(jcgfx_verify_weather_icon_fixtures());
    ESP_LOGI(TAG,
             "VERIFY: colour weather icon mapping, layers and 2x RGB565 fixtures PASS");
    jcrez_ttm_t ttm = {0};
    ESP_ERROR_CHECK(jcrez_load_ttm(&archive, "MJAMBWLK.TTM", &ttm));
    ESP_LOGI(TAG,
             "ENGINE: TTM %s version=%s bytecode=%u named_tags=%u bookmarks=%u sha256=%s",
             ttm.name, ttm.version, (unsigned)ttm.bytecode_size,
             (unsigned)ttm.tag_count, (unsigned)ttm.bookmark_count, ttm.sha256);
    ESP_ERROR_CHECK(ttm.bytecode_size == 7760 && ttm.tag_count == 61 &&
                    ttm.bookmark_count == 61 &&
                    strcmp(ttm.sha256, EXPECTED_MJAMBWLK_SHA256) == 0
                        ? ESP_OK
                        : ESP_ERR_INVALID_CRC);
    ESP_LOGI(TAG, "VERIFY: MJAMBWLK.TTM parser and tag index PASS");
    jcengine_ttm_thread_t probe_thread = {0};
    ttm_probe_t probe = {0};
    ESP_ERROR_CHECK(jcengine_ttm_start(&probe_thread, &ttm, 1));
    ESP_ERROR_CHECK(jcengine_ttm_advance(&probe_thread, collect_ttm_probe, &probe));
    static const uint16_t EXPECTED_OPCODES[] = {0xa601, 0xa524, 0xa504, 0x1021, 0x0ff0};
    ESP_ERROR_CHECK(probe.count == 5 &&
                    memcmp(probe.opcodes, EXPECTED_OPCODES, sizeof(EXPECTED_OPCODES)) == 0 &&
                    probe_thread.ip == 122 && probe_thread.delay == 10 &&
                    probe_thread.timer == 10 &&
                    probe_thread.status == JCENGINE_TTM_RUNNING
                        ? ESP_OK
                        : ESP_ERR_INVALID_RESPONSE);
    ESP_LOGI(TAG, "ENGINE: TTM tag=1 first_tick_commands=5 ip=%" PRIu32
                  " delay=%u status=running",
             probe_thread.ip, probe_thread.delay);
    ESP_LOGI(TAG, "VERIFY: TTM interpreter first tick matches desktop baseline PASS");
    ESP_ERROR_CHECK(verify_delayed_pending_goto());
    ESP_LOGI(TAG, "VERIFY: TTM pending GOTO waits for delay expiry PASS");
    ESP_ERROR_CHECK(verify_ttm_delay_reload());
    ESP_LOGI(TAG, "VERIFY: TTM delay reloads after every UPDATE PASS");
    jcrez_bitmap_t bitmap_probe = {0};
    ESP_ERROR_CHECK(jcrez_load_bitmap(&archive, "MJ_AMB.BMP", &bitmap_probe));
    ESP_LOGI(TAG,
             "ENGINE: BMP %s sprites=%u packed=%u sha256=%s",
             bitmap_probe.name, (unsigned)bitmap_probe.sprite_count,
             (unsigned)bitmap_probe.packed_size, bitmap_probe.sha256);
    ESP_ERROR_CHECK(bitmap_probe.sprite_count == 35 && bitmap_probe.packed_size == 27484 &&
                    bitmap_probe.sprites[16].width == 40 &&
                    bitmap_probe.sprites[16].height == 56 &&
                    bitmap_probe.sprites[17].width == 40 &&
                    bitmap_probe.sprites[17].height == 56 &&
                    strcmp(bitmap_probe.sha256, EXPECTED_MJ_AMB_BMP_SHA256) == 0
                        ? ESP_OK
                        : ESP_ERR_INVALID_CRC);
    ESP_LOGI(TAG, "VERIFY: MJ_AMB.BMP sprite table and packed pixels PASS");
    jcrez_release_bitmap(&bitmap_probe);
    jcrez_release_screen(&screen);
    ESP_ERROR_CHECK(jcrez_load_screen(&archive, "ISLETEMP.SCR", &screen));
    uint16_t *background_framebuffer = heap_caps_malloc(
        JCBOARD_WIDTH * JCBOARD_HEIGHT * sizeof(*background_framebuffer),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ESP_ERROR_CHECK(background_framebuffer == NULL ? ESP_ERR_NO_MEM : ESP_OK);
    uint16_t *screen_framebuffer = heap_caps_malloc(
        JCBOARD_WIDTH * JCBOARD_HEIGHT * sizeof(*screen_framebuffer),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ESP_ERROR_CHECK(screen_framebuffer == NULL ? ESP_ERR_NO_MEM : ESP_OK);
    ESP_ERROR_CHECK(jcgfx_render_screen(screen_framebuffer, JCBOARD_WIDTH,
                                        JCBOARD_HEIGHT, JCGFX_LAYOUT_RIGHT,
                                        &palette, &screen));
    memcpy(background_framebuffer, screen_framebuffer,
           JCBOARD_WIDTH * JCBOARD_HEIGHT * sizeof(*background_framebuffer));
    memcpy(framebuffer, background_framebuffer,
           JCBOARD_WIDTH * JCBOARD_HEIGHT * sizeof(*framebuffer));
    ttm_runtime_t *runtime = heap_caps_calloc(
        1, sizeof(*runtime), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ESP_ERROR_CHECK(runtime == NULL ? ESP_ERR_NO_MEM : ESP_OK);
    runtime->archive = &archive;
    runtime->palette = &palette;
    runtime->framebuffer = framebuffer;
    runtime->background_framebuffer = background_framebuffer;
    runtime->screen_framebuffer = screen_framebuffer;
    runtime->story_day = 1;
    runtime->sky_mode = JCENGINE_SKY_AUTOMATIC;
    runtime->holiday_mode = JCCONTROL_HOLIDAY_AUTOMATIC;
    memcpy(runtime->active_screen, "ISLETEMP.SCR", sizeof("ISLETEMP.SCR"));
    static const struct {
        const char *name;
        const char *framebuffer_sha256;
    } LOAD_SCREEN_FIXTURES[] = {
        {"ISLETEMP.SCR", "a68119006596c3a6402f4fadadc945df10fb32481d2996b7d8b81cdb1c8be4f1"},
        {"ISLAND2.SCR", "00e902698f0d6c463c918e104898c2d319d1ff44d07dc780d63542e461885fc9"},
        {"SUZBEACH.SCR", "ec8aa7e92ca5b077b96747c8f7cf839000f8429a960765975eeac77919287347"},
        {"JOFFICE.SCR", "6b8082aee6cfab77c9e1a4fe16017bf5473fd3277604909c5cc672f12cd662ab"},
        {"THEEND.SCR", "25a66d88dcb80a836440e793d564e07c447c2a2a8a98012a6c6287683366250a"},
    };
    ttm_runtime_thread_t screen_fixture_thread = {.runtime = runtime};
    for (size_t index = 0;
         index < sizeof(LOAD_SCREEN_FIXTURES) / sizeof(LOAD_SCREEN_FIXTURES[0]);
         ++index) {
        jcengine_ttm_command_t command = {.opcode = 0xf01f};
        memcpy(command.string_arg, LOAD_SCREEN_FIXTURES[index].name,
               strlen(LOAD_SCREEN_FIXTURES[index].name) + 1);
        ESP_ERROR_CHECK(run_ttm_command(&screen_fixture_thread, &command));
        ESP_ERROR_CHECK(strcmp(runtime->active_screen,
                               LOAD_SCREEN_FIXTURES[index].name) == 0 &&
                                strcmp(runtime->active_screen_sha256,
                                       LOAD_SCREEN_FIXTURES[index]
                                           .framebuffer_sha256) == 0
                            ? ESP_OK
                            : ESP_ERR_INVALID_CRC);
    }
    ESP_ERROR_CHECK(load_runtime_screen(runtime, "ISLETEMP.SCR"));
    ESP_LOGI(TAG,
             "VERIFY: TTM LOAD_SCREEN preserves authored SCR names and hashes PASS");
    ESP_ERROR_CHECK(register_ttm_runtime_resource(runtime, 1, &ttm));
    jcrez_ads_t stand_ads = {0};
    ESP_ERROR_CHECK(jcrez_load_ads(&archive, "STAND.ADS", &stand_ads));
    ESP_LOGI(TAG,
             "ENGINE: ADS %s version=%s bytecode=%u resources=%u named_tags=%u sha256=%s",
             stand_ads.name, stand_ads.version, (unsigned)stand_ads.bytecode_size,
             (unsigned)stand_ads.resource_count, (unsigned)stand_ads.tag_count,
             stand_ads.sha256);
    ESP_ERROR_CHECK(stand_ads.bytecode_size == 2396 &&
                    stand_ads.resource_count == 2 && stand_ads.tag_count == 15 &&
                    stand_ads.resources[0].slot == 1 &&
                    strcmp(stand_ads.resources[0].name, "MJAMBWLK.TTM") == 0 &&
                    strcmp(stand_ads.sha256, EXPECTED_STAND_ADS_SHA256) == 0
                        ? ESP_OK
                        : ESP_ERR_INVALID_CRC);
    jcengine_ads_scheduler_t *ads_scheduler = heap_caps_calloc(
        1, sizeof(*ads_scheduler), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ESP_ERROR_CHECK(ads_scheduler == NULL ? ESP_ERR_NO_MEM : ESP_OK);
    ESP_ERROR_CHECK(jcengine_ads_start(ads_scheduler, &stand_ads, 1, 0));
    ESP_ERROR_CHECK(ads_scheduler->action_count == 2 &&
                    ads_scheduler->actions[0].type == JCENGINE_ADS_ADD_SCENE &&
                    ads_scheduler->actions[0].slot == 1 &&
                    ads_scheduler->actions[0].tag == 42 &&
                    ads_scheduler->actions[1].type == JCENGINE_ADS_ADD_SCENE &&
                    ads_scheduler->actions[1].slot == 1 &&
                    ads_scheduler->actions[1].tag == 1
                        ? ESP_OK
                        : ESP_ERR_INVALID_RESPONSE);
    ESP_LOGI(TAG,
             "VERIFY: STAND.ADS tag=1 scheduled MJAMBWLK tags 42 then 1 PASS");
    ESP_ERROR_CHECK(apply_ads_actions(runtime, ads_scheduler));
    ttm_runtime_thread_t *loader_thread = &runtime->threads[0];
    ttm_runtime_thread_t *ambient_thread = &runtime->threads[1];
    ESP_ERROR_CHECK(loader_thread->allocated && ambient_thread->allocated &&
                    loader_thread->engine.scene_tag == 42 &&
                    ambient_thread->engine.scene_tag == 1 &&
                    loader_thread->engine.status == JCENGINE_TTM_FINISHED &&
                    loader_thread->resource->bitmap_slots[0].sprite_count == 35 &&
                    strcmp(loader_thread->resource->bitmap_slots[0].sha256,
                           EXPECTED_MJ_AMB_BMP_SHA256) == 0
                        ? ESP_OK
                        : ESP_ERR_INVALID_RESPONSE);
    ESP_LOGI(TAG,
             "VERIFY: STAND.ADS actions materialized as independent TTM threads PASS");
    ESP_ERROR_CHECK(compose_ttm_runtime(runtime));
    size_t ambient_frame = 0;
    ESP_ERROR_CHECK(verify_ambient_frame(framebuffer, ambient_frame++));
    ESP_LOGI(TAG, "VERIFY: first animated-story frame is command-sink rendered PASS");
    jcrez_ads_t ads = {0};
    ESP_ERROR_CHECK(jcrez_load_ads(&archive, "ACTIVITY.ADS", &ads));
    ESP_LOGI(TAG,
             "ENGINE: ADS %s version=%s bytecode=%u resources=%u named_tags=%u sha256=%s",
             ads.name, ads.version, (unsigned)ads.bytecode_size,
             (unsigned)ads.resource_count, (unsigned)ads.tag_count, ads.sha256);
    ESP_ERROR_CHECK(ads.bytecode_size == 2558 && ads.resource_count == 6 &&
                    ads.tag_count == 10 &&
                    strcmp(ads.sha256, EXPECTED_ACTIVITY_ADS_SHA256) == 0
                        ? ESP_OK
                        : ESP_ERR_INVALID_CRC);
    ESP_LOGI(TAG, "VERIFY: ACTIVITY.ADS parser and resource table PASS");
    jcengine_ads_scheduler_t *activity_scheduler = heap_caps_calloc(
        1, sizeof(*activity_scheduler), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ESP_ERROR_CHECK(activity_scheduler == NULL ? ESP_ERR_NO_MEM : ESP_OK);
    ESP_ERROR_CHECK(jcengine_ads_start(activity_scheduler, &ads, 4, 0));
    ESP_ERROR_CHECK(activity_scheduler->action_count == 1 &&
                    activity_scheduler->actions[0].slot == 2 &&
                    activity_scheduler->actions[0].tag == 1
                        ? ESP_OK
                        : ESP_ERR_INVALID_RESPONSE);
    ESP_ERROR_CHECK(jcengine_ads_scene_finished(activity_scheduler, 2, 1));
    ESP_ERROR_CHECK(activity_scheduler->action_count == 1 &&
                    activity_scheduler->actions[0].slot == 2 &&
                    activity_scheduler->actions[0].tag == 3
                        ? ESP_OK
                        : ESP_ERR_INVALID_RESPONSE);
    ESP_ERROR_CHECK(jcengine_ads_scene_finished(activity_scheduler, 2, 3));
    ESP_ERROR_CHECK(activity_scheduler->action_count == 1 &&
                    activity_scheduler->actions[0].slot == 2 &&
                    activity_scheduler->actions[0].tag == 2
                        ? ESP_OK
                        : ESP_ERR_INVALID_RESPONSE);
    ESP_ERROR_CHECK(jcengine_ads_scene_finished(activity_scheduler, 2, 2));
    ESP_ERROR_CHECK(activity_scheduler->action_count == 0
                        ? ESP_OK
                        : ESP_ERR_INVALID_RESPONSE);
    ESP_LOGI(TAG,
             "VERIFY: ACTIVITY.ADS tag=4 completion chain 2:1 -> 2:3 -> 2:2 PASS");
    ESP_ERROR_CHECK(jcengine_ads_start(activity_scheduler, &ads, 1, 0));
    ESP_ERROR_CHECK(activity_scheduler->action_count == 2 &&
                    activity_scheduler->actions[0].slot == 1 &&
                    activity_scheduler->actions[0].tag == 12 &&
                    activity_scheduler->actions[1].slot == 1 &&
                    activity_scheduler->actions[1].tag == 13 &&
                    activity_scheduler->scene_count == 2 &&
                    activity_scheduler->scenes[0].running &&
                    activity_scheduler->scenes[1].running
                        ? ESP_OK
                        : ESP_ERR_INVALID_RESPONSE);
    ESP_LOGI(TAG,
             "VERIFY: ACTIVITY.ADS tag=1 scheduled concurrent scenes 1:12 and 1:13 PASS");
    jcrez_ttm_t *activity_ttms = heap_caps_calloc(
        ads.resource_count, sizeof(*activity_ttms),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ESP_ERROR_CHECK(activity_ttms == NULL ? ESP_ERR_NO_MEM : ESP_OK);
    uint16_t *activity_framebuffer = heap_caps_malloc(
        JCBOARD_WIDTH * JCBOARD_HEIGHT * sizeof(*activity_framebuffer),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ESP_ERROR_CHECK(activity_framebuffer == NULL ? ESP_ERR_NO_MEM : ESP_OK);
    ttm_runtime_t *activity_runtime = heap_caps_calloc(
        1, sizeof(*activity_runtime), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ESP_ERROR_CHECK(activity_runtime == NULL ? ESP_ERR_NO_MEM : ESP_OK);
    activity_runtime->archive = &archive;
    activity_runtime->palette = &palette;
    activity_runtime->framebuffer = activity_framebuffer;
    activity_runtime->background_framebuffer = background_framebuffer;
    activity_runtime->screen_framebuffer = screen_framebuffer;
    activity_runtime->story_day = 1;
    activity_runtime->sky_mode = JCENGINE_SKY_AUTOMATIC;
    activity_runtime->holiday_mode = JCCONTROL_HOLIDAY_AUTOMATIC;
    memcpy(activity_runtime->active_screen, "ISLETEMP.SCR",
           sizeof("ISLETEMP.SCR"));
    activity_runtime->stored_framebuffer = heap_caps_malloc(
        JCBOARD_WIDTH * JCBOARD_HEIGHT *
            sizeof(*activity_runtime->stored_framebuffer),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ESP_ERROR_CHECK(activity_runtime->stored_framebuffer == NULL
                        ? ESP_ERR_NO_MEM
                        : ESP_OK);
    clear_stored_framebuffer(activity_runtime);
    for (size_t index = 0; index < ads.resource_count; ++index) {
        ESP_ERROR_CHECK(jcrez_load_ttm(&archive, ads.resources[index].name,
                                       &activity_ttms[index]));
        ESP_ERROR_CHECK(register_ttm_runtime_resource(
            activity_runtime, ads.resources[index].slot,
            &activity_ttms[index]));
    }
    ESP_ERROR_CHECK(apply_ads_actions(activity_runtime, activity_scheduler));
    ESP_ERROR_CHECK(activity_runtime->threads[0].allocated &&
                    activity_runtime->threads[0].engine.scene_tag == 12 &&
                    activity_runtime->threads[0].engine.status == JCENGINE_TTM_FINISHED &&
                    activity_runtime->threads[1].allocated &&
                    activity_runtime->threads[1].engine.scene_tag == 13 &&
                    activity_runtime->threads[1].engine.status == JCENGINE_TTM_RUNNING &&
                    activity_runtime->threads[1].draw_count == 1
                        ? ESP_OK
                        : ESP_ERR_INVALID_RESPONSE);
    ESP_ERROR_CHECK(compose_ttm_runtime(activity_runtime));
    char activity_layer_sha256[65] = {0};
    ESP_ERROR_CHECK(sha256_hex(
        activity_framebuffer,
        JCBOARD_WIDTH * JCBOARD_HEIGHT * sizeof(*activity_framebuffer),
        activity_layer_sha256));
    ESP_ERROR_CHECK(strcmp(activity_layer_sha256,
                           EXPECTED_ACTIVITY_LAYER_SHA256) == 0
                        ? ESP_OK
                        : ESP_ERR_INVALID_CRC);
    ESP_LOGI(TAG,
             "VERIFY: ACTIVITY.ADS actions run as independent ordered layers "
             "framebuffer_sha256=%s PASS",
             activity_layer_sha256);
    static const uint16_t EXPECTED_ACTIVITY_COMPLETION_SLOTS[] = {
        1, 1, 1, 1, 2, 1, 1, 1, 2, 1, 1, 1,
    };
    static const uint16_t EXPECTED_ACTIVITY_COMPLETION_TAGS[] = {
        12, 13, 8, 14, 2, 13, 9, 14, 2, 13, 9, 11,
    };
    static const uint16_t EXPECTED_ACTIVITY_TAG12_COMPLETION_SLOTS[] = {
        4, 4, 4, 4, 4, 4, 4, 4, 4,
    };
    static const uint16_t EXPECTED_ACTIVITY_TAG12_COMPLETION_TAGS[] = {
        110, 24, 98, 100, 101, 105, 106, 103, 107,
    };
    static const uint16_t EXPECTED_ACTIVITY_TAG11_COMPLETION_SLOTS[] = {
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    };
    static const uint16_t EXPECTED_ACTIVITY_TAG11_COMPLETION_TAGS[] = {
        42, 20, 3, 12, 11, 9, 17, 21, 14, 15, 10, 16, 22, 8,
        18, 24, 25, 19, 26, 28, 37, 45, 29, 31, 39, 32, 40, 44,
    };
    static const uint16_t EXPECTED_ACTIVITY_TAG10_COMPLETION_SLOTS[] = {
        4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    };
    static const uint16_t EXPECTED_ACTIVITY_TAG10_COMPLETION_TAGS[] = {
        110, 24, 114, 108, 92, 93, 109, 84, 85, 119, 96, 118, 116,
    };
    ttm_completion_probe_t activity_probe = {0};
    bool activity_complete = false;
    for (size_t tick = 0; tick < 512 && !activity_complete; ++tick) {
        bool activity_changed = false;
        ESP_ERROR_CHECK(tick_ads_runtime(
            activity_runtime, activity_scheduler, LOGIC_PERIOD_CENTISECONDS,
            true, &activity_probe, &activity_changed, &activity_complete));
        vTaskDelay(1);
    }
    ESP_ERROR_CHECK(activity_complete && activity_scheduler->stop_requested &&
                            activity_probe.count ==
                                sizeof(EXPECTED_ACTIVITY_COMPLETION_TAGS) /
                                    sizeof(EXPECTED_ACTIVITY_COMPLETION_TAGS[0])
                        ? ESP_OK
                        : ESP_ERR_INVALID_RESPONSE);
    for (size_t index = 0; index < activity_probe.count; ++index) {
        ESP_ERROR_CHECK(activity_probe.slots[index] ==
                                EXPECTED_ACTIVITY_COMPLETION_SLOTS[index] &&
                                activity_probe.tags[index] ==
                                    EXPECTED_ACTIVITY_COMPLETION_TAGS[index]
                            ? ESP_OK
                            : ESP_ERR_INVALID_RESPONSE);
    }
    char activity_final_sha256[65] = {0};
    ESP_ERROR_CHECK(sha256_hex(
        activity_framebuffer,
        JCBOARD_WIDTH * JCBOARD_HEIGHT * sizeof(*activity_framebuffer),
        activity_final_sha256));
    ESP_ERROR_CHECK(strcmp(activity_final_sha256,
                           EXPECTED_ACTIVITY_FINAL_SHA256) == 0
                        ? ESP_OK
                        : ESP_ERR_INVALID_CRC);
    ESP_LOGI(TAG,
             "VERIFY: ACTIVITY.ADS tag=1 complete runtime chain "
             "12 deterministic scene completions "
             "framebuffer_sha256=%s PASS",
             activity_final_sha256);
    ttm_runtime_resource_t *activity_gjdive_resource =
        find_ttm_runtime_resource(activity_runtime, 1);
    ttm_runtime_resource_t *activity_mjdive_resource =
        find_ttm_runtime_resource(activity_runtime, 2);
    ESP_ERROR_CHECK(
        activity_gjdive_resource != NULL &&
                activity_mjdive_resource != NULL &&
                strcmp(activity_gjdive_resource->bitmap_slots[0].name,
                       "MJDIVE.BMP") == 0 &&
                strcmp(activity_mjdive_resource->bitmap_slots[0].name,
                       "JOHNWALK.BMP") == 0
            ? ESP_OK
            : ESP_ERR_INVALID_RESPONSE);
    ESP_LOGI(TAG,
             "VERIFY: TTM resources retain independent bitmap slots PASS");
    release_ttm_runtime(activity_runtime);
    ESP_ERROR_CHECK(jcengine_ads_start(activity_scheduler, &ads, 12, 0));
    ESP_ERROR_CHECK(apply_ads_actions(activity_runtime, activity_scheduler));
    ttm_completion_probe_t activity_tag12_probe = {0};
    bool activity_tag12_complete = false;
    for (size_t tick = 0; tick < 1024 && !activity_tag12_complete; ++tick) {
        bool activity_changed = false;
        ESP_ERROR_CHECK(tick_ads_runtime(
            activity_runtime, activity_scheduler, LOGIC_PERIOD_CENTISECONDS,
            true, &activity_tag12_probe, &activity_changed,
            &activity_tag12_complete));
        vTaskDelay(1);
    }
    ESP_ERROR_CHECK(activity_tag12_complete &&
                            activity_scheduler->stop_requested &&
                            activity_tag12_probe.count ==
                                sizeof(EXPECTED_ACTIVITY_TAG12_COMPLETION_TAGS) /
                                    sizeof(EXPECTED_ACTIVITY_TAG12_COMPLETION_TAGS[0])
                        ? ESP_OK
                        : ESP_ERR_INVALID_RESPONSE);
    for (size_t index = 0; index < activity_tag12_probe.count; ++index) {
        ESP_ERROR_CHECK(
            activity_tag12_probe.slots[index] ==
                    EXPECTED_ACTIVITY_TAG12_COMPLETION_SLOTS[index] &&
                activity_tag12_probe.tags[index] ==
                    EXPECTED_ACTIVITY_TAG12_COMPLETION_TAGS[index]
                ? ESP_OK
                : ESP_ERR_INVALID_RESPONSE);
    }
    char activity_tag12_sha256[65] = {0};
    ESP_ERROR_CHECK(sha256_hex(
        activity_framebuffer,
        JCBOARD_WIDTH * JCBOARD_HEIGHT * sizeof(*activity_framebuffer),
        activity_tag12_sha256));
    ESP_ERROR_CHECK(strcmp(activity_tag12_sha256,
                           EXPECTED_ACTIVITY_FINAL_SHA256) == 0
                        ? ESP_OK
                        : ESP_ERR_INVALID_CRC);
    ESP_LOGI(TAG,
             "VERIFY: ACTIVITY.ADS tag=12 complete runtime chain "
             "9 deterministic scene completions framebuffer_sha256=%s PASS",
             activity_tag12_sha256);
    release_ttm_runtime(activity_runtime);
    ESP_ERROR_CHECK(jcengine_ads_start(activity_scheduler, &ads, 11, 0));
    ESP_ERROR_CHECK(apply_ads_actions(activity_runtime, activity_scheduler));
    ttm_completion_probe_t activity_tag11_probe = {0};
    bool activity_tag11_complete = false;
    for (size_t tick = 0; tick < 1024 && !activity_tag11_complete; ++tick) {
        bool activity_changed = false;
        ESP_ERROR_CHECK(tick_ads_runtime(
            activity_runtime, activity_scheduler, LOGIC_PERIOD_CENTISECONDS,
            true, &activity_tag11_probe, &activity_changed,
            &activity_tag11_complete));
        vTaskDelay(1);
    }
    ESP_ERROR_CHECK(activity_tag11_complete &&
                            activity_scheduler->stop_requested &&
                            activity_tag11_probe.count ==
                                sizeof(EXPECTED_ACTIVITY_TAG11_COMPLETION_TAGS) /
                                    sizeof(EXPECTED_ACTIVITY_TAG11_COMPLETION_TAGS[0])
                        ? ESP_OK
                        : ESP_ERR_INVALID_RESPONSE);
    for (size_t index = 0; index < activity_tag11_probe.count; ++index) {
        ESP_ERROR_CHECK(
            activity_tag11_probe.slots[index] ==
                    EXPECTED_ACTIVITY_TAG11_COMPLETION_SLOTS[index] &&
                activity_tag11_probe.tags[index] ==
                    EXPECTED_ACTIVITY_TAG11_COMPLETION_TAGS[index]
                ? ESP_OK
                : ESP_ERR_INVALID_RESPONSE);
    }
    char activity_tag11_sha256[65] = {0};
    ESP_ERROR_CHECK(sha256_hex(
        activity_framebuffer,
        JCBOARD_WIDTH * JCBOARD_HEIGHT * sizeof(*activity_framebuffer),
        activity_tag11_sha256));
    ESP_ERROR_CHECK(strcmp(activity_tag11_sha256,
                           EXPECTED_ACTIVITY_FINAL_SHA256) == 0
                        ? ESP_OK
                        : ESP_ERR_INVALID_CRC);
    ESP_LOGI(TAG,
             "VERIFY: ACTIVITY.ADS tag=11 complete runtime chain "
             "28 deterministic scene completions framebuffer_sha256=%s PASS",
             activity_tag11_sha256);
    release_ttm_runtime(activity_runtime);
    ESP_ERROR_CHECK(jcengine_ads_start(activity_scheduler, &ads, 10, 0));
    ESP_ERROR_CHECK(apply_ads_actions(activity_runtime, activity_scheduler));
    ttm_completion_probe_t activity_tag10_probe = {0};
    bool activity_tag10_complete = false;
    for (size_t tick = 0; tick < 2048 && !activity_tag10_complete; ++tick) {
        bool activity_changed = false;
        ESP_ERROR_CHECK(tick_ads_runtime(
            activity_runtime, activity_scheduler, LOGIC_PERIOD_CENTISECONDS,
            true, &activity_tag10_probe, &activity_changed,
            &activity_tag10_complete));
        vTaskDelay(1);
    }
    ESP_ERROR_CHECK(activity_tag10_complete &&
                            activity_scheduler->stop_requested &&
                            activity_tag10_probe.count ==
                                sizeof(EXPECTED_ACTIVITY_TAG10_COMPLETION_TAGS) /
                                    sizeof(EXPECTED_ACTIVITY_TAG10_COMPLETION_TAGS[0])
                        ? ESP_OK
                        : ESP_ERR_INVALID_RESPONSE);
    for (size_t index = 0; index < activity_tag10_probe.count; ++index) {
        ESP_ERROR_CHECK(
            activity_tag10_probe.slots[index] ==
                    EXPECTED_ACTIVITY_TAG10_COMPLETION_SLOTS[index] &&
                activity_tag10_probe.tags[index] ==
                    EXPECTED_ACTIVITY_TAG10_COMPLETION_TAGS[index]
                ? ESP_OK
                : ESP_ERR_INVALID_RESPONSE);
    }
    char activity_tag10_sha256[65] = {0};
    ESP_ERROR_CHECK(sha256_hex(
        activity_framebuffer,
        JCBOARD_WIDTH * JCBOARD_HEIGHT * sizeof(*activity_framebuffer),
        activity_tag10_sha256));
    ESP_ERROR_CHECK(strcmp(activity_tag10_sha256,
                           EXPECTED_ACTIVITY_FINAL_SHA256) == 0
                        ? ESP_OK
                        : ESP_ERR_INVALID_CRC);
    ESP_LOGI(TAG,
             "VERIFY: ACTIVITY.ADS tag=10 complete runtime chain "
             "13 deterministic scene completions framebuffer_sha256=%s PASS",
             activity_tag10_sha256);
    release_ttm_runtime(activity_runtime);
    for (size_t index = 0; index < ads.resource_count; ++index) {
        jcrez_release_ttm(&activity_ttms[index]);
    }
    heap_caps_free(activity_ttms);
    jcrez_release_ads(&ads);
    live_ads_resources_t *live_resources = heap_caps_calloc(
        1, sizeof(*live_resources), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ESP_ERROR_CHECK(live_resources == NULL ? ESP_ERR_NO_MEM : ESP_OK);
    size_t rendered_checkpoint_fixture_count = 0;
    size_t rewind_equivalence_fixture_count = 0;
    for (size_t event = 0; event < VALIDATED_STORY_FIXTURE_COUNT; ++event) {
        const live_story_fixture_t *fixture =
            &VALIDATED_STORY_FIXTURES[event];
        const jcengine_story_scene_t *probe_scene =
            find_story_scene_by_identity(fixture->ads_name, fixture->ads_tag);
        ESP_ERROR_CHECK(probe_scene == NULL ? ESP_ERR_NOT_FOUND : ESP_OK);
        /* Per-event framebuffer fixtures remain independent. Story-block
           continuity has its own deterministic gate above. */
        activity_runtime->block_anchor_valid = false;
        activity_runtime->block_anchor_refresh = false;
        ESP_ERROR_CHECK(start_live_story_event(
            activity_runtime, live_resources, activity_scheduler, probe_scene));
        if (strcmp(fixture->ads_name, "VISITOR.ADS") == 0 &&
            fixture->ads_tag == 5) {
            ttm_runtime_resource_t *gjvis5 =
                find_ttm_runtime_resource(activity_runtime, 5);
            ESP_ERROR_CHECK(
                gjvis5 != NULL &&
                        gjvis5->bitmap_slots[1].packed_pixels != NULL
                    ? ESP_OK
                    : ESP_ERR_INVALID_RESPONSE);
            ESP_LOGI(TAG,
                     "VERIFY: VISITOR.ADS tag=5 canonical GJVIS5 slot-1 "
                     "dependency owned before playback PASS");
        }
        ttm_completion_probe_t batch_probe = {0};
        bool batch_complete = false;
        char checkpoint_sha256[65] = {0};
        char forward_rewind_sha256[65] = {0};
        uint32_t checkpoint_cs =
            fixture->checkpoint_frame == 0
                ? 0
                : centiseconds_for_displayed_frame(fixture->checkpoint_frame);
        uint32_t rewind_frame = fixture->checkpoint_frame > 10
                                    ? fixture->checkpoint_frame - 10
                                    : 0;
        uint32_t rewind_cs = centiseconds_for_displayed_frame(rewind_frame);
        bool island_background_changed = false;
        for (size_t tick = 0; tick < 32768 && !batch_complete; ++tick) {
            bool batch_changed = false;
            if (jcengine_island_tick(&activity_runtime->island, 1) &&
                activity_runtime->island_dynamic_background) {
                island_background_changed = true;
            }
            ESP_ERROR_CHECK(tick_ads_runtime(
                activity_runtime, activity_scheduler, 1,
                false, &batch_probe, &batch_changed, &batch_complete));
            if (checkpoint_cs != 0 && tick + 1 == checkpoint_cs) {
                if (island_background_changed) {
                    ESP_ERROR_CHECK(
                        render_island_background(activity_runtime));
                    island_background_changed = false;
                }
                ESP_ERROR_CHECK(compose_ttm_runtime(activity_runtime));
                ESP_ERROR_CHECK(sha256_hex(
                    activity_framebuffer,
                    JCBOARD_WIDTH * JCBOARD_HEIGHT *
                        sizeof(*activity_framebuffer),
                    checkpoint_sha256));
            }
            if (rewind_cs != 0 && tick + 1 == rewind_cs) {
                if (island_background_changed) {
                    ESP_ERROR_CHECK(
                        render_island_background(activity_runtime));
                    island_background_changed = false;
                }
                ESP_ERROR_CHECK(compose_ttm_runtime(activity_runtime));
                ESP_ERROR_CHECK(sha256_hex(
                    activity_framebuffer,
                    JCBOARD_WIDTH * JCBOARD_HEIGHT *
                        sizeof(*activity_framebuffer),
                    forward_rewind_sha256));
            }
            if ((tick % LOGIC_PERIOD_CENTISECONDS) == 0) vTaskDelay(1);
        }
        if (island_background_changed) {
            ESP_ERROR_CHECK(render_island_background(activity_runtime));
        }
        ESP_ERROR_CHECK(compose_ttm_runtime(activity_runtime));
        ESP_ERROR_CHECK(
            batch_complete &&
                    batch_probe.count == fixture->completion_count &&
                    batch_probe.sequence_hash == fixture->completion_hash &&
                    activity_scheduler->stop_requested ==
                        fixture->stop_requested
                ? ESP_OK
                : ESP_ERR_INVALID_RESPONSE);
        if (fixture->draw_command_peak != 0) {
            ESP_ERROR_CHECK(
                activity_runtime->draw_command_peak ==
                            fixture->draw_command_peak &&
                        activity_runtime->store_area_count ==
                            fixture->store_area_count
                    ? ESP_OK
                    : ESP_ERR_INVALID_RESPONSE);
        }
        if (fixture->checkpoint_frame != 0) {
            ESP_LOGI(TAG,
                     "VERIFY: checkpoint event=%s#%u frame=%" PRIu32
                     " framebuffer_sha256=%s",
                     fixture->ads_name, (unsigned)fixture->ads_tag,
                     fixture->checkpoint_frame, checkpoint_sha256);
            ESP_ERROR_CHECK(
                strcmp(checkpoint_sha256, fixture->checkpoint_sha256) == 0
                    ? ESP_OK
                    : ESP_ERR_INVALID_CRC);
            ++rendered_checkpoint_fixture_count;
        }
        if (strcmp(fixture->ads_name, "BUILDING.ADS") == 0 &&
            fixture->ads_tag == 4) {
            ESP_ERROR_CHECK(activity_runtime->draw_screen_count == 1 &&
                                    activity_runtime->store_area_count == 2 &&
                                    activity_runtime->stored_pixel_peak > 0 &&
                                    activity_runtime->line_draw_count > 0
                                ? ESP_OK
                                : ESP_ERR_INVALID_RESPONSE);
            ESP_LOGI(TAG,
                     "VERIFY: scene 4 retained DRAW_SCREEN/STORE_AREA pixels "
                     "and rope line commands peak_pixels=%u lines=%u PASS",
                     (unsigned)activity_runtime->stored_pixel_peak,
                     (unsigned)activity_runtime->line_draw_count);
        } else if (strcmp(fixture->ads_name, "BUILDING.ADS") == 0 &&
                   fixture->ads_tag == 2) {
            ESP_LOGI(TAG,
                     "VERIFY: scene 2 retention probe retained=%u "
                     "draw_screen=%u store_area=%u peak_pixels=%u",
                     activity_runtime->sandcastle_clip_retained ? 1U : 0U,
                     (unsigned)activity_runtime->draw_screen_count,
                     (unsigned)activity_runtime->store_area_count,
                     (unsigned)activity_runtime->stored_pixel_peak);
            ESP_ERROR_CHECK(activity_runtime->sandcastle_clip_retained &&
                                    activity_runtime->draw_screen_count == 3 &&
                                    activity_runtime->store_area_count == 3 &&
                                    activity_runtime->stored_pixel_peak > 0
                                ? ESP_OK
                                : ESP_ERR_INVALID_RESPONSE);
            ESP_LOGI(TAG,
                     "VERIFY: scene 2 lower-left sandcastle and scene 5 "
                     "DRAW_SCREEN/STORE_AREA pixels retained "
                     "peak_pixels=%u PASS",
                     (unsigned)activity_runtime->stored_pixel_peak);
        }
        char batch_sha256[65] = {0};
        ESP_ERROR_CHECK(sha256_hex(
            activity_framebuffer,
            JCBOARD_WIDTH * JCBOARD_HEIGHT * sizeof(*activity_framebuffer),
            batch_sha256));
        if (strcmp(batch_sha256, fixture->framebuffer_sha256) != 0) {
            ESP_LOGE(TAG,
                     "VERIFY: validated event=%u/%u framebuffer changed "
                     "actual=%s expected=%s",
                     (unsigned)(event + 1),
                     (unsigned)VALIDATED_STORY_FIXTURE_COUNT, batch_sha256,
                     fixture->framebuffer_sha256);
        }
        ESP_ERROR_CHECK(strcmp(batch_sha256, fixture->framebuffer_sha256) == 0
                            ? ESP_OK
                            : ESP_ERR_INVALID_CRC);
        ESP_LOGI(TAG,
                 "VERIFY: validated event=%u/%u %s tag=%u completions=%u "
                 "completion_hash=%016" PRIx64
                 " framebuffer_sha256=%s peak_draws=%u store=%u PASS",
                 (unsigned)(event + 1),
                 (unsigned)VALIDATED_STORY_FIXTURE_COUNT,
                 probe_scene->ads_name, probe_scene->ads_tag,
                 (unsigned)batch_probe.count,
                 batch_probe.sequence_hash, batch_sha256,
                 (unsigned)activity_runtime->draw_command_peak,
                 (unsigned)activity_runtime->store_area_count);
        if (rewind_cs != 0) {
            ttm_completion_probe_t rewind_probe = {0};
            ESP_ERROR_CHECK(replay_live_story_event(
                activity_runtime, live_resources, activity_scheduler,
                probe_scene, rewind_cs, &rewind_probe));
            char replay_rewind_sha256[65] = {0};
            ESP_ERROR_CHECK(sha256_hex(
                activity_framebuffer,
                JCBOARD_WIDTH * JCBOARD_HEIGHT *
                    sizeof(*activity_framebuffer),
                replay_rewind_sha256));
            ESP_ERROR_CHECK(
                forward_rewind_sha256[0] != '\0' &&
                        strcmp(forward_rewind_sha256,
                               replay_rewind_sha256) == 0
                    ? ESP_OK
                    : ESP_ERR_INVALID_CRC);
            ++rewind_equivalence_fixture_count;
            ESP_LOGI(TAG,
                     "VERIFY: exact Back 10 Frames replay event=%s#%u "
                     "frame=%" PRIu32 " framebuffer_sha256=%s PASS",
                     fixture->ads_name, (unsigned)fixture->ads_tag,
                     rewind_frame, replay_rewind_sha256);
        }
    }
    ESP_ERROR_CHECK(rendered_checkpoint_fixture_count == 3 &&
                            rewind_equivalence_fixture_count == 2
                        ? ESP_OK
                        : ESP_ERR_INVALID_RESPONSE);
    ESP_LOGI(TAG,
             "VERIFY: rendered z-order/island checkpoints and exact rewind "
             "equivalence fixtures PASS");
    for (size_t index = 0; index < LIVE_STORY_COUNT; ++index) {
        const jcengine_story_scene_t *catalog_scene =
            jcengine_story_scene(index);
        ESP_ERROR_CHECK(catalog_scene == NULL ? ESP_ERR_NOT_FOUND : ESP_OK);
        ESP_ERROR_CHECK(start_live_story_event(
            activity_runtime, live_resources, activity_scheduler,
            catalog_scene));
        vTaskDelay(1);
    }
    ESP_LOGI(TAG,
             "VERIFY: all %u catalog scenes load and start through the live runtime PASS",
             (unsigned)LIVE_STORY_COUNT);
    activity_runtime->framebuffer = framebuffer;
    heap_caps_free(activity_framebuffer);
    ESP_ERROR_CHECK(wrap_live_story_index(0, -1) == LIVE_STORY_COUNT - 1 &&
                            wrap_live_story_index(LIVE_STORY_COUNT - 1, 1) == 0 &&
                            displayed_frame_from_centiseconds(0) == 1 &&
                            displayed_frame_from_centiseconds(33) == 10 &&
                            centiseconds_for_displayed_frame(1) == 0 &&
                            centiseconds_for_displayed_frame(10) == 30
                        ? ESP_OK
                        : ESP_ERR_INVALID_RESPONSE);
    ESP_ERROR_CHECK(
        jcgfx_validation_sidebar_hit_test(656, 112) ==
                    JCGFX_VALIDATION_CONTROL_PAUSE &&
                jcgfx_validation_sidebar_hit_test(656, 164) ==
                    JCGFX_VALIDATION_CONTROL_BACK_10 &&
                jcgfx_validation_sidebar_hit_test(656, 216) ==
                    JCGFX_VALIDATION_CONTROL_PREVIOUS_SCENE &&
                jcgfx_validation_sidebar_hit_test(656, 268) ==
                    JCGFX_VALIDATION_CONTROL_NEXT_SCENE &&
                jcgfx_validation_sidebar_hit_test(656, 324) ==
                    JCGFX_VALIDATION_CONTROL_OK &&
                jcgfx_validation_sidebar_hit_test(656, 380) ==
                    JCGFX_VALIDATION_CONTROL_REVIEW &&
                jcgfx_validation_sidebar_hit_test(640, 450) ==
                    JCGFX_VALIDATION_CONTROL_NONE
            ? ESP_OK
            : ESP_ERR_INVALID_RESPONSE);
    live_story_state_t review_probe = {0};
    size_t next_review_index = 0;
    ESP_ERROR_CHECK(
        record_live_story_review(&review_probe,
                                 JCGFX_VALIDATION_REVIEW_OK,
                                 &next_review_index) &&
                next_review_index == 1 &&
                review_probe.reviews[0] == JCGFX_VALIDATION_REVIEW_OK
            ? ESP_OK
            : ESP_ERR_INVALID_RESPONSE);
    review_probe.slice_index = 1;
    ESP_ERROR_CHECK(
        record_live_story_review(&review_probe,
                                 JCGFX_VALIDATION_REVIEW_NEEDS_REVIEW,
                                 &next_review_index) &&
                next_review_index == 2 &&
                review_probe.reviews[1] ==
                    JCGFX_VALIDATION_REVIEW_NEEDS_REVIEW
            ? ESP_OK
            : ESP_ERR_INVALID_RESPONSE);
    for (size_t index = 2; index < LIVE_STORY_COUNT; ++index) {
        review_probe.slice_index = index;
        bool has_next = record_live_story_review(
            &review_probe, JCGFX_VALIDATION_REVIEW_OK, &next_review_index);
        ESP_ERROR_CHECK((index + 1 < LIVE_STORY_COUNT) == has_next
                            ? ESP_OK
                            : ESP_ERR_INVALID_RESPONSE);
    }
    review_probe.slice_index = 2;
    ESP_ERROR_CHECK(
        !record_live_story_review(&review_probe,
                                  JCGFX_VALIDATION_REVIEW_NEEDS_REVIEW,
                                  &next_review_index) &&
                review_probe.review_complete && review_probe.paused &&
                review_probe.reviews[2] ==
                    JCGFX_VALIDATION_REVIEW_NEEDS_REVIEW
            ? ESP_OK
            : ESP_ERR_INVALID_RESPONSE);
    live_story_state_t persistence_probe = {0};
    apply_live_story_review_masks(
        &persistence_probe, UINT64_C(1) << 0,
        (UINT64_C(1) << 1) | (UINT64_C(1) << 62));
    size_t persistence_next = 0;
    uint64_t persistence_ok = 0;
    uint64_t persistence_review = 0;
    live_story_review_masks(&persistence_probe, &persistence_ok,
                            &persistence_review);
    ESP_ERROR_CHECK(
        live_story_catalog_fingerprint() != 0 &&
                persistence_ok == (UINT64_C(1) << 0) &&
                persistence_review ==
                    ((UINT64_C(1) << 1) | (UINT64_C(1) << 62)) &&
                find_unreviewed_scene(&persistence_probe, 0,
                                      &persistence_next) &&
                persistence_next == 2 && !persistence_probe.review_complete
            ? ESP_OK
            : ESP_ERR_INVALID_RESPONSE);
    ESP_LOGI(TAG,
             "VERIFY: 63-scene review masks, catalog fingerprint, wrap, frame conversion and six sidebar hit targets PASS");
#if CONFIG_JOHNNY_REVIEW_ONLY
    live_story_state_t shortlist_probe = {0};
    apply_live_story_review_masks(
        &shortlist_probe, 0,
        (UINT64_C(1) << 1) | (UINT64_C(1) << 3) |
            (UINT64_C(1) << 62));
    shortlist_probe.review_pass_pending = live_story_review_mask(&shortlist_probe);
    size_t shortlist_position = 0;
    size_t shortlist_count = 0;
    size_t shortlist_next = 0;
    ESP_ERROR_CHECK(
        review_shortlist_position(&shortlist_probe, 3, &shortlist_position,
                                  &shortlist_count) &&
                shortlist_position == 1 && shortlist_count == 3 &&
                find_review_scene(&shortlist_probe, 1, -1, false,
                                  &shortlist_next) &&
                shortlist_next == 62 &&
                find_review_scene(&shortlist_probe, 62, 1, false,
                                  &shortlist_next) &&
                shortlist_next == 1
            ? ESP_OK
            : ESP_ERR_INVALID_RESPONSE);
    shortlist_probe.reviews[1] = JCGFX_VALIDATION_REVIEW_OK;
    ESP_ERROR_CHECK(
        finish_review_only_decision(&shortlist_probe, 1, &shortlist_next) &&
                shortlist_next == 3 && !shortlist_probe.all_resolved
            ? ESP_OK
            : ESP_ERR_INVALID_RESPONSE);
    ESP_ERROR_CHECK(
        finish_review_only_decision(&shortlist_probe, 3, &shortlist_next) &&
                shortlist_next == 62
            ? ESP_OK
            : ESP_ERR_INVALID_RESPONSE);
    shortlist_probe.reviews[62] = JCGFX_VALIDATION_REVIEW_OK;
    ESP_ERROR_CHECK(
        finish_review_only_decision(&shortlist_probe, 62, &shortlist_next) &&
                shortlist_next == 3 &&
                shortlist_probe.review_pass_pending == (UINT64_C(1) << 3)
            ? ESP_OK
            : ESP_ERR_INVALID_RESPONSE);
    shortlist_probe.reviews[3] = JCGFX_VALIDATION_REVIEW_OK;
    ESP_ERROR_CHECK(
        !finish_review_only_decision(&shortlist_probe, 3, &shortlist_next) &&
                shortlist_probe.all_resolved && shortlist_probe.paused
            ? ESP_OK
            : ESP_ERR_INVALID_RESPONSE);
    memset(&shortlist_probe, 0, sizeof(shortlist_probe));
    apply_live_story_review_masks(
        &shortlist_probe, 0,
        ((UINT64_C(1) << 12) - 1) << 38);
    const jcengine_story_scene_t *stand_first = jcengine_story_scene(38);
    const jcengine_story_scene_t *stand_last = jcengine_story_scene(49);
    ESP_ERROR_CHECK(
        find_next_stand_review_loop_scene(&shortlist_probe, stand_first,
                                          &shortlist_next) &&
                shortlist_next == 39 &&
                find_next_stand_review_loop_scene(&shortlist_probe, stand_last,
                                                  &shortlist_next) &&
                shortlist_next == 38
            ? ESP_OK
            : ESP_ERR_INVALID_RESPONSE);
    ESP_LOGI(TAG,
             "VERIFY: REVIEW-only filtering, removal, wrap, STAND 1-12 loop, pass export and all-resolved state PASS");
#endif
    /* Existing deterministic boot fixtures retain their recorded baseline.
       Live catalog playback uses the desktop negative ADD_SCENE lifetime
       contract required by BUILDING.ADS#7 and other authored events. */
    activity_runtime->honor_scene_lifetimes = true;
    live_story_state_t live_story = {0};
    ESP_ERROR_CHECK(jccontrol_init());
#if CONFIG_JOHNNY_REVIEW_ONLY
    ESP_ERROR_CHECK(load_live_story_reviews(&live_story));
#else
    ESP_ERROR_CHECK(nvs_flash_init());
#endif
    ESP_ERROR_CHECK(load_story_progress(activity_runtime));
#if CONFIG_JOHNNY_REVIEW_ONLY
    uint64_t saved_review_mask = live_story_review_mask(&live_story);
    live_story.review_pass_pending = saved_review_mask;
    live_story.all_resolved = saved_review_mask == 0;
    if (live_story.all_resolved) {
        live_story.review_complete = true;
        live_story.paused = true;
        log_review_shortlist(&live_story);
        ESP_LOGI(TAG, "REVIEW-ONLY: ALL RESOLVED");
    } else {
        size_t first_review_index = 0;
        live_story.review_complete = false;
        live_story.paused = false;
        ESP_ERROR_CHECK((find_first_review_scene_named(
                             &live_story, "STAND.ADS", &first_review_index) ||
                         find_review_scene(&live_story, 0, 1, true,
                                           &first_review_index))
                            ? ESP_OK
                            : ESP_ERR_NOT_FOUND);
        ESP_LOGI(TAG, "REVIEW-ONLY: active shortlist count=%u",
                 (unsigned)__builtin_popcountll(saved_review_mask));
        ESP_ERROR_CHECK(begin_review_scene_or_continue(
            activity_runtime, live_resources, activity_scheduler, &live_story,
            first_review_index));
    }
#else
    jccontrol_settings_t settings = {0};
    ESP_ERROR_CHECK(jccontrol_load_settings(&settings));
    ESP_LOGI(TAG, "SETTINGS: playback=%s sidebar=%s sky=%s holiday=%s",
             jccontrol_playback_mode_name(settings.playback_mode),
             jccontrol_sidebar_mode_name(settings.sidebar_mode),
             jccontrol_sky_name(settings.sky),
             jccontrol_holiday_name(settings.holiday));
    activity_runtime->sky_mode = settings.sky;
    activity_runtime->holiday_mode = settings.holiday;
    live_story.playback_mode = settings.playback_mode;
    live_story.sidebar_mode = settings.sidebar_mode;
    live_story.shuffle.last_scene = -1;
    jccontrol_shuffle_reset(&live_story.shuffle, esp_random());
    live_story.review_complete = false;
    live_story.paused = false;
    if (live_story.playback_mode == JCCONTROL_PLAYBACK_REVIEW) {
        ESP_ERROR_CHECK(begin_live_story_event(
            activity_runtime, live_resources, activity_scheduler, &live_story,
            0));
    } else {
        ESP_ERROR_CHECK(begin_random_scene_or_continue(
            activity_runtime, live_resources, activity_scheduler, &live_story));
    }
#endif
#if CONFIG_JOHNNY_QEMU && !CONFIG_JOHNNY_QEMU_HEADLESS
#if CONFIG_JOHNNY_REVIEW_ONLY
    jcgfx_draw_validation_all_resolved(framebuffer, JCBOARD_WIDTH,
                                       JCBOARD_HEIGHT);
#else
    jcgfx_validation_review_t qemu_summary_reviews[LIVE_STORY_COUNT] = {0};
    for (size_t index = 0; index < LIVE_STORY_COUNT; ++index) {
        qemu_summary_reviews[index] = JCGFX_VALIDATION_REVIEW_OK;
    }
    qemu_summary_reviews[1] = JCGFX_VALIDATION_REVIEW_NEEDS_REVIEW;
    qemu_summary_reviews[17] = JCGFX_VALIDATION_REVIEW_NEEDS_REVIEW;
    qemu_summary_reviews[62] = JCGFX_VALIDATION_REVIEW_NEEDS_REVIEW;
    jcgfx_draw_validation_review_summary(
        framebuffer, JCBOARD_WIDTH, JCBOARD_HEIGHT, qemu_summary_reviews,
        LIVE_STORY_COUNT);
#endif
    ESP_ERROR_CHECK(jcboard_present(&board));
    ESP_LOGI(TAG, "QEMU: review summary capture ready");
    while (true) vTaskDelay(pdMS_TO_TICKS(1000));
#endif

    release_ttm_runtime(runtime);
    heap_caps_free(runtime);
    heap_caps_free(ads_scheduler);
    jcrez_release_ads(&stand_ads);
    jcrez_release_ttm(&ttm);
    jcrez_release_screen(&screen);
#endif

#if !CONFIG_JOHNNY_BOARD_TEST
#if !CONFIG_JOHNNY_REVIEW_ONLY
    ESP_ERROR_CHECK(jcnet_start());
#endif
    draw_live_story_sidebar(framebuffer, &live_story);
#endif
    ESP_ERROR_CHECK(jcboard_present(&board));

#if !CONFIG_JOHNNY_BOARD_TEST
    bool touch_was_down = false;
#endif
    TickType_t next_presentation = xTaskGetTickCount();
    bool presentation_dirty = false;
#if !CONFIG_JOHNNY_BOARD_TEST && !CONFIG_JOHNNY_REVIEW_ONLY
    int64_t next_theme_check_us = 0;
#endif
    while (true) {
#if !CONFIG_JOHNNY_BOARD_TEST
#if !CONFIG_JOHNNY_REVIEW_ONLY
        if (apply_live_control_commands(activity_runtime, live_resources,
                                        activity_scheduler, &live_story)) {
            presentation_dirty = true;
        }
        int64_t theme_now_us = esp_timer_get_time();
        if (theme_now_us >= next_theme_check_us) {
            next_theme_check_us = theme_now_us + INT64_C(60000000);
            jcengine_local_time_t updated_time = current_local_time();
            bool time_changed =
                updated_time.valid != activity_runtime->local_time.valid ||
                updated_time.hour != activity_runtime->local_time.hour ||
                updated_time.month != activity_runtime->local_time.month ||
                updated_time.day != activity_runtime->local_time.day;
            activity_runtime->local_time = updated_time;
            if (time_changed &&
                (activity_runtime->sky_mode == JCENGINE_SKY_AUTOMATIC ||
                 activity_runtime->holiday_mode ==
                     JCCONTROL_HOLIDAY_AUTOMATIC) &&
                refresh_runtime_theme(activity_runtime, live_story.scene) ==
                    ESP_OK) {
                presentation_dirty = true;
            }
        }
#endif
        int64_t animation_now_us = esp_timer_get_time();
        if (!live_story.paused) {
            live_story.animation_remainder_us +=
                animation_now_us - live_story.animation_clock_us;
        }
        live_story.animation_clock_us = animation_now_us;
        uint16_t elapsed_centiseconds =
            (uint16_t)(live_story.animation_remainder_us / CENTISECOND_US);
        live_story.animation_remainder_us %= CENTISECOND_US;
        if (!live_story.paused && elapsed_centiseconds > 0) {
            live_story.elapsed_centiseconds += elapsed_centiseconds;
            bool activity_changed = false;
            bool activity_complete = false;
            bool activity_failed = false;
            bool island_background_changed = false;
            for (uint16_t tick = 0;
                 tick < elapsed_centiseconds && !activity_complete; ++tick) {
                bool tick_changed = false;
                esp_err_t tick_err = ESP_OK;
                if (jcengine_island_tick(&activity_runtime->island, 1)) {
                    tick_changed = true;
                    if (activity_runtime->island_dynamic_background) {
                        island_background_changed = true;
                    }
                }
                if (tick_err == ESP_OK) {
                    bool ads_changed = false;
                    tick_err = tick_ads_runtime(
                        activity_runtime, activity_scheduler, 1,
                        false,
                        live_story.verified[live_story.slice_index]
                            ? NULL
                            : &live_story.probe,
                        &ads_changed, &activity_complete);
                    tick_changed = tick_changed || ads_changed;
                }
                if (tick_err != ESP_OK) {
                    size_t failed_index = live_story.slice_index;
#if CONFIG_JOHNNY_REVIEW_ONLY
                    ESP_LOGE(TAG,
                             "REVIEW: scene=%u/%u runtime failed error=%s; saved as REVIEW",
                             (unsigned)(failed_index + 1),
                             (unsigned)LIVE_STORY_COUNT,
                             esp_err_to_name(tick_err));
                    ESP_ERROR_CHECK(persist_live_story_review(
                        &live_story, failed_index,
                        JCGFX_VALIDATION_REVIEW_NEEDS_REVIEW, tick_err));
                    size_t next_index = 0;
                    if (finish_review_only_decision(&live_story, failed_index,
                                                    &next_index)) {
                        ESP_ERROR_CHECK(begin_review_scene_or_continue(
                            activity_runtime, live_resources,
                            activity_scheduler, &live_story, next_index));
                    } else {
                        ESP_LOGI(TAG, "REVIEW-ONLY: ALL RESOLVED");
                    }
#else
                    ESP_LOGE(TAG,
                             "PLAYBACK: scene=%u/%u runtime failed error=%s; skipping",
                             (unsigned)(failed_index + 1),
                             (unsigned)LIVE_STORY_COUNT,
                             esp_err_to_name(tick_err));
                    if (live_story.playback_mode ==
                        JCCONTROL_PLAYBACK_REVIEW) {
                        publish_live_status(activity_runtime, &live_story);
                        jccontrol_snapshot_t snapshot = {0};
                        jccontrol_snapshot(&snapshot);
                        snapshot.runtime_error = tick_err;
                        ESP_ERROR_CHECK(jccontrol_bug_capture(&snapshot));
                        ESP_ERROR_CHECK(begin_live_story_event(
                            activity_runtime, live_resources,
                            activity_scheduler, &live_story,
                            (failed_index + 1) % LIVE_STORY_COUNT));
                    } else {
                        ESP_ERROR_CHECK(begin_random_scene_or_continue(
                            activity_runtime, live_resources,
                            activity_scheduler, &live_story));
                    }
#endif
                    presentation_dirty = true;
                    activity_failed = true;
                    break;
                }
                activity_changed = activity_changed || tick_changed;
            }
            if (!activity_failed && activity_changed) {
                esp_err_t compose_err = island_background_changed
                                            ? render_island_background(
                                                  activity_runtime)
                                            : ESP_OK;
                if (compose_err == ESP_OK) {
                    compose_err = compose_ttm_runtime(activity_runtime);
                }
                if (compose_err != ESP_OK) {
                    size_t failed_index = live_story.slice_index;
#if CONFIG_JOHNNY_REVIEW_ONLY
                    ESP_LOGE(TAG,
                             "REVIEW: scene=%u/%u composition failed error=%s; saved as REVIEW",
                             (unsigned)(failed_index + 1),
                             (unsigned)LIVE_STORY_COUNT,
                             esp_err_to_name(compose_err));
                    ESP_ERROR_CHECK(persist_live_story_review(
                        &live_story, failed_index,
                        JCGFX_VALIDATION_REVIEW_NEEDS_REVIEW, compose_err));
                    size_t next_index = 0;
                    if (finish_review_only_decision(&live_story, failed_index,
                                                    &next_index)) {
                        ESP_ERROR_CHECK(begin_review_scene_or_continue(
                            activity_runtime, live_resources,
                            activity_scheduler, &live_story, next_index));
                    } else {
                        ESP_LOGI(TAG, "REVIEW-ONLY: ALL RESOLVED");
                    }
#else
                    ESP_LOGE(TAG,
                             "PLAYBACK: scene=%u/%u composition failed error=%s; skipping",
                             (unsigned)(failed_index + 1),
                             (unsigned)LIVE_STORY_COUNT,
                             esp_err_to_name(compose_err));
                    if (live_story.playback_mode ==
                        JCCONTROL_PLAYBACK_REVIEW) {
                        publish_live_status(activity_runtime, &live_story);
                        jccontrol_snapshot_t snapshot = {0};
                        jccontrol_snapshot(&snapshot);
                        snapshot.runtime_error = compose_err;
                        ESP_ERROR_CHECK(jccontrol_bug_capture(&snapshot));
                        ESP_ERROR_CHECK(begin_live_story_event(
                            activity_runtime, live_resources,
                            activity_scheduler, &live_story,
                            (failed_index + 1) % LIVE_STORY_COUNT));
                    } else {
                        ESP_ERROR_CHECK(begin_random_scene_or_continue(
                            activity_runtime, live_resources,
                            activity_scheduler, &live_story));
                    }
#endif
                    activity_failed = true;
                }
                presentation_dirty = true;
            }
            if (!activity_failed && activity_complete) {
                if (!live_story.verified[live_story.slice_index]) {
                    const live_story_fixture_t *fixture =
                        find_validated_story_fixture(live_story.scene);
                    ESP_LOGI(TAG,
                             "REVIEW: native candidate event=%u/%u %s tag=%u completions=%u "
                             "completion_hash=%016" PRIx64 " stop=%u peak_draws=%u",
                             (unsigned)(live_story.slice_index + 1),
                             (unsigned)LIVE_STORY_COUNT,
                             live_story.scene->ads_name,
                             live_story.scene->ads_tag,
                             (unsigned)live_story.probe.count,
                             live_story.probe.sequence_hash,
                             activity_scheduler->stop_requested ? 1U : 0U,
                             (unsigned)activity_runtime->draw_command_peak);
                    if (fixture != NULL) {
                        ESP_ERROR_CHECK(
                            activity_scheduler->stop_requested ==
                                        fixture->stop_requested &&
                                    live_story.probe.count ==
                                        fixture->completion_count &&
                                    live_story.probe.sequence_hash ==
                                        fixture->completion_hash
                                            ? ESP_OK
                                            : ESP_ERR_INVALID_RESPONSE);
                        ESP_LOGI(
                            TAG,
                            "VERIFY: story scene %s tag=%u live native-timed event completed "
                            "in order actual_us=%" PRId64 " PASS",
                            live_story.scene->ads_name,
                            live_story.scene->ads_tag,
                            esp_timer_get_time() - live_story.started_us);
                    }
                    live_story.verified[live_story.slice_index] = true;
                }
#if CONFIG_JOHNNY_REVIEW_ONLY
                size_t restart_index = live_story.slice_index;
                bool continue_stand_loop = find_next_stand_review_loop_scene(
                    &live_story, live_story.scene, &restart_index);
                if (continue_stand_loop) {
                    ESP_LOGI(TAG,
                             "REVIEW-ONLY: STAND 1-12 loop advancing to tag=%u",
                             (unsigned)jcengine_story_scene(restart_index)->ads_tag);
                } else
                {
                    ESP_LOGI(TAG,
                             "REVIEW: event=%u/%u completed without decision; restarting frame=1",
                             (unsigned)(restart_index + 1),
                             (unsigned)LIVE_STORY_COUNT);
                }
                ESP_ERROR_CHECK(begin_review_scene_or_continue(
                    activity_runtime, live_resources, activity_scheduler,
                    &live_story, restart_index));
#else
                if (live_story.playback_mode == JCCONTROL_PLAYBACK_REVIEW) {
                    ESP_LOGI(TAG,
                             "REVIEW: event=%u/%u completed; repeating until decision",
                             (unsigned)(live_story.slice_index + 1),
                             (unsigned)LIVE_STORY_COUNT);
                    ESP_ERROR_CHECK(begin_live_story_event(
                        activity_runtime, live_resources, activity_scheduler,
                        &live_story, live_story.slice_index));
                } else {
                    advance_story_block(activity_runtime);
                    ESP_ERROR_CHECK(begin_random_scene_or_continue(
                        activity_runtime, live_resources, activity_scheduler,
                        &live_story));
                }
#endif
                presentation_dirty = true;
            }
        }
        uint32_t displayed_frame =
            displayed_frame_from_centiseconds(live_story.elapsed_centiseconds);
        if (displayed_frame != live_story.displayed_frame) {
            live_story.displayed_frame = displayed_frame;
            presentation_dirty = true;
        }
#if !CONFIG_JOHNNY_REVIEW_ONLY
        publish_live_status(activity_runtime, &live_story);
#endif
#endif
        uint16_t x = 0;
        uint16_t y = 0;
        bool touch_down = jcboard_read_touch(&board, &x, &y);
#if !CONFIG_JOHNNY_BOARD_TEST
        if (touch_down && !touch_was_down) {
#if CONFIG_JOHNNY_REVIEW_ONLY
            jcgfx_validation_control_t control =
                jcgfx_validation_sidebar_hit_test(x, y);
#else
            jcgfx_validation_control_t control =
                live_story.sidebar_mode == JCCONTROL_SIDEBAR_REVIEW
                    ? jcgfx_validation_sidebar_hit_test(x, y)
                    : JCGFX_VALIDATION_CONTROL_NONE;
#endif
            if (control == JCGFX_VALIDATION_CONTROL_PAUSE &&
                !live_story.review_complete) {
                live_story.paused = !live_story.paused;
                live_story.animation_clock_us = esp_timer_get_time();
                live_story.animation_remainder_us = 0;
                presentation_dirty = true;
                next_presentation = xTaskGetTickCount();
                ESP_LOGI(TAG, "CONTROL: %s event=%u/%u frame=%" PRIu32,
                         live_story.paused ? "pause" : "play",
                         (unsigned)(live_story.slice_index + 1),
                         (unsigned)LIVE_STORY_COUNT,
                         live_story.displayed_frame);
            } else if (control == JCGFX_VALIDATION_CONTROL_BACK_10 &&
                       !live_story.review_complete) {
                uint32_t target_frame = live_story.displayed_frame > 10
                                            ? live_story.displayed_frame - 10
                                            : 1;
                uint32_t target_centiseconds =
                    centiseconds_for_displayed_frame(target_frame);
                if (!live_story.verified[live_story.slice_index]) {
                    memset(&live_story.probe, 0, sizeof(live_story.probe));
                }
                esp_err_t replay_err = replay_live_story_event(
                    activity_runtime, live_resources, activity_scheduler,
                    live_story.scene, target_centiseconds,
                    live_story.verified[live_story.slice_index]
                        ? NULL
                        : &live_story.probe);
                if (replay_err != ESP_OK) {
                    size_t failed_index = live_story.slice_index;
#if CONFIG_JOHNNY_REVIEW_ONLY
                    ESP_LOGE(TAG,
                             "REVIEW: scene=%u/%u rewind failed error=%s; saved as REVIEW",
                             (unsigned)(failed_index + 1),
                             (unsigned)LIVE_STORY_COUNT,
                             esp_err_to_name(replay_err));
                    ESP_ERROR_CHECK(persist_live_story_review(
                        &live_story, failed_index,
                        JCGFX_VALIDATION_REVIEW_NEEDS_REVIEW, replay_err));
                    size_t next_index = 0;
                    if (finish_review_only_decision(&live_story, failed_index,
                                                    &next_index)) {
                        ESP_ERROR_CHECK(begin_review_scene_or_continue(
                            activity_runtime, live_resources,
                            activity_scheduler, &live_story, next_index));
                    } else {
                        ESP_LOGI(TAG, "REVIEW-ONLY: ALL RESOLVED");
                    }
#else
                    ESP_LOGE(TAG,
                             "PLAYBACK: scene=%u rewind failed error=%s; skipping",
                             (unsigned)(failed_index + 1),
                             esp_err_to_name(replay_err));
                    if (live_story.playback_mode ==
                        JCCONTROL_PLAYBACK_REVIEW) {
                        publish_live_status(activity_runtime, &live_story);
                        jccontrol_snapshot_t snapshot = {0};
                        jccontrol_snapshot(&snapshot);
                        snapshot.runtime_error = replay_err;
                        ESP_ERROR_CHECK(jccontrol_bug_capture(&snapshot));
                        ESP_ERROR_CHECK(begin_live_story_event(
                            activity_runtime, live_resources,
                            activity_scheduler, &live_story,
                            (failed_index + 1) % LIVE_STORY_COUNT));
                    } else {
                        ESP_ERROR_CHECK(begin_random_scene_or_continue(
                            activity_runtime, live_resources,
                            activity_scheduler, &live_story));
                    }
#endif
                    presentation_dirty = true;
                    next_presentation = xTaskGetTickCount();
                    touch_was_down = touch_down;
                    continue;
                }
                live_story.elapsed_centiseconds = target_centiseconds;
                live_story.displayed_frame = target_frame;
                live_story.paused = true;
                live_story.animation_clock_us = esp_timer_get_time();
                live_story.animation_remainder_us = 0;
                live_story.started_us = live_story.animation_clock_us -
                    (int64_t)target_centiseconds * CENTISECOND_US;
                presentation_dirty = true;
                next_presentation = xTaskGetTickCount();
                ESP_LOGI(TAG,
                         "CONTROL: back 10 frames event=%u/%u frame=%" PRIu32
                         " target_cs=%" PRIu32 " paused=1",
                         (unsigned)(live_story.slice_index + 1),
                         (unsigned)LIVE_STORY_COUNT, target_frame,
                         target_centiseconds);
            } else if (control == JCGFX_VALIDATION_CONTROL_PREVIOUS_SCENE ||
                       control == JCGFX_VALIDATION_CONTROL_NEXT_SCENE) {
                int direction =
                    control == JCGFX_VALIDATION_CONTROL_PREVIOUS_SCENE ? -1 : 1;
#if CONFIG_JOHNNY_REVIEW_ONLY
                size_t target_index = live_story.slice_index;
                ESP_ERROR_CHECK(find_review_scene(
                                    &live_story, live_story.slice_index,
                                    direction, false, &target_index)
                                    ? ESP_OK
                                    : ESP_ERR_NOT_FOUND);
#else
                size_t target_index =
                    wrap_live_story_index(live_story.slice_index, direction);
#endif
#if CONFIG_JOHNNY_REVIEW_ONLY
                ESP_ERROR_CHECK(begin_review_scene_or_continue(
                    activity_runtime, live_resources, activity_scheduler,
                    &live_story, target_index));
#else
                ESP_ERROR_CHECK(begin_live_story_event(
                    activity_runtime, live_resources, activity_scheduler,
                    &live_story, target_index));
#endif
                presentation_dirty = true;
                next_presentation = xTaskGetTickCount();
                if (!live_story.review_complete && live_story.scene != NULL) {
                    ESP_LOGI(TAG,
                             "CONTROL: scene %s event=%u/%u %s tag=%u frame=1",
                             direction < 0 ? "-1" : "+1",
                             (unsigned)(live_story.slice_index + 1),
                             (unsigned)LIVE_STORY_COUNT,
                             live_story.scene->ads_name,
                             live_story.scene->ads_tag);
                }
#if CONFIG_JOHNNY_REVIEW_ONLY
            } else if ((control == JCGFX_VALIDATION_CONTROL_OK ||
                        control == JCGFX_VALIDATION_CONTROL_REVIEW) &&
                       !live_story.review_complete) {
                jcgfx_validation_review_t review =
                    control == JCGFX_VALIDATION_CONTROL_OK
                        ? JCGFX_VALIDATION_REVIEW_OK
                        : JCGFX_VALIDATION_REVIEW_NEEDS_REVIEW;
                uint32_t reviewed_frame = live_story.displayed_frame;
                size_t reviewed_index = live_story.slice_index;
                ESP_ERROR_CHECK(persist_live_story_review(
                    &live_story, reviewed_index, review, ESP_OK));
                ESP_LOGI(TAG,
                         "REVIEW: event=%u/%u title=%s %s tag=%u frame=%" PRIu32
                         " result=%s",
                         (unsigned)(live_story.slice_index + 1),
                         (unsigned)LIVE_STORY_COUNT, live_story.title,
                         live_story.scene->ads_name,
                         live_story.scene->ads_tag, reviewed_frame,
                         live_story_review_name(review));
                size_t next_index = 0;
#if CONFIG_JOHNNY_REVIEW_ONLY
                if (finish_review_only_decision(&live_story, reviewed_index,
                                                &next_index)) {
                    ESP_ERROR_CHECK(begin_review_scene_or_continue(
                        activity_runtime, live_resources, activity_scheduler,
                        &live_story, next_index));
                } else {
                    live_story.animation_clock_us = esp_timer_get_time();
                    live_story.animation_remainder_us = 0;
                    ESP_LOGI(TAG, "REVIEW-ONLY: ALL RESOLVED");
                }
#else
                if (find_unreviewed_scene(&live_story, reviewed_index + 1,
                                          &next_index)) {
                    ESP_ERROR_CHECK(begin_review_scene_or_continue(
                        activity_runtime, live_resources, activity_scheduler,
                        &live_story, next_index));
                } else {
                    live_story.review_complete = true;
                    live_story.paused = true;
                    live_story.animation_clock_us = esp_timer_get_time();
                    live_story.animation_remainder_us = 0;
                    log_live_story_review_summary(&live_story);
                }
#endif
                presentation_dirty = true;
                next_presentation = xTaskGetTickCount();
#else
            } else if ((control == JCGFX_VALIDATION_CONTROL_OK ||
                        control == JCGFX_VALIDATION_CONTROL_REVIEW) &&
                       live_story.playback_mode ==
                           JCCONTROL_PLAYBACK_REVIEW) {
                if (control == JCGFX_VALIDATION_CONTROL_REVIEW) {
                    publish_live_status(activity_runtime, &live_story);
                    jccontrol_snapshot_t snapshot = {0};
                    jccontrol_snapshot(&snapshot);
                    ESP_ERROR_CHECK(jccontrol_bug_capture(&snapshot));
                    ESP_LOGI(TAG,
                             "BUG: captured scene=%u %s tag=%u frame=%" PRIu32,
                             (unsigned)(live_story.slice_index + 1),
                             live_story.scene->ads_name,
                             live_story.scene->ads_tag,
                             live_story.displayed_frame);
                }
                advance_story_block(activity_runtime);
                ESP_ERROR_CHECK(begin_live_story_event(
                    activity_runtime, live_resources, activity_scheduler,
                    &live_story,
                    (live_story.slice_index + 1) % LIVE_STORY_COUNT));
                presentation_dirty = true;
                next_presentation = xTaskGetTickCount();
#endif
            } else if (x < 640) {
                jcgfx_draw_cursor(framebuffer, JCBOARD_WIDTH, JCBOARD_HEIGHT,
                                  x, y);
                presentation_dirty = true;
                ESP_LOGI(TAG, "touch x=%u y=%u", x, y);
            }
        }
        touch_was_down = touch_down;
#else
        if (touch_down) {
            jcgfx_draw_cursor(framebuffer, JCBOARD_WIDTH, JCBOARD_HEIGHT, x, y);
            presentation_dirty = true;
            ESP_LOGI(TAG, "touch x=%u y=%u", x, y);
        }
#endif
        if (presentation_dirty) {
#if !CONFIG_JOHNNY_BOARD_TEST
            draw_live_story_sidebar(framebuffer, &live_story);
#endif
            ESP_ERROR_CHECK(jcboard_present(&board));
            presentation_dirty = false;
        }
        vTaskDelayUntil(&next_presentation, pdMS_TO_TICKS(PRESENTATION_PERIOD_MS));
    }
}

void app_main(void)
{
    BaseType_t created = xTaskCreatePinnedToCore(
        johnny_runtime_task, "johnny_runtime", 12288, NULL, 5, NULL, 0);
    ESP_ERROR_CHECK(created == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}
