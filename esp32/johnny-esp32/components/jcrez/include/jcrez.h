#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_partition.h"

typedef struct {
    const esp_partition_t *partition;
    esp_partition_mmap_handle_t mapping;
    const uint8_t *map_data;
    size_t map_size;
    const uint8_t *archive_data;
    size_t archive_size;
} jcrez_archive_t;

typedef struct {
    uint8_t rgb[16][3];
} jcrez_palette_t;

typedef struct {
    uint16_t width;
    uint16_t height;
    uint8_t *packed_pixels;
    size_t packed_size;
    char sha256[65];
} jcrez_screen_t;

typedef struct {
    uint16_t width;
    uint16_t height;
    size_t packed_offset;
} jcrez_bitmap_sprite_t;

typedef struct {
    char name[14];
    jcrez_bitmap_sprite_t *sprites;
    size_t sprite_count;
    uint8_t *packed_pixels;
    size_t packed_size;
    char sha256[65];
} jcrez_bitmap_t;

typedef struct {
    uint16_t id;
    char description[41];
} jcrez_script_tag_t;

typedef struct {
    uint16_t id;
    uint32_t offset;
} jcrez_ttm_bookmark_t;

typedef struct {
    char name[14];
    char version[5];
    uint8_t *bytecode;
    size_t bytecode_size;
    jcrez_script_tag_t *tags;
    size_t tag_count;
    jcrez_ttm_bookmark_t *bookmarks;
    size_t bookmark_count;
    char sha256[65];
} jcrez_ttm_t;

typedef struct {
    uint16_t slot;
    char name[41];
} jcrez_ads_resource_t;

typedef struct {
    char name[14];
    char version[5];
    jcrez_ads_resource_t *resources;
    size_t resource_count;
    uint8_t *bytecode;
    size_t bytecode_size;
    jcrez_script_tag_t *tags;
    size_t tag_count;
    char sha256[65];
} jcrez_ads_t;

esp_err_t jcrez_open(jcrez_archive_t *archive);
void jcrez_close(jcrez_archive_t *archive);
esp_err_t jcrez_load_palette(const jcrez_archive_t *archive, const char *name,
                             jcrez_palette_t *palette);
esp_err_t jcrez_load_screen(const jcrez_archive_t *archive, const char *name,
                            jcrez_screen_t *screen);
void jcrez_release_screen(jcrez_screen_t *screen);
esp_err_t jcrez_load_bitmap(const jcrez_archive_t *archive, const char *name,
                            jcrez_bitmap_t *bitmap);
void jcrez_release_bitmap(jcrez_bitmap_t *bitmap);
esp_err_t jcrez_load_ttm(const jcrez_archive_t *archive, const char *name,
                         jcrez_ttm_t *ttm);
void jcrez_release_ttm(jcrez_ttm_t *ttm);
esp_err_t jcrez_load_ads(const jcrez_archive_t *archive, const char *name,
                         jcrez_ads_t *ads);
void jcrez_release_ads(jcrez_ads_t *ads);

esp_err_t jcunpack(uint8_t method, const uint8_t *input, size_t input_size,
                   uint8_t *output, size_t output_size);
