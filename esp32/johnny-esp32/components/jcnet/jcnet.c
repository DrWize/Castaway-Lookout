#include "jcnet.h"

#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cJSON.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "jccontrol.h"
#include "jcengine.h"
#include "mbedtls/pkcs5.h"
#include "mbedtls/sha256.h"
#include "mdns.h"
#include "nvs.h"

static const char *TAG = "jcnet";
enum {
    AUTH_SCHEMA = 1,
    PBKDF2_ITERATIONS = 60000,
    SESSION_BYTES = 24,
    HTTP_BODY_MAX = 384,
};

static const char CONTROL_PAGE[] =
"<!doctype html><meta name=viewport content='width=device-width,initial-scale=1'>"
"<title>Johnny Castaway</title><style>body{font:16px system-ui;background:#07131d;color:#eee;max-width:900px;margin:auto;padding:18px}h1{color:#ffd54a}.card{background:#102635;padding:16px;border-radius:12px;margin:12px 0}button,select,input{font:inherit;padding:10px;margin:4px;border-radius:8px;border:1px solid #567}button{background:#e9a928;color:#111}.scenes{display:grid;grid-template-columns:repeat(auto-fit,minmax(240px,1fr));gap:6px}.scene{background:#18394b;color:#fff;text-align:left}small{color:#9bc}#msg{min-height:1.4em}</style>"
"<h1>Johnny Castaway</h1><div id=login class=card><h2>Login</h2><input id=p type=password placeholder='Administrator password'><button onclick=login()>Login</button></div>"
"<main id=app hidden><div class=card><b id=current>Loading...</b><p id=net></p><button onclick=randomPlay()>Random now</button></div>"
"<div class=card><label>Sky <select id=sky onchange=settings()><option value=automatic>Automatic</option><option value=day>Day</option><option value=night>Night</option></select></label>"
"<label>Holiday <select id=holiday onchange=settings()><option value=off>Off</option><option value=automatic>Automatic</option><option value=halloween>Halloween</option><option value=st_patrick>St. Patrick</option><option value=christmas>Christmas</option><option value=new_year>New Year</option></select></label></div>"
"<div class=card><h2>Scenes</h2><div id=scenes class=scenes></div></div><p id=msg></p></main>"
"<script>const j=(u,o={})=>fetch(u,{headers:{'Content-Type':'application/json'},...o}).then(async r=>{if(!r.ok)throw Error(await r.text());return r.status==204?{}:r.json()});"
"async function login(){try{await j('/api/v1/session',{method:'POST',body:JSON.stringify({password:p.value})});login.hidden=true;app.hidden=false;await load()}catch(e){msg.textContent=e}}"
"async function load(){let [s,c]=await Promise.all([j('/api/v1/status'),j('/api/v1/scenes')]);show(s);scenes.innerHTML=c.scenes.map(x=>`<button class=scene onclick=play('${x.id}')><small>${x.id} - ${x.category}</small><br>${x.title}</button>`).join('')}"
"function show(s){current.textContent=`${s.current_scene.id}: ${s.current_scene.title} - frame ${s.frame}`;net.textContent=`${s.hostname} | ${s.ip||'offline'} | ${s.shuffle_remaining} left in shuffle`;sky.value=s.sky;holiday.value=s.holiday}"
"async function settings(){try{show(await j('/api/v1/settings',{method:'PUT',body:JSON.stringify({sky:sky.value,holiday:holiday.value})}));msg.textContent='Applied'}catch(e){msg.textContent=e}}"
"async function play(id){try{show(await j('/api/v1/playback/scene',{method:'POST',body:JSON.stringify({scene_id:id})}));msg.textContent='Scene started'}catch(e){msg.textContent=e}}"
"async function randomPlay(){try{show(await j('/api/v1/playback/random',{method:'POST',body:'{}'}));msg.textContent='Random playback resumed'}catch(e){msg.textContent=e}}"
"setInterval(()=>{if(!app.hidden)j('/api/v1/status').then(show)},3000)</script>";

