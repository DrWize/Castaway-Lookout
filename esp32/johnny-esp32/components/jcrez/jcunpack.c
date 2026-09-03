#include "jcrez.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "esp_heap_caps.h"

typedef struct {
    const uint8_t *data;
    size_t size;
    size_t byte_offset;
    unsigned bit_offset;
} bit_reader_t;

typedef struct {
    uint16_t prefix;
    uint8_t append;
} code_entry_t;

static esp_err_t read_bits(bit_reader_t *reader, unsigned count, uint16_t *value)
{
    uint16_t result = 0;
    for (unsigned bit = 0; bit < count; ++bit) {
        uint8_t current = reader->byte_offset < reader->size ? reader->data[reader->byte_offset] : 0;
        if ((current & (1U << reader->bit_offset)) != 0) {
            result |= (uint16_t)(1U << bit);
        }
        ++reader->bit_offset;
        if (reader->bit_offset == 8) {
            reader->bit_offset = 0;
            ++reader->byte_offset;
        }
    }
    *value = result;
    return ESP_OK;
}

static esp_err_t unpack_rle(const uint8_t *input, size_t input_size,
                            uint8_t *output, size_t output_size)
{
    size_t in = 0;
    size_t out = 0;
    while (out < output_size) {
        if (in >= input_size) {
            return ESP_ERR_INVALID_SIZE;
        }
        uint8_t control = input[in++];
        size_t length = control & 0x7f;
        if (out + length > output_size) {
            return ESP_ERR_INVALID_SIZE;
        }
        if ((control & 0x80) != 0) {
            if (in >= input_size) {
                return ESP_ERR_INVALID_SIZE;
            }
            memset(output + out, input[in++], length);
        } else {
            if (in + length > input_size) {
                return ESP_ERR_INVALID_SIZE;
            }
            memcpy(output + out, input + in, length);
            in += length;
        }
        out += length;
    }
    return in == input_size ? ESP_OK : ESP_ERR_INVALID_SIZE;
}

static esp_err_t unpack_lzw(const uint8_t *input, size_t input_size,
                            uint8_t *output, size_t output_size)
{
    if (input_size == 0 || output_size == 0) {
        return ESP_ERR_INVALID_SIZE;
    }
    bit_reader_t reader = {.data = input, .size = input_size};
    code_entry_t *table = heap_caps_calloc(4096, sizeof(*table),
                                           MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    uint8_t *stack = heap_caps_malloc(4096, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (table == NULL || stack == NULL) {
        free(table);
        free(stack);
        return ESP_ERR_NO_MEM;
    }
    esp_err_t result = ESP_OK;
    unsigned width = 9;
    uint32_t free_entry = 257;
    uint32_t stream_bit_pos = 0;
    size_t out = 0;
    uint16_t old_code = 0;
    result = read_bits(&reader, width, &old_code);
    if (result != ESP_OK) {
        goto cleanup;
    }
    uint16_t last_byte = old_code;
    output[out++] = (uint8_t)old_code;

    while (reader.byte_offset < input_size && out < output_size) {
        uint16_t new_code = 0;
        result = read_bits(&reader, width, &new_code);
        if (result != ESP_OK) {
            goto cleanup;
        }
        stream_bit_pos += width;
        if (new_code == 256) {
            unsigned block_bits = width * 8;
            unsigned skip = block_bits - ((stream_bit_pos - 1) % block_bits) - 1;
            uint16_t ignored = 0;
            result = read_bits(&reader, skip, &ignored);
            if (result != ESP_OK) {
                goto cleanup;
            }
            width = 9;
            free_entry = 256;
            stream_bit_pos = 0;
            continue;
        }

        uint16_t code = new_code;
        size_t stack_size = 0;
        if (code >= free_entry) {
            if (code > free_entry) {
                result = ESP_ERR_INVALID_RESPONSE;
                goto cleanup;
            }
            stack[stack_size++] = (uint8_t)last_byte;
            code = old_code;
        }
        while (code > 255) {
            if (code >= 4096 || stack_size >= 4096) {
                result = ESP_ERR_INVALID_RESPONSE;
                goto cleanup;
            }
            stack[stack_size++] = table[code].append;
            code = table[code].prefix;
        }
        if (stack_size >= 4096) {
            result = ESP_ERR_INVALID_RESPONSE;
            goto cleanup;
        }
        stack[stack_size++] = (uint8_t)code;
        last_byte = code;
        while (stack_size > 0 && out < output_size) {
            output[out++] = stack[--stack_size];
        }

        if (free_entry < 4096) {
            table[free_entry].prefix = old_code;
            table[free_entry].append = (uint8_t)last_byte;
            ++free_entry;
            if (free_entry >= (1U << width) && width < 12) {
                ++width;
                stream_bit_pos = 0;
            }
        }
        old_code = new_code;
    }
    result = out == output_size ? ESP_OK : ESP_ERR_INVALID_SIZE;

cleanup:
    free(table);
    free(stack);
    return result;
}

esp_err_t jcunpack(uint8_t method, const uint8_t *input, size_t input_size,
                   uint8_t *output, size_t output_size)
{
    if (input == NULL || output == NULL || output_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (method == 1) {
        return unpack_rle(input, input_size, output, output_size);
    }
    if (method == 2) {
        return unpack_lzw(input, input_size, output, output_size);
    }
    return ESP_ERR_NOT_SUPPORTED;
}
