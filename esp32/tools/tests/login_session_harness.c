/* Host dependencies for the real session handler; no firmware fault hooks. */
#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int esp_err_t;
typedef int nvs_handle_t;
typedef struct { int unused; } httpd_req_t;
typedef struct { char *valuestring; } cJSON;
typedef struct {
    size_t used_entries, free_entries, available_entries, total_entries, namespace_count;
} nvs_stats_t;
enum { ESP_OK = 0, STORAGE_ERROR = 0x1105, STATS_ERROR = 0x1101,
       HTTPD_400_BAD_REQUEST = 400, HTTPD_500_INTERNAL_SERVER_ERROR = 500,
       NVS_READWRITE = 1, HTTP_BODY_MAX = 384, SESSION_BYTES = 24 };
static const char *TAG = "jcnet";
static const char *scenario;
static int opens, writes, commits, closes, stats_calls, status, cookies;
static char response[160], logs[2048], supplied_password[80];
static cJSON password;

static void log_message(const char *tag, const char *format, ...)
{
    (void)tag;
    size_t used = strlen(logs);
    va_list args;
    va_start(args, format);
    vsnprintf(logs + used, sizeof(logs) - used, format, args);
    va_end(args);
}
#define ESP_LOGE log_message
#define ESP_LOGI log_message

static const char *esp_err_to_name(esp_err_t error)
{
    return error == STATS_ERROR ? "ESP_ERR_NVS_NOT_INITIALIZED"
                               : "ESP_ERR_NVS_NOT_ENOUGH_SPACE";
}
static esp_err_t nvs_get_stats(const char *partition, nvs_stats_t *stats)
{
    assert(partition == NULL);
    ++stats_calls;
    *stats = (nvs_stats_t){600, 30, 0, 630, 7};
    return strcmp(scenario, "stats") == 0 ? STATS_ERROR : ESP_OK;
}
static esp_err_t read_body(httpd_req_t *request, char *body, size_t capacity)
{
    (void)request;
    snprintf(body, capacity, "mock-json");
    return ESP_OK;
}
static cJSON *cJSON_Parse(const char *body) { (void)body; return &password; }
static cJSON *cJSON_GetObjectItem(cJSON *root, const char *key)
{
    assert(root == &password && strcmp(key, "password") == 0);
    return strcmp(scenario, "missing") == 0 ? NULL : &password;
}
static bool cJSON_IsString(cJSON *item) { return item != NULL; }
static void cJSON_Delete(cJSON *root) { assert(root == &password); }
static esp_err_t load_blob(const char *key, void *value, size_t size)
{
    assert(strcmp(key, "salt") == 0 || strcmp(key, "verifier") == 0);
    memset(value, 0x42, size);
    return ESP_OK;
}
static esp_err_t derive_password(const char *text, const uint8_t salt[16], uint8_t out[32])
{
    assert(salt[0] == 0x42);
    memset(out, strcmp(text, "correct-secret") == 0 ? 0x42 : 0x43, 32);
    return ESP_OK;
}
static bool constant_equal(const uint8_t *a, const uint8_t *b, size_t size)
{
    return memcmp(a, b, size) == 0;
}
static esp_err_t unauthorized(httpd_req_t *request)
{
    (void)request;
    status = 401;
    return ESP_OK;
}
static void esp_fill_random(void *data, size_t size) { memset(data, 0xaa, size); }
static void hex_encode(const uint8_t *data, size_t size, char *hex)
{
    (void)data;
    memset(hex, 'a', size * 2);
    hex[size * 2] = '\0';
}
static void mbedtls_sha256(const unsigned char *data, size_t size, uint8_t out[32], int mode)
{
    (void)data; (void)size; (void)mode;
    memset(out, 0xbb, 32);
}
static esp_err_t nvs_open(const char *name, int mode, nvs_handle_t *handle)
{
    assert(strcmp(name, "jc_web") == 0 && mode == NVS_READWRITE);
    ++opens;
    if (strcmp(scenario, "open") == 0) return STORAGE_ERROR;
    *handle = 7;
    return ESP_OK;
}
static esp_err_t nvs_set_blob(nvs_handle_t handle, const char *key, const void *data, size_t size)
{
    assert(handle == 7 && strcmp(key, "session") == 0 && size == 32);
    assert(((const uint8_t *)data)[0] == 0xbb);
    ++writes;
    return strcmp(scenario, "write") == 0 || strcmp(scenario, "stats") == 0
               ? STORAGE_ERROR : ESP_OK;
}
static esp_err_t nvs_commit(nvs_handle_t handle)
{
    assert(handle == 7);
    ++commits;
    return strcmp(scenario, "commit") == 0 ? STORAGE_ERROR : ESP_OK;
}
static void nvs_close(nvs_handle_t handle) { assert(handle == 7); ++closes; }
static esp_err_t httpd_resp_send_err(httpd_req_t *request, int code, const char *text)
{
    (void)request;
    status = code;
    snprintf(response, sizeof(response), "%s", text);
    return ESP_OK;
}
static void httpd_resp_set_hdr(httpd_req_t *request, const char *name, const char *value)
{
    (void)request;
    assert(strcmp(name, "Set-Cookie") == 0);
    assert(commits == 1 && closes == 1);
    assert(strstr(value, "HttpOnly; SameSite=Strict; Max-Age=2592000"));
    ++cookies;
}
static void httpd_resp_set_type(httpd_req_t *request, const char *type)
{
    (void)request;
    assert(strcmp(type, "application/json") == 0);
}
static esp_err_t httpd_resp_sendstr(httpd_req_t *request, const char *text)
{
    (void)request;
    status = 200;
    snprintf(response, sizeof(response), "%s", text);
    return ESP_OK;
}

#include "session_under_test.c"

int main(int argc, char **argv)
{
    assert(argc == 2);
    scenario = argv[1];
    strcpy(supplied_password, strcmp(scenario, "wrong") == 0 ? "wrong-secret" : "correct-secret");
    password.valuestring = supplied_password;
    httpd_req_t request = {0};
    assert(session_handler(&request) == ESP_OK);
    assert(!strstr(logs, "correct-secret") && !strstr(logs, "wrong-secret"));
    assert(!strstr(logs, "aaaaaaaa") && !strstr(response, "aaaaaaaa"));
    if (strcmp(scenario, "wrong") == 0 || strcmp(scenario, "missing") == 0) {
        assert(status == 401 && opens == 0 && cookies == 0 && stats_calls == 0);
    } else if (strcmp(scenario, "success") == 0) {
        assert(status == 200 && opens == 1 && writes == 1 && commits == 1 && closes == 1);
        assert(cookies == 1 && stats_calls == 0);
        assert(strcmp(response, "{\"authenticated\":true}") == 0);
    } else {
        const char *ref = strcmp(scenario, "open") == 0 ? "LOGIN_OPEN"
                        : strcmp(scenario, "commit") == 0 ? "LOGIN_COMMIT" : "LOGIN_WRITE";
        assert(status == 500 && cookies == 0 && stats_calls == 1 && opens == 1);
        assert(writes == (strcmp(scenario, "open") != 0));
        assert(commits == (strcmp(scenario, "commit") == 0));
        assert(closes == (strcmp(scenario, "open") != 0));
        assert(strstr(response, "Could not save your login session") && strstr(response, ref));
        assert(strstr(logs, ref) && strstr(logs, "ESP_ERR_NVS_NOT_ENOUGH_SPACE"));
        assert(strstr(logs, strcmp(scenario, "stats") == 0
                           ? "statistics unavailable" : "available=0 total=630"));
    }
    return 0;
}