static const char SETUP_PAGE[] =
"<!doctype html><meta name=viewport content='width=device-width,initial-scale=1'><title>Johnny setup</title>"
"<style>body{font:18px system-ui;max-width:520px;margin:auto;padding:24px;background:#07131d;color:#fff}input,button{box-sizing:border-box;width:100%;padding:12px;margin:7px 0;font:inherit}button{background:#e9a928;border:0}</style>"
"<h1>Johnny Castaway setup</h1><p>Enter a 2.4 GHz Wi-Fi network and create the administrator password.</p>"
"<input id=s placeholder='Wi-Fi name (SSID)' maxlength=32><input id=w type=password placeholder='Wi-Fi password' maxlength=63>"
"<input id=a type=password placeholder='Administrator password (8+ characters)' maxlength=64><button onclick=save()>Connect</button><p id=m></p>"
"<script>async function save(){m.textContent='Saving...';let r=await fetch('/api/v1/setup',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid:s.value,wifi_password:w.value,admin_password:a.value})});m.textContent=await r.text()}</script>";

static SemaphoreHandle_t status_lock;
static jcnet_status_t network_status;
static httpd_handle_t server;
static int reconnects;
static bool fallback_ap_active;

static void hex_encode(const uint8_t *input, size_t length, char *output)
{
    static const char digits[] = "0123456789abcdef";
    for (size_t index = 0; index < length; ++index) {
        output[index * 2] = digits[input[index] >> 4];
        output[index * 2 + 1] = digits[input[index] & 15];
    }
    output[length * 2] = '\0';
}

static bool constant_equal(const uint8_t *left, const uint8_t *right,
                           size_t length)
{
    uint8_t difference = 0;
    for (size_t index = 0; index < length; ++index) {
        difference |= left[index] ^ right[index];
    }
    return difference == 0;
}

static esp_err_t derive_password(const char *password, const uint8_t salt[16],
                                 uint8_t output[32])
{
    int result = mbedtls_pkcs5_pbkdf2_hmac_ext(
        MBEDTLS_MD_SHA256, (const unsigned char *)password, strlen(password),
        salt, 16, PBKDF2_ITERATIONS, 32, output);
    return result == 0 ? ESP_OK : ESP_FAIL;
}

static esp_err_t read_body(httpd_req_t *request, char *body, size_t capacity)
{
    if (request->content_len <= 0 || request->content_len >= capacity) {
        return ESP_ERR_INVALID_SIZE;
    }
    size_t received = 0;
    while (received < request->content_len) {
        int count = httpd_req_recv(request, body + received,
                                   request->content_len - received);
        if (count <= 0) return ESP_FAIL;
        received += (size_t)count;
    }
    body[received] = '\0';
    return ESP_OK;
}

static esp_err_t load_blob(const char *key, void *value, size_t length)
{
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open("jc_web", NVS_READONLY, &handle);
    if (err != ESP_OK) return err;
    size_t actual = length;
    err = nvs_get_blob(handle, key, value, &actual);
    nvs_close(handle);
    return err == ESP_OK && actual == length ? ESP_OK : ESP_ERR_NOT_FOUND;
}

static bool request_authenticated(httpd_req_t *request)
{
    size_t length = httpd_req_get_hdr_value_len(request, "Cookie");
    if (length == 0 || length > 160) return false;
    char cookie[161] = {0};
    if (httpd_req_get_hdr_value_str(request, "Cookie", cookie,
                                    sizeof(cookie)) != ESP_OK) return false;
    char *token = strstr(cookie, "jc_session=");
    if (token == NULL) return false;
    token += strlen("jc_session=");
    char hex[SESSION_BYTES * 2 + 1] = {0};
    size_t index = 0;
    while (index < sizeof(hex) - 1 && token[index] != '\0' &&
           token[index] != ';') {
        hex[index] = token[index];
        ++index;
    }
    if (index != SESSION_BYTES * 2) return false;
    uint8_t digest[32] = {0}, saved[32] = {0};
    mbedtls_sha256((const unsigned char *)hex, strlen(hex), digest, 0);
    return load_blob("session", saved, sizeof(saved)) == ESP_OK &&
           constant_equal(digest, saved, sizeof(saved));
}

static esp_err_t unauthorized(httpd_req_t *request)
{
    httpd_resp_set_status(request, "401 Unauthorized");
    return httpd_resp_sendstr(request, "authentication required");
}

