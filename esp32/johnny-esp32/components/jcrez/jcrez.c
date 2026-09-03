#include "jcrez.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "mbedtls/md5.h"
#include "mbedtls/sha256.h"

static const char *TAG = "jcrez";
static const uint8_t EXPECTED_MAP_MD5[16] = {
    0x37, 0x4e, 0x6d, 0x05, 0xc5, 0xe0, 0xac, 0xd8,
    0x8f, 0xb5, 0xaf, 0x74, 0x89, 0x48, 0xc8, 0x99,
};
static const uint8_t EXPECTED_ARCHIVE_MD5[16] = {
    0x8b, 0xb6, 0xc9, 0x9e, 0x91, 0x29, 0x80, 0x6b,
    0x50, 0x89, 0xa3, 0x9d, 0x24, 0x22, 0x8a, 0x36,
};

typedef struct __attribute__((packed)) {
    char magic[4];
    uint8_t version;
    uint8_t reserved[3];
    uint32_t map_len;
    uint32_t total_len;
    uint32_t crc32;
} jcdata_header_t;

typedef struct {
    const uint8_t *payload;
    size_t size;
    char name[14];
} resource_view_t;

static uint16_t le16(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static uint32_t le32(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static uint32_t crc32(const uint8_t *data, size_t size)
{
    uint32_t crc = 0xffffffffU;
    for (size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
        }
    }
    return crc ^ 0xffffffffU;
}

static bool same_name(const char *left, const char *right)
{
    while (*left != '\0' && *right != '\0') {
        if (toupper((unsigned char)*left++) != toupper((unsigned char)*right++)) {
            return false;
        }
    }
    return *left == *right;
}

static esp_err_t expect_tag(const uint8_t *data, size_t size, size_t *offset, const char tag[4])
{
    if (*offset + 4 > size || memcmp(data + *offset, tag, 4) != 0) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    *offset += 4;
    return ESP_OK;
}

static esp_err_t find_resource(const jcrez_archive_t *archive, const char *name,
                               resource_view_t *resource)
{
    ESP_RETURN_ON_FALSE(archive != NULL && name != NULL && resource != NULL,
                        ESP_ERR_INVALID_ARG, TAG, "invalid resource lookup");
    ESP_RETURN_ON_FALSE(archive->map_size >= 21, ESP_ERR_INVALID_SIZE, TAG, "map truncated");
    uint16_t count = le16(archive->map_data + 19);
    ESP_RETURN_ON_FALSE(21U + (size_t)count * 8U <= archive->map_size,
                        ESP_ERR_INVALID_SIZE, TAG, "map entries truncated");

    for (uint16_t index = 0; index < count; ++index) {
        uint32_t offset = le32(archive->map_data + 21U + (size_t)index * 8U + 4U);
        if ((size_t)offset + 17U > archive->archive_size) {
            continue;
        }
        char candidate[14] = {0};
        memcpy(candidate, archive->archive_data + offset, 13);
        for (int i = 12; i >= 0 && (candidate[i] == '\0' || candidate[i] == ' '); --i) {
            candidate[i] = '\0';
        }
        if (!same_name(candidate, name)) {
            continue;
        }
        uint32_t size = le32(archive->archive_data + offset + 13);
        ESP_RETURN_ON_FALSE((size_t)offset + 17U + size <= archive->archive_size,
                            ESP_ERR_INVALID_SIZE, TAG, "resource truncated");
        memcpy(resource->name, candidate, sizeof(resource->name));
        resource->payload = archive->archive_data + offset + 17;
        resource->size = size;
        return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
}

static void digest_hex(const uint8_t *digest, size_t size, char *output)
{
    static const char HEX[] = "0123456789abcdef";
    for (size_t index = 0; index < size; ++index) {
        output[index * 2] = HEX[digest[index] >> 4];
        output[index * 2 + 1] = HEX[digest[index] & 0x0f];
    }
    output[size * 2] = '\0';
}

static esp_err_t read_cstring(const uint8_t *data, size_t limit, size_t *offset,
                              char *output, size_t output_size)
{
    ESP_RETURN_ON_FALSE(data != NULL && offset != NULL && output != NULL && output_size > 0,
                        ESP_ERR_INVALID_ARG, TAG, "invalid string destination");
    size_t start = *offset;
    while (*offset < limit && data[*offset] != 0) {
        ++*offset;
    }
    ESP_RETURN_ON_FALSE(*offset < limit, ESP_ERR_INVALID_SIZE, TAG, "unterminated script string");
    size_t length = *offset - start;
    ESP_RETURN_ON_FALSE(length < output_size, ESP_ERR_INVALID_SIZE, TAG, "script string too long");
    memcpy(output, data + start, length);
    output[length] = '\0';
    ++*offset;
    return ESP_OK;
}

static esp_err_t scan_ttm_bookmarks(const uint8_t *data, size_t size,
                                    jcrez_ttm_bookmark_t *bookmarks, size_t capacity,
                                    size_t *count)
{
    size_t offset = 0;
    size_t found = 0;
    while (offset < size) {
        ESP_RETURN_ON_FALSE(offset + 2 <= size, ESP_ERR_INVALID_SIZE, TAG, "truncated TTM opcode");
        uint16_t opcode = le16(data + offset);
        offset += 2;
        uint8_t arg_count = opcode & 0x0f;
        if (arg_count == 0x0f) {
            char ignored[201] = {0};
            ESP_RETURN_ON_ERROR(read_cstring(data, size, &offset, ignored, sizeof(ignored)),
                                TAG, "TTM string");
            if (offset & 1U) {
                ++offset;
            }
            ESP_RETURN_ON_FALSE(offset <= size, ESP_ERR_INVALID_SIZE, TAG, "TTM string padding");
            continue;
        }
        size_t byte_count = (size_t)arg_count * 2U;
        ESP_RETURN_ON_FALSE(offset + byte_count <= size, ESP_ERR_INVALID_SIZE, TAG, "TTM arguments");
        if (opcode == 0x1101 || opcode == 0x1111) {
            if (bookmarks != NULL) {
                ESP_RETURN_ON_FALSE(found < capacity, ESP_ERR_INVALID_SIZE, TAG, "TTM bookmark overflow");
                bookmarks[found].id = le16(data + offset);
                bookmarks[found].offset = (uint32_t)(offset + 2U);
            }
            ++found;
        }
        offset += byte_count;
    }
    *count = found;
    return ESP_OK;
}

esp_err_t jcrez_open(jcrez_archive_t *archive)
{
    ESP_RETURN_ON_FALSE(archive != NULL, ESP_ERR_INVALID_ARG, TAG, "archive is null");
    memset(archive, 0, sizeof(*archive));
    archive->partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, 0x40, "jcdata");
    ESP_RETURN_ON_FALSE(archive->partition != NULL, ESP_ERR_NOT_FOUND, TAG, "jcdata partition missing");

    const void *mapped = NULL;
    ESP_RETURN_ON_ERROR(
        esp_partition_mmap(archive->partition, 0, archive->partition->size,
                           ESP_PARTITION_MMAP_DATA, &mapped, &archive->mapping),
        TAG, "map jcdata partition");
    const uint8_t *bytes = mapped;
    const jcdata_header_t *header = mapped;
    if (archive->partition->size < sizeof(*header) || memcmp(header->magic, "JCDT", 4) != 0 ||
        header->version != 1 || header->total_len > archive->partition->size ||
        header->total_len < sizeof(*header) ||
        header->map_len > header->total_len - sizeof(*header)) {
        jcrez_close(archive);
        return ESP_ERR_INVALID_RESPONSE;
    }
    const uint8_t *payload = bytes + sizeof(*header);
    size_t payload_size = header->total_len - sizeof(*header);
    if (crc32(payload, payload_size) != header->crc32) {
        jcrez_close(archive);
        return ESP_ERR_INVALID_CRC;
    }

    archive->map_data = payload;
    archive->map_size = header->map_len;
    archive->archive_data = payload + header->map_len;
    archive->archive_size = payload_size - header->map_len;

    uint8_t map_md5[16] = {0};
    uint8_t archive_md5[16] = {0};
    ESP_RETURN_ON_FALSE(mbedtls_md5(archive->map_data, archive->map_size, map_md5) == 0,
                        ESP_FAIL, TAG, "map MD5 failed");
    ESP_RETURN_ON_FALSE(mbedtls_md5(archive->archive_data, archive->archive_size, archive_md5) == 0,
                        ESP_FAIL, TAG, "archive MD5 failed");
    if (memcmp(map_md5, EXPECTED_MAP_MD5, 16) != 0 ||
        memcmp(archive_md5, EXPECTED_ARCHIVE_MD5, 16) != 0) {
        jcrez_close(archive);
        return ESP_ERR_INVALID_CRC;
    }
    ESP_LOGI(TAG, "jcdata valid total=%" PRIu32 " map=%u payload_crc32=%08" PRIx32
                  " source_md5=PASS",
             header->total_len, header->map_len, header->crc32);
    return ESP_OK;
}

void jcrez_close(jcrez_archive_t *archive)
{
    if (archive != NULL && archive->mapping != 0) {
        esp_partition_munmap(archive->mapping);
    }
    if (archive != NULL) {
        memset(archive, 0, sizeof(*archive));
    }
}

esp_err_t jcrez_load_palette(const jcrez_archive_t *archive, const char *name,
                             jcrez_palette_t *palette)
{
    ESP_RETURN_ON_FALSE(palette != NULL, ESP_ERR_INVALID_ARG, TAG, "palette is null");
    resource_view_t resource = {0};
    ESP_RETURN_ON_ERROR(find_resource(archive, name, &resource), TAG, "find palette");
    size_t offset = 0;
    ESP_RETURN_ON_ERROR(expect_tag(resource.payload, resource.size, &offset, "PAL:"), TAG, "PAL tag");
    offset += 4;
    ESP_RETURN_ON_ERROR(expect_tag(resource.payload, resource.size, &offset, "VGA:"), TAG, "VGA tag");
    offset += 4;
    ESP_RETURN_ON_FALSE(offset + 256U * 3U <= resource.size, ESP_ERR_INVALID_SIZE, TAG, "palette truncated");
    for (size_t index = 0; index < 16; ++index) {
        const uint8_t *color = resource.payload + offset + index * 3;
        palette->rgb[index][0] = (uint8_t)(color[0] << 2);
        palette->rgb[index][1] = (uint8_t)(color[1] << 2);
        palette->rgb[index][2] = (uint8_t)(color[2] << 2);
    }
    return ESP_OK;
}

esp_err_t jcrez_load_screen(const jcrez_archive_t *archive, const char *name,
                            jcrez_screen_t *screen)
{
    ESP_RETURN_ON_FALSE(screen != NULL, ESP_ERR_INVALID_ARG, TAG, "screen is null");
    memset(screen, 0, sizeof(*screen));
    resource_view_t resource = {0};
    ESP_RETURN_ON_ERROR(find_resource(archive, name, &resource), TAG, "find screen");
    size_t offset = 0;
    ESP_RETURN_ON_ERROR(expect_tag(resource.payload, resource.size, &offset, "SCR:"), TAG, "SCR tag");
    offset += 4;
    ESP_RETURN_ON_ERROR(expect_tag(resource.payload, resource.size, &offset, "DIM:"), TAG, "DIM tag");
    ESP_RETURN_ON_FALSE(offset + 8 <= resource.size, ESP_ERR_INVALID_SIZE, TAG, "DIM truncated");
    offset += 4;
    screen->width = le16(resource.payload + offset);
    screen->height = le16(resource.payload + offset + 2);
    offset += 4;
    ESP_RETURN_ON_ERROR(expect_tag(resource.payload, resource.size, &offset, "BIN:"), TAG, "BIN tag");
    ESP_RETURN_ON_FALSE(offset + 9 <= resource.size, ESP_ERR_INVALID_SIZE, TAG, "BIN truncated");
    uint32_t chunk_size = le32(resource.payload + offset);
    offset += 4;
    ESP_RETURN_ON_FALSE(chunk_size >= 5 && offset + chunk_size <= resource.size,
                        ESP_ERR_INVALID_SIZE, TAG, "BIN size invalid");
    uint8_t method = resource.payload[offset];
    uint32_t output_size = le32(resource.payload + offset + 1);
    offset += 5;
    ESP_RETURN_ON_FALSE(screen->width <= 640 && screen->height <= 480 &&
                        output_size >= (size_t)screen->width * screen->height / 2,
                        ESP_ERR_INVALID_SIZE, TAG, "screen dimensions invalid");
    screen->packed_pixels = heap_caps_malloc(output_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(screen->packed_pixels != NULL, ESP_ERR_NO_MEM, TAG, "allocate screen");
    screen->packed_size = output_size;
    esp_err_t err = jcunpack(method, resource.payload + offset, chunk_size - 5,
                             screen->packed_pixels, output_size);
    if (err != ESP_OK) {
        jcrez_release_screen(screen);
        return err;
    }
    uint8_t digest[32] = {0};
    if (mbedtls_sha256(screen->packed_pixels, screen->packed_size, digest, 0) != 0) {
        jcrez_release_screen(screen);
        return ESP_FAIL;
    }
    digest_hex(digest, sizeof(digest), screen->sha256);
    return ESP_OK;
}

void jcrez_release_screen(jcrez_screen_t *screen)
{
    if (screen != NULL) {
        free(screen->packed_pixels);
        memset(screen, 0, sizeof(*screen));
    }
}

esp_err_t jcrez_load_bitmap(const jcrez_archive_t *archive, const char *name,
                            jcrez_bitmap_t *bitmap)
{
    ESP_RETURN_ON_FALSE(bitmap != NULL, ESP_ERR_INVALID_ARG, TAG, "bitmap is null");
    memset(bitmap, 0, sizeof(*bitmap));
    resource_view_t resource = {0};
    esp_err_t err = find_resource(archive, name, &resource);
    if (err != ESP_OK) goto fail;
    size_t offset = 0;
    if ((err = expect_tag(resource.payload, resource.size, &offset, "BMP:")) != ESP_OK) goto fail;
    if (offset + 4 > resource.size) { err = ESP_ERR_INVALID_SIZE; goto fail; }
    offset += 4; /* Aggregate dimensions are not used by the sprite renderer. */
    if ((err = expect_tag(resource.payload, resource.size, &offset, "INF:")) != ESP_OK) goto fail;
    if (offset + 6 > resource.size) { err = ESP_ERR_INVALID_SIZE; goto fail; }
    uint32_t info_size = le32(resource.payload + offset);
    uint16_t sprite_count = le16(resource.payload + offset + 4);
    offset += 6;
    size_t info_end = offset - 2U + info_size;
    if (sprite_count == 0 || sprite_count > 256 ||
        info_size < 2U + (uint32_t)sprite_count * 4U || info_end > resource.size) {
        err = ESP_ERR_INVALID_SIZE;
        goto fail;
    }
    bitmap->sprites = heap_caps_calloc(sprite_count, sizeof(*bitmap->sprites),
                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (bitmap->sprites == NULL) { err = ESP_ERR_NO_MEM; goto fail; }
    bitmap->sprite_count = sprite_count;
    size_t packed_size = 0;
    for (size_t index = 0; index < sprite_count; ++index) {
        uint16_t width = le16(resource.payload + offset + index * 2U);
        uint16_t height = le16(resource.payload + offset + sprite_count * 2U + index * 2U);
        if (width == 0 || (width & 1U) != 0 || height == 0) {
            err = ESP_ERR_INVALID_SIZE;
            goto fail;
        }
        size_t sprite_size = (size_t)width * height / 2U;
        if (packed_size > SIZE_MAX - sprite_size) { err = ESP_ERR_INVALID_SIZE; goto fail; }
        bitmap->sprites[index] = (jcrez_bitmap_sprite_t){
            .width = width,
            .height = height,
            .packed_offset = packed_size,
        };
        packed_size += sprite_size;
    }
    offset = info_end;
    if ((err = expect_tag(resource.payload, resource.size, &offset, "BIN:")) != ESP_OK) goto fail;
    if (offset + 9 > resource.size) { err = ESP_ERR_INVALID_SIZE; goto fail; }
    uint32_t chunk_size = le32(resource.payload + offset);
    uint8_t method = resource.payload[offset + 4];
    uint32_t output_size = le32(resource.payload + offset + 5);
    offset += 9;
    if (chunk_size < 5 || output_size != packed_size ||
        offset + chunk_size - 5U > resource.size) {
        err = ESP_ERR_INVALID_SIZE;
        goto fail;
    }
    bitmap->packed_pixels = heap_caps_malloc(output_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (bitmap->packed_pixels == NULL) { err = ESP_ERR_NO_MEM; goto fail; }
    bitmap->packed_size = output_size;
    if ((err = jcunpack(method, resource.payload + offset, chunk_size - 5U,
                        bitmap->packed_pixels, output_size)) != ESP_OK) goto fail;
    memcpy(bitmap->name, resource.name, sizeof(bitmap->name));
    uint8_t digest[32] = {0};
    if (mbedtls_sha256(bitmap->packed_pixels, bitmap->packed_size, digest, 0) != 0) {
        err = ESP_FAIL;
        goto fail;
    }
    digest_hex(digest, sizeof(digest), bitmap->sha256);
    return ESP_OK;

fail:
    jcrez_release_bitmap(bitmap);
    return err;
}

void jcrez_release_bitmap(jcrez_bitmap_t *bitmap)
{
    if (bitmap != NULL) {
        free(bitmap->packed_pixels);
        free(bitmap->sprites);
        memset(bitmap, 0, sizeof(*bitmap));
    }
}

esp_err_t jcrez_load_ttm(const jcrez_archive_t *archive, const char *name,
                         jcrez_ttm_t *ttm)
{
    ESP_RETURN_ON_FALSE(ttm != NULL, ESP_ERR_INVALID_ARG, TAG, "TTM is null");
    memset(ttm, 0, sizeof(*ttm));
    resource_view_t resource = {0};
    ESP_RETURN_ON_ERROR(find_resource(archive, name, &resource), TAG, "find TTM");
    size_t offset = 0;
    esp_err_t err = expect_tag(resource.payload, resource.size, &offset, "VER:");
    if (err != ESP_OK) goto fail;
    if (offset + 4 > resource.size) { err = ESP_ERR_INVALID_SIZE; goto fail; }
    uint32_t version_size = le32(resource.payload + offset);
    offset += 4;
    if (version_size < 4 || offset + version_size > resource.size) { err = ESP_ERR_INVALID_SIZE; goto fail; }
    memcpy(ttm->version, resource.payload + offset, 4);
    ttm->version[4] = '\0';
    offset += version_size;
    if ((err = expect_tag(resource.payload, resource.size, &offset, "PAG:")) != ESP_OK) goto fail;
    if (offset + 6 > resource.size) { err = ESP_ERR_INVALID_SIZE; goto fail; }
    offset += 6;
    if ((err = expect_tag(resource.payload, resource.size, &offset, "TT3:")) != ESP_OK) goto fail;
    if (offset + 9 > resource.size) { err = ESP_ERR_INVALID_SIZE; goto fail; }
    uint32_t chunk_size = le32(resource.payload + offset);
    uint8_t method = resource.payload[offset + 4];
    uint32_t output_size = le32(resource.payload + offset + 5);
    offset += 9;
    if (chunk_size < 5 || output_size == 0 || offset + chunk_size - 5U > resource.size) {
        err = ESP_ERR_INVALID_SIZE;
        goto fail;
    }
    ttm->bytecode = heap_caps_malloc(output_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (ttm->bytecode == NULL) { err = ESP_ERR_NO_MEM; goto fail; }
    ttm->bytecode_size = output_size;
    if ((err = jcunpack(method, resource.payload + offset, chunk_size - 5U,
                        ttm->bytecode, output_size)) != ESP_OK) goto fail;
    offset += chunk_size - 5U;
    if ((err = expect_tag(resource.payload, resource.size, &offset, "TTI:")) != ESP_OK) goto fail;
    if (offset + 4 > resource.size) { err = ESP_ERR_INVALID_SIZE; goto fail; }
    offset += 4;
    if ((err = expect_tag(resource.payload, resource.size, &offset, "TAG:")) != ESP_OK) goto fail;
    if (offset + 6 > resource.size) { err = ESP_ERR_INVALID_SIZE; goto fail; }
    uint32_t tag_chunk_size = le32(resource.payload + offset);
    uint16_t tag_count = le16(resource.payload + offset + 4);
    offset += 6;
    if (tag_count == 0 || tag_count > 512 || tag_chunk_size < 2) { err = ESP_ERR_INVALID_SIZE; goto fail; }
    size_t tag_limit = offset + tag_chunk_size - 2U;
    if (tag_limit > resource.size) { tag_limit = resource.size; }
    ttm->tags = heap_caps_calloc(tag_count, sizeof(*ttm->tags),
                                 MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (ttm->tags == NULL) { err = ESP_ERR_NO_MEM; goto fail; }
    ttm->tag_count = tag_count;
    for (size_t index = 0; index < tag_count; ++index) {
        if (offset + 2 > tag_limit) { err = ESP_ERR_INVALID_SIZE; goto fail; }
        ttm->tags[index].id = le16(resource.payload + offset);
        offset += 2;
        if ((err = read_cstring(resource.payload, tag_limit, &offset,
                                ttm->tags[index].description,
                                sizeof(ttm->tags[index].description))) != ESP_OK) goto fail;
    }
    size_t bookmark_count = 0;
    if ((err = scan_ttm_bookmarks(ttm->bytecode, ttm->bytecode_size, NULL, 0,
                                  &bookmark_count)) != ESP_OK) goto fail;
    if (bookmark_count > 0) {
        ttm->bookmarks = heap_caps_calloc(bookmark_count, sizeof(*ttm->bookmarks),
                                          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (ttm->bookmarks == NULL) { err = ESP_ERR_NO_MEM; goto fail; }
        if ((err = scan_ttm_bookmarks(ttm->bytecode, ttm->bytecode_size,
                                      ttm->bookmarks, bookmark_count,
                                      &ttm->bookmark_count)) != ESP_OK) goto fail;
    }
    memcpy(ttm->name, resource.name, sizeof(ttm->name));
    uint8_t digest[32] = {0};
    if (mbedtls_sha256(ttm->bytecode, ttm->bytecode_size, digest, 0) != 0) {
        err = ESP_FAIL;
        goto fail;
    }
    digest_hex(digest, sizeof(digest), ttm->sha256);
    return ESP_OK;

fail:
    jcrez_release_ttm(ttm);
    return err;
}

void jcrez_release_ttm(jcrez_ttm_t *ttm)
{
    if (ttm != NULL) {
        free(ttm->bookmarks);
        free(ttm->tags);
        free(ttm->bytecode);
        memset(ttm, 0, sizeof(*ttm));
    }
}

esp_err_t jcrez_load_ads(const jcrez_archive_t *archive, const char *name,
                         jcrez_ads_t *ads)
{
    ESP_RETURN_ON_FALSE(ads != NULL, ESP_ERR_INVALID_ARG, TAG, "ADS is null");
    memset(ads, 0, sizeof(*ads));
    resource_view_t resource = {0};
    ESP_RETURN_ON_ERROR(find_resource(archive, name, &resource), TAG, "find ADS");
    size_t offset = 0;
    esp_err_t err = expect_tag(resource.payload, resource.size, &offset, "VER:");
    if (err != ESP_OK) goto fail;
    if (offset + 4 > resource.size) { err = ESP_ERR_INVALID_SIZE; goto fail; }
    uint32_t version_size = le32(resource.payload + offset);
    offset += 4;
    if (version_size < 4 || offset + version_size > resource.size) { err = ESP_ERR_INVALID_SIZE; goto fail; }
    memcpy(ads->version, resource.payload + offset, 4);
    ads->version[4] = '\0';
    offset += version_size;
    if ((err = expect_tag(resource.payload, resource.size, &offset, "ADS:")) != ESP_OK) goto fail;
    if (offset + 4 > resource.size) { err = ESP_ERR_INVALID_SIZE; goto fail; }
    offset += 4;
    if ((err = expect_tag(resource.payload, resource.size, &offset, "RES:")) != ESP_OK) goto fail;
    if (offset + 6 > resource.size) { err = ESP_ERR_INVALID_SIZE; goto fail; }
    uint16_t resource_count = le16(resource.payload + offset + 4);
    offset += 6;
    if (resource_count == 0 || resource_count > 100) { err = ESP_ERR_INVALID_SIZE; goto fail; }
    ads->resources = heap_caps_calloc(resource_count, sizeof(*ads->resources),
                                      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (ads->resources == NULL) { err = ESP_ERR_NO_MEM; goto fail; }
    ads->resource_count = resource_count;
    for (size_t index = 0; index < resource_count; ++index) {
        if (offset + 2 > resource.size) { err = ESP_ERR_INVALID_SIZE; goto fail; }
        ads->resources[index].slot = le16(resource.payload + offset);
        offset += 2;
        if ((err = read_cstring(resource.payload, resource.size, &offset,
                                ads->resources[index].name,
                                sizeof(ads->resources[index].name))) != ESP_OK) goto fail;
    }
    if ((err = expect_tag(resource.payload, resource.size, &offset, "SCR:")) != ESP_OK) goto fail;
    if (offset + 9 > resource.size) { err = ESP_ERR_INVALID_SIZE; goto fail; }
    uint32_t chunk_size = le32(resource.payload + offset);
    uint8_t method = resource.payload[offset + 4];
    uint32_t output_size = le32(resource.payload + offset + 5);
    offset += 9;
    if (chunk_size < 5 || output_size == 0 || offset + chunk_size - 5U > resource.size) {
        err = ESP_ERR_INVALID_SIZE;
        goto fail;
    }
    ads->bytecode = heap_caps_malloc(output_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (ads->bytecode == NULL) { err = ESP_ERR_NO_MEM; goto fail; }
    ads->bytecode_size = output_size;
    if ((err = jcunpack(method, resource.payload + offset, chunk_size - 5U,
                        ads->bytecode, output_size)) != ESP_OK) goto fail;
    offset += chunk_size - 5U;
    if ((err = expect_tag(resource.payload, resource.size, &offset, "TAG:")) != ESP_OK) goto fail;
    if (offset + 6 > resource.size) { err = ESP_ERR_INVALID_SIZE; goto fail; }
    uint32_t tag_chunk_size = le32(resource.payload + offset);
    uint16_t tag_count = le16(resource.payload + offset + 4);
    offset += 6;
    if (tag_count == 0 || tag_count > 100 || tag_chunk_size < 2) { err = ESP_ERR_INVALID_SIZE; goto fail; }
    size_t tag_limit = offset + tag_chunk_size - 2U;
    if (tag_limit > resource.size) { tag_limit = resource.size; }
    ads->tags = heap_caps_calloc(tag_count, sizeof(*ads->tags),
                                 MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (ads->tags == NULL) { err = ESP_ERR_NO_MEM; goto fail; }
    ads->tag_count = tag_count;
    for (size_t index = 0; index < tag_count; ++index) {
        if (offset + 2 > tag_limit) { err = ESP_ERR_INVALID_SIZE; goto fail; }
        ads->tags[index].id = le16(resource.payload + offset);
        offset += 2;
        if ((err = read_cstring(resource.payload, tag_limit, &offset,
                                ads->tags[index].description,
                                sizeof(ads->tags[index].description))) != ESP_OK) goto fail;
    }
    memcpy(ads->name, resource.name, sizeof(ads->name));
    uint8_t digest[32] = {0};
    if (mbedtls_sha256(ads->bytecode, ads->bytecode_size, digest, 0) != 0) {
        err = ESP_FAIL;
        goto fail;
    }
    digest_hex(digest, sizeof(digest), ads->sha256);
    return ESP_OK;

fail:
    jcrez_release_ads(ads);
    return err;
}

void jcrez_release_ads(jcrez_ads_t *ads)
{
    if (ads != NULL) {
        free(ads->tags);
        free(ads->bytecode);
        free(ads->resources);
        memset(ads, 0, sizeof(*ads));
    }
}