static esp_err_t root_handler(httpd_req_t *request)
{
    jcnet_status_t status = {0};
    jcnet_status(&status);
    httpd_resp_set_type(request, "text/html");
    return httpd_resp_sendstr(request,
                              status.provisioned ? CONTROL_PAGE : SETUP_PAGE);
}

static void restart_task(void *context)
{
    (void)context;
    vTaskDelay(pdMS_TO_TICKS(750));
    esp_restart();
}

static esp_err_t setup_handler(httpd_req_t *request)
{
    jcnet_status_t status = {0};
    jcnet_status(&status);
    if (status.provisioned) return unauthorized(request);
    char body[HTTP_BODY_MAX] = {0};
    if (read_body(request, body, sizeof(body)) != ESP_OK) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "bad body");
    }
    cJSON *root = cJSON_Parse(body);
    cJSON *ssid = root == NULL ? NULL : cJSON_GetObjectItem(root, "ssid");
    cJSON *wifi = root == NULL ? NULL : cJSON_GetObjectItem(root, "wifi_password");
    cJSON *admin = root == NULL ? NULL : cJSON_GetObjectItem(root, "admin_password");
    if (!cJSON_IsString(ssid) || !cJSON_IsString(wifi) ||
        !cJSON_IsString(admin) || strlen(ssid->valuestring) == 0 ||
        strlen(ssid->valuestring) > 32 || strlen(wifi->valuestring) > 63 ||
        strlen(admin->valuestring) < 8 || strlen(admin->valuestring) > 64) {
        cJSON_Delete(root);
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                                   "invalid setup values");
    }
    uint8_t salt[16], verifier[32];
    esp_fill_random(salt, sizeof(salt));
    esp_err_t err = derive_password(admin->valuestring, salt, verifier);
    nvs_handle_t handle = 0;
    if (err == ESP_OK) err = nvs_open("jc_web", NVS_READWRITE, &handle);
    if (err == ESP_OK) err = nvs_set_u8(handle, "schema", AUTH_SCHEMA);
    if (err == ESP_OK) err = nvs_set_str(handle, "ssid", ssid->valuestring);
    if (err == ESP_OK) err = nvs_set_str(handle, "wifi", wifi->valuestring);
    if (err == ESP_OK) err = nvs_set_blob(handle, "salt", salt, sizeof(salt));
    if (err == ESP_OK) err = nvs_set_blob(handle, "verifier", verifier,
                                           sizeof(verifier));
    if (err == ESP_OK) err = nvs_commit(handle);
    if (handle != 0) nvs_close(handle);
    memset(wifi->valuestring, 0, strlen(wifi->valuestring));
    memset(admin->valuestring, 0, strlen(admin->valuestring));
    cJSON_Delete(root);
    memset(verifier, 0, sizeof(verifier));
    if (err != ESP_OK) {
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "could not save setup");
    }
    httpd_resp_sendstr(request, "Saved. Johnny is restarting...");
    xTaskCreate(restart_task, "jc_restart", 2048, NULL, 3, NULL);
    return ESP_OK;
}

static esp_err_t session_handler(httpd_req_t *request)
{
    char body[HTTP_BODY_MAX] = {0};
    if (read_body(request, body, sizeof(body)) != ESP_OK) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "bad body");
    }
    cJSON *root = cJSON_Parse(body);
    cJSON *password = root == NULL ? NULL : cJSON_GetObjectItem(root, "password");
    uint8_t salt[16], expected[32], actual[32];
    bool valid = cJSON_IsString(password) && strlen(password->valuestring) <= 64 &&
                 load_blob("salt", salt, sizeof(salt)) == ESP_OK &&
                 load_blob("verifier", expected, sizeof(expected)) == ESP_OK &&
                 derive_password(password->valuestring, salt, actual) == ESP_OK &&
                 constant_equal(expected, actual, sizeof(actual));
    if (cJSON_IsString(password)) {
        memset(password->valuestring, 0, strlen(password->valuestring));
    }
    cJSON_Delete(root);
    if (!valid) return unauthorized(request);
    uint8_t token[SESSION_BYTES], digest[32];
    char hex[SESSION_BYTES * 2 + 1];
    esp_fill_random(token, sizeof(token));
    hex_encode(token, sizeof(token), hex);
    mbedtls_sha256((const unsigned char *)hex, strlen(hex), digest, 0);
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open("jc_web", NVS_READWRITE, &handle);
    if (err == ESP_OK) err = nvs_set_blob(handle, "session", digest,
                                           sizeof(digest));
    if (err == ESP_OK) err = nvs_commit(handle);
    if (handle != 0) nvs_close(handle);
    if (err != ESP_OK) return httpd_resp_send_500(request);
    char cookie[128];
    snprintf(cookie, sizeof(cookie),
             "jc_session=%s; Path=/; HttpOnly; SameSite=Strict; Max-Age=2592000",
             hex);
    httpd_resp_set_hdr(request, "Set-Cookie", cookie);
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_sendstr(request, "{\"authenticated\":true}");
}

static esp_err_t logout_handler(httpd_req_t *request)
{
    if (!request_authenticated(request)) return unauthorized(request);
    nvs_handle_t handle = 0;
    if (nvs_open("jc_web", NVS_READWRITE, &handle) == ESP_OK) {
        nvs_erase_key(handle, "session");
        nvs_commit(handle);
        nvs_close(handle);
    }
    httpd_resp_set_hdr(request, "Set-Cookie",
                       "jc_session=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0");
    return httpd_resp_sendstr(request, "{}");
}

static void send_json_string(httpd_req_t *request, const char *value)
{
    httpd_resp_sendstr_chunk(request, "\"");
    for (const char *at = value; at != NULL && *at != '\0'; ++at) {
        char out[3] = {*at, 0, 0};
        if (*at == '\"' || *at == '\\') {
            out[0] = '\\';
            out[1] = *at;
        }
        httpd_resp_sendstr_chunk(request, out);
    }
    httpd_resp_sendstr_chunk(request, "\"");
}

static esp_err_t status_response(httpd_req_t *request)
{
    jccontrol_snapshot_t playback = {0};
    jcnet_status_t net = {0};
    jccontrol_snapshot(&playback);
    jcnet_status(&net);
    const jcengine_story_scene_t *scene =
        jcengine_story_scene(playback.scene_index);
    const jcengine_scene_menu_entry_t *menu =
        jcengine_scene_menu_entry(playback.scene_index);
    char json[768];
    snprintf(json, sizeof(json),
             "{\"hostname\":\"%s\",\"ip\":\"%s\",\"rssi\":%d,"
             "\"connected\":%s,\"time_synced\":%s,\"uptime_seconds\":%u,"
             "\"frame\":%u,\"shuffle_remaining\":%u,\"paused\":%s,"
             "\"sky\":\"%s\",\"holiday\":\"%s\",\"effective_night\":%s,"
             "\"effective_holiday\":%u,\"current_scene\":{\"id\":\"SCENE_%02u\","
             "\"title\":\"%s\",\"category\":\"%s\",\"ads\":\"%s\",\"tag\":%u}}",
             net.hostname, net.ip, net.rssi, net.connected ? "true" : "false",
             net.time_synced ? "true" : "false", (unsigned)(esp_timer_get_time() / 1000000),
             (unsigned)playback.frame, (unsigned)playback.shuffle_remaining,
             playback.paused ? "true" : "false",
             jccontrol_sky_name(playback.settings.sky),
             jccontrol_holiday_name(playback.settings.holiday),
             playback.effective_night ? "true" : "false",
             playback.effective_holiday, (unsigned)(playback.scene_index + 1),
             menu == NULL ? "Unknown" : menu->title,
             menu == NULL ? "Unknown" : menu->category,
             scene == NULL ? "UNKNOWN.ADS" : scene->ads_name,
             scene == NULL ? 0U : (unsigned)scene->ads_tag);
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_sendstr(request, json);
}

static esp_err_t status_handler(httpd_req_t *request)
{
    return request_authenticated(request) ? status_response(request)
                                          : unauthorized(request);
}

static esp_err_t scenes_handler(httpd_req_t *request)
{
    if (!request_authenticated(request)) return unauthorized(request);
    httpd_resp_set_type(request, "application/json");
    httpd_resp_sendstr_chunk(request, "{\"scenes\":[");
    for (size_t index = 0; index < jcengine_story_scene_count(); ++index) {
        const jcengine_story_scene_t *scene = jcengine_story_scene(index);
        const jcengine_scene_menu_entry_t *menu = jcengine_scene_menu_entry(index);
        char prefix[128];
        snprintf(prefix, sizeof(prefix),
                 "%s{\"id\":\"SCENE_%02u\",\"category\":",
                 index == 0 ? "" : ",", (unsigned)(index + 1));
        httpd_resp_sendstr_chunk(request, prefix);
        send_json_string(request, menu == NULL ? "Unknown" : menu->category);
        httpd_resp_sendstr_chunk(request, ",\"title\":");
        send_json_string(request, menu == NULL ? "Unknown" : menu->title);
        snprintf(prefix, sizeof(prefix), ",\"ads\":\"%s\",\"tag\":%u}",
                 scene->ads_name, (unsigned)scene->ads_tag);
        httpd_resp_sendstr_chunk(request, prefix);
    }
    httpd_resp_sendstr_chunk(request, "]}");
    return httpd_resp_sendstr_chunk(request, NULL);
}

static esp_err_t command_result(httpd_req_t *request, esp_err_t err)
{
    if (err == ESP_OK) return status_response(request);
    if (err == ESP_ERR_NO_MEM) {
        httpd_resp_set_status(request, "503 Service Unavailable");
        return httpd_resp_sendstr(request, "runtime command queue full");
    }
    if (err == ESP_ERR_TIMEOUT) {
        httpd_resp_set_status(request, "504 Gateway Timeout");
        return httpd_resp_sendstr(request, "runtime command timed out");
    }
    return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                               "runtime rejected command");
}

static esp_err_t settings_handler(httpd_req_t *request)
{
    if (!request_authenticated(request)) return unauthorized(request);
    char body[HTTP_BODY_MAX] = {0};
    if (read_body(request, body, sizeof(body)) != ESP_OK) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "bad body");
    }
    cJSON *root = cJSON_Parse(body);
    jccontrol_command_t command = {.type = JCCONTROL_COMMAND_SETTINGS};
    cJSON *sky = root == NULL ? NULL : cJSON_GetObjectItem(root, "sky");
    cJSON *holiday = root == NULL ? NULL : cJSON_GetObjectItem(root, "holiday");
    if (cJSON_IsString(sky)) {
        command.has_sky = jccontrol_parse_sky(sky->valuestring,
                                              &command.settings.sky);
        if (!command.has_sky) goto invalid;
    }
    if (cJSON_IsString(holiday)) {
        command.has_holiday = jccontrol_parse_holiday(
            holiday->valuestring, &command.settings.holiday);
        if (!command.has_holiday) goto invalid;
    }
    if (!command.has_sky && !command.has_holiday) goto invalid;
    cJSON_Delete(root);
    return command_result(request, jccontrol_submit(&command, 2000));
invalid:
    cJSON_Delete(root);
    return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                               "invalid sky or holiday");
}

static esp_err_t scene_handler(httpd_req_t *request)
{
    if (!request_authenticated(request)) return unauthorized(request);
    char body[HTTP_BODY_MAX] = {0};
    if (read_body(request, body, sizeof(body)) != ESP_OK) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "bad body");
    }
    cJSON *root = cJSON_Parse(body);
    cJSON *id = root == NULL ? NULL : cJSON_GetObjectItem(root, "scene_id");
    unsigned number = 0;
    bool valid = cJSON_IsString(id) &&
                 sscanf(id->valuestring, "SCENE_%u", &number) == 1 &&
                 number >= 1 && number <= JCENGINE_STORY_SCENE_COUNT;
    cJSON_Delete(root);
    if (!valid) return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                                            "invalid scene_id");
    jccontrol_command_t command = {.type = JCCONTROL_COMMAND_SCENE,
                                   .scene_index = number - 1};
    return command_result(request, jccontrol_submit(&command, 2000));
}

static esp_err_t random_handler(httpd_req_t *request)
{
    if (!request_authenticated(request)) return unauthorized(request);
    jccontrol_command_t command = {.type = JCCONTROL_COMMAND_RANDOM};
    return command_result(request, jccontrol_submit(&command, 2000));
}

static esp_err_t start_server(void)
{
    if (server != NULL) return ESP_OK;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 10;
    config.stack_size = 8192;
    esp_err_t err = httpd_start(&server, &config);
    if (err != ESP_OK) return err;
    const httpd_uri_t routes[] = {
        {.uri = "/", .method = HTTP_GET, .handler = root_handler},
        {.uri = "/api/v1/setup", .method = HTTP_POST, .handler = setup_handler},
        {.uri = "/api/v1/session", .method = HTTP_POST, .handler = session_handler},
        {.uri = "/api/v1/session", .method = HTTP_DELETE, .handler = logout_handler},
        {.uri = "/api/v1/status", .method = HTTP_GET, .handler = status_handler},
        {.uri = "/api/v1/scenes", .method = HTTP_GET, .handler = scenes_handler},
        {.uri = "/api/v1/settings", .method = HTTP_PUT, .handler = settings_handler},
        {.uri = "/api/v1/playback/scene", .method = HTTP_POST, .handler = scene_handler},
        {.uri = "/api/v1/playback/random", .method = HTTP_POST, .handler = random_handler},
    };
    for (size_t index = 0; index < sizeof(routes) / sizeof(routes[0]); ++index) {
        err = httpd_register_uri_handler(server, &routes[index]);
        if (err != ESP_OK) return err;
    }
    return ESP_OK;
}

static void start_fallback_ap(void)
{
    if (fallback_ap_active) return;
    if (esp_netif_create_default_wifi_ap() == NULL) return;
    wifi_config_t config = {0};
    memcpy(config.ap.ssid, network_status.ap_ssid,
           strlen(network_status.ap_ssid));
    config.ap.ssid_len = strlen(network_status.ap_ssid);
    memcpy(config.ap.password, network_status.setup_password,
           strlen(network_status.setup_password));
    config.ap.max_connection = 2;
    config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    if (esp_wifi_set_mode(WIFI_MODE_APSTA) != ESP_OK ||
        esp_wifi_set_config(WIFI_IF_AP, &config) != ESP_OK) return;
    fallback_ap_active = true;
    xSemaphoreTake(status_lock, portMAX_DELAY);
    network_status.provisioned = false;
    memcpy(network_status.ip, "192.168.4.1", sizeof("192.168.4.1"));
    xSemaphoreGive(status_lock);
    ESP_LOGW(TAG,
             "SETUP: station unavailable; fallback ssid=%s password=%s url=http://192.168.4.1",
             network_status.ap_ssid, network_status.setup_password);
}

static void network_event(void *context, esp_event_base_t base, int32_t id,
                          void *data)
{
    (void)context;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xSemaphoreTake(status_lock, portMAX_DELAY);
        network_status.connected = false;
        network_status.ip[0] = '\0';
        xSemaphoreGive(status_lock);
        ++reconnects;
        if (reconnects >= 5) start_fallback_ap();
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = data;
        xSemaphoreTake(status_lock, portMAX_DELAY);
        network_status.connected = true;
        snprintf(network_status.ip, sizeof(network_status.ip), IPSTR,
                 IP2STR(&event->ip_info.ip));
        xSemaphoreGive(status_lock);
        reconnects = 0;
        wifi_ap_record_t access_point = {0};
        if (esp_wifi_sta_get_ap_info(&access_point) == ESP_OK) {
            xSemaphoreTake(status_lock, portMAX_DELAY);
            network_status.rssi = access_point.rssi;
            xSemaphoreGive(status_lock);
        }
        ESP_LOGI(TAG, "WEB: http://%s.local ip=" IPSTR,
                 network_status.hostname, IP2STR(&event->ip_info.ip));
        esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
        if (esp_netif_sntp_init(&config) == ESP_OK) {
            ESP_LOGI(TAG, "SNTP: synchronization started timezone=Europe/Stockholm");
        }
    }
}

static void time_monitor(void *context)
{
    (void)context;
    while (true) {
        time_t now = time(NULL);
        bool valid = now > 1704067200;
        xSemaphoreTake(status_lock, portMAX_DELAY);
        network_status.time_synced = valid;
        xSemaphoreGive(status_lock);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

static bool load_wifi(char ssid[33], char password[64])
{
    nvs_handle_t handle = 0;
    uint8_t schema = 0;
    if (nvs_open("jc_web", NVS_READONLY, &handle) != ESP_OK) return false;
    size_t ssid_size = 33, password_size = 64;
    bool valid = nvs_get_u8(handle, "schema", &schema) == ESP_OK &&
                 schema == AUTH_SCHEMA &&
                 nvs_get_str(handle, "ssid", ssid, &ssid_size) == ESP_OK &&
                 nvs_get_str(handle, "wifi", password, &password_size) == ESP_OK;
    nvs_close(handle);
    return valid;
}

esp_err_t jcnet_start(void)
{
    status_lock = xSemaphoreCreateMutex();
    if (status_lock == NULL) return ESP_ERR_NO_MEM;
    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_STA));
    snprintf(network_status.ap_ssid, sizeof(network_status.ap_ssid),
             "Johnny-%02X%02X", mac[4], mac[5]);
    snprintf(network_status.hostname, sizeof(network_status.hostname),
             "johnny-%02x%02x", mac[4], mac[5]);
    snprintf(network_status.setup_password,
             sizeof(network_status.setup_password), "%08" PRIu32,
             esp_random() % UINT32_C(100000000));
    char ssid[33] = {0}, password[64] = {0};
    network_status.provisioned = load_wifi(ssid, password);
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();
    ESP_ERROR_CHECK(esp_netif_init());
    esp_err_t loop_err = esp_event_loop_create_default();
    if (loop_err != ESP_OK && loop_err != ESP_ERR_INVALID_STATE) return loop_err;
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                network_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                network_event, NULL));
    if (network_status.provisioned) {
        esp_netif_t *station = esp_netif_create_default_wifi_sta();
        ESP_ERROR_CHECK(station == NULL ? ESP_ERR_NO_MEM : ESP_OK);
        ESP_ERROR_CHECK(esp_netif_set_hostname(station,
                                               network_status.hostname));
        wifi_config_t config = {0};
        memcpy(config.sta.ssid, ssid, strlen(ssid));
        memcpy(config.sta.password, password, strlen(password));
        config.sta.threshold.authmode = strlen(password) == 0
                                            ? WIFI_AUTH_OPEN
                                            : WIFI_AUTH_WPA2_PSK;
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &config));
    } else {
        esp_netif_create_default_wifi_ap();
        wifi_config_t config = {0};
        memcpy(config.ap.ssid, network_status.ap_ssid,
               strlen(network_status.ap_ssid));
        config.ap.ssid_len = strlen(network_status.ap_ssid);
        memcpy(config.ap.password, network_status.setup_password,
               strlen(network_status.setup_password));
        config.ap.max_connection = 2;
        config.ap.authmode = WIFI_AUTH_WPA2_PSK;
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &config));
        memcpy(network_status.ip, "192.168.4.1", sizeof("192.168.4.1"));
        ESP_LOGI(TAG, "SETUP: ssid=%s password=%s url=http://192.168.4.1",
                 network_status.ap_ssid, network_status.setup_password);
    }
    memset(password, 0, sizeof(password));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(start_server());
    if (network_status.provisioned) {
        ESP_ERROR_CHECK(mdns_init());
        ESP_ERROR_CHECK(mdns_hostname_set(network_status.hostname));
        ESP_ERROR_CHECK(mdns_instance_name_set("Johnny Castaway"));
        ESP_ERROR_CHECK(mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0));
    }
    xTaskCreate(time_monitor, "jc_time", 3072, NULL, 3, NULL);
    return ESP_OK;
}

void jcnet_status(jcnet_status_t *status)
{
    if (status == NULL) return;
    memset(status, 0, sizeof(*status));
    if (status_lock != NULL && xSemaphoreTake(status_lock, pdMS_TO_TICKS(20))) {
        *status = network_status;
        xSemaphoreGive(status_lock);
    }
}
