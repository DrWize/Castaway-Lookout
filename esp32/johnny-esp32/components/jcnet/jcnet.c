#include "jcnet.h"

#include <stdio.h>
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cJSON.h"
#include "esp_event.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
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
extern const uint8_t favicon_svg_start[] asm("_binary_favicon_svg_start");
enum {
    AUTH_SCHEMA = 1,
    PBKDF2_ITERATIONS = 60000,
    SESSION_BYTES = 24,
    HTTP_BODY_MAX = 384,
    WEATHER_JSON_MAX = 6144,
    WEATHER_SCHEMA = 1,
    WEATHER_REFRESH_SECONDS = 45 * 60,
};

static const char CONTROL_PAGE[] =
"<!doctype html><meta name=viewport content='width=device-width,initial-scale=1'>"
"<link rel=icon href=/favicon.svg type=image/svg+xml>"
"<title>Johnny Castaway</title><style>body{font:16px system-ui;background:#07131d;color:#eee;max-width:900px;margin:auto;padding:18px}h1{color:#ffd54a}.card{background:#102635;padding:16px;border-radius:12px;margin:12px 0}button,select,input,textarea{font:inherit;padding:10px;margin:4px;border-radius:8px;border:1px solid #567}button{background:#e9a928;color:#111}.scenes{display:grid;grid-template-columns:repeat(auto-fit,minmax(240px,1fr));gap:6px}.scene{background:#18394b;color:#fff;text-align:left}textarea{box-sizing:border-box;width:100%;min-height:90px;background:#07131d;color:#fff}small{color:#9bc}#msg{min-height:1.4em}</style>"
"<h1>Johnny Castaway</h1><div id=loginPanel class=card><h2>Login</h2><input id=p type=password placeholder='Administrator password' onkeydown=\"if(event.key==='Enter')submitLogin()\"><button onclick=submitLogin()>Login</button><p id=loginMsg></p></div>"
"<main id=app hidden><div class=card><b id=current>Loading...</b><p id=net></p><button onclick=randomPlay()>Random now</button></div>"
"<div class=card><label>Playback <select id=mode onchange=settings()><option value=normal>Normal</option><option value=review>Review</option></select></label>"
"<label>Sidebar <select id=sidebar onchange=settings()><option value=off>Off</option><option value=clock>Clock &amp; weather</option><option value=review>Reviewer</option></select></label>"
"<label>Sky <select id=sky onchange=settings()><option value=automatic>Automatic</option><option value=day>Day</option><option value=night>Night</option><option value=cycle>Cycle 10 day / 10 night</option></select></label>"
"<label>Holiday <select id=holiday onchange=settings()><option value=off>Off</option><option value=automatic>Automatic</option><option value=halloween>Halloween</option><option value=st_patrick>St. Patrick</option><option value=christmas>Christmas</option><option value=new_year>New Year</option></select></label></div>"
"<div id=review class=card hidden><h2>Review</h2><button onclick=reviewAction('previous')>Previous</button><button onclick=reviewAction('ok')>Looks OK</button><button onclick=reviewAction('bug')>Bug</button><button onclick=reviewAction('next')>Next</button></div>"
"<div class=card><h2>Clock &amp; weather</h2><p id=weather>Choose a location.</p><input id=city maxlength=64 placeholder='City or postal code' onkeydown=\"if(event.key==='Enter')searchCity()\"><button onclick=searchCity()>Search</button><div id=locations></div></div>"
"<div class=card><h2>Bug Log (<span id=bugcount>0</span>)</h2><button onclick=copyAll()>Copy All</button><button onclick=clearBugs()>Clear All</button><div id=bugs></div></div>"
"<div class=card><h2>Scenes</h2><div id=scenes class=scenes></div></div><p id=msg></p></main>"
"<script>const el=id=>document.getElementById(id);const j=(u,o={})=>fetch(u,{headers:{'Content-Type':'application/json'},...o}).then(async r=>{if(!r.ok)throw Error(await r.text());return r.status==204?{}:r.json()});"
"async function submitLogin(){try{await j('/api/v1/session',{method:'POST',body:JSON.stringify({password:el('p').value})});el('loginPanel').hidden=true;el('app').hidden=false;el('loginMsg').textContent='';await load()}catch(e){el('loginMsg').textContent=e.message||String(e)}}"
"async function load(){let [s,c]=await Promise.all([j('/api/v1/status'),j('/api/v1/scenes')]);show(s);el('scenes').innerHTML=c.scenes.map(x=>`<button class=scene onclick=play('${x.id}')><small>${x.id} - ${x.category}</small><br>${x.title}</button>`).join('');await loadBugs()}"
"function show(s){el('current').textContent=`${s.current_scene.id}: ${s.current_scene.title} | ${s.current_scene.ads} tag ${s.current_scene.tag} | frame ${s.frame}`;el('net').textContent=`${s.playback_mode} | ${s.effective_night?'Night':'Day'} | block ${s.cycle.block} scene ${s.cycle.position}/10 | ${s.shuffle_remaining} left`;el('mode').value=s.playback_mode;el('sidebar').value=s.sidebar_mode;el('sky').value=s.sky;el('holiday').value=s.holiday;el('review').hidden=s.playback_mode!='review';el('bugcount').textContent=s.bug_count;let w=s.weather;el('weather').textContent=!w.configured?'Choose a location.':`${w.location} | ${w.available?(w.temperature_tenths/10).toFixed(1)+' C, '+(w.low_tenths/10).toFixed(1)+' / '+(w.high_tenths/10).toFixed(1)+' C'+' | '+(s.time_synced&&w.updated_at>0?'Last updated '+new Date(w.updated_at*1000).toLocaleString(undefined,{timeZone:w.timezone||'UTC',month:'short',day:'numeric',hour:'2-digit',minute:'2-digit'}):'Saved weather'):'Waiting for weather'}`}"
"async function settings(){try{show(await j('/api/v1/settings',{method:'PUT',body:JSON.stringify({playback_mode:el('mode').value,sidebar_mode:el('sidebar').value,sky:el('sky').value,holiday:el('holiday').value})}));el('msg').textContent='Applied'}catch(e){el('msg').textContent=e.message||String(e)}}"
"async function searchCity(){try{let r=await j('/api/v1/weather/search',{method:'POST',body:JSON.stringify({query:el('city').value})});window.locations=r.locations;let box=el('locations');box.replaceChildren();if(!r.locations.length){box.textContent='No matches';return}r.locations.forEach((x,i)=>{let b=document.createElement('button');b.textContent=x.name+(x.region?', '+x.region:'')+', '+x.country;b.onclick=()=>chooseCity(i);box.appendChild(b)})}catch(e){el('msg').textContent=e.message||String(e)}}"
"async function chooseCity(i){try{let x=window.locations[i];show(await j('/api/v1/settings',{method:'PUT',body:JSON.stringify({location:{name:x.name,timezone:x.timezone,latitude:x.latitude,longitude:x.longitude}})}));el('locations').innerHTML='';el('msg').textContent='Location saved; weather will refresh shortly'}catch(e){el('msg').textContent=e.message||String(e)}}"
"async function play(id){try{show(await j('/api/v1/playback/scene',{method:'POST',body:JSON.stringify({scene_id:id})}));el('msg').textContent='Scene started'}catch(e){el('msg').textContent=e.message||String(e)}}"
"async function randomPlay(){try{show(await j('/api/v1/playback/random',{method:'POST',body:'{}'}));el('msg').textContent='Random playback resumed'}catch(e){el('msg').textContent=e.message||String(e)}}"
"async function reviewAction(action){try{show(await j('/api/v1/playback/review',{method:'POST',body:JSON.stringify({action})}));if(action=='bug')await loadBugs();el('msg').textContent=action=='bug'?'Bug captured':'Review advanced'}catch(e){el('msg').textContent=e.message||String(e)}}"
"function copyText(t){if(navigator.clipboard&&window.isSecureContext)return navigator.clipboard.writeText(t);let a=document.createElement('textarea');a.value=t;document.body.appendChild(a);a.select();document.execCommand('copy');a.remove();return Promise.resolve()}"
"async function loadBugs(){let b=await j('/api/v1/bugs');el('bugcount').textContent=b.count;el('bugs').innerHTML=b.bugs.map(x=>`<div><textarea readonly>${x.report}</textarea><button onclick='copyBug(${x.scene_index})'>Copy</button><button onclick='resolveBug(${x.scene_index})'>Resolve</button></div>`).join('');window.bugData=b.bugs}"
"function copyBug(i){let x=(window.bugData||[]).find(x=>x.scene_index==i);if(x)copyText(x.report)}function copyAll(){copyText((window.bugData||[]).map(x=>x.report).join('\\n\\n'))}"
"async function resolveBug(i){await j('/api/v1/bugs/resolve',{method:'POST',body:JSON.stringify({scene_index:i})});await loadBugs()}async function clearBugs(){if(confirm('Clear the complete bug log?')){await j('/api/v1/bugs',{method:'DELETE'});await loadBugs()}}"
"setInterval(()=>{if(!el('app').hidden)j('/api/v1/status').then(show)},3000)</script>";

static const char SETUP_PAGE[] =
"<!doctype html><meta name=viewport content='width=device-width,initial-scale=1'><title>Johnny setup</title>"
"<link rel=icon href=/favicon.svg type=image/svg+xml>"
"<style>body{font:18px system-ui;max-width:520px;margin:auto;padding:24px;background:#07131d;color:#fff}input,button{box-sizing:border-box;width:100%;padding:12px;margin:7px 0;font:inherit}button{background:#e9a928;border:0}</style>"
"<h1>Johnny Castaway setup</h1><p>Enter a 2.4 GHz Wi-Fi network and create the administrator password.</p>"
"<input id=s placeholder='Wi-Fi name (SSID)' maxlength=32><input id=w type=password placeholder='Wi-Fi password' maxlength=63>"
"<input id=a type=password placeholder='Administrator password (8+ characters)' maxlength=64><button onclick=save()>Connect</button><p id=m></p>"
"<script>async function save(){m.textContent='Saving...';let r=await fetch('/api/v1/setup',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid:s.value,wifi_password:w.value,admin_password:a.value})});m.textContent=await r.text()}</script>";

static SemaphoreHandle_t status_lock;
static jcnet_status_t network_status;
static jcnet_weather_status_t weather_status;
static httpd_handle_t server;
static int reconnects;
static bool fallback_ap_active;
static uint32_t weather_generation;

typedef struct {
    char *data;
    size_t capacity;
    size_t used;
    bool overflow;
} http_buffer_t;

static esp_err_t http_event(esp_http_client_event_t *event)
{
    if (event->event_id != HTTP_EVENT_ON_DATA || event->data_len <= 0) {
        return ESP_OK;
    }
    http_buffer_t *buffer = event->user_data;
    if (buffer == NULL || buffer->used + (size_t)event->data_len >=
                              buffer->capacity) {
        if (buffer != NULL) buffer->overflow = true;
        return ESP_FAIL;
    }
    memcpy(buffer->data + buffer->used, event->data, event->data_len);
    buffer->used += (size_t)event->data_len;
    buffer->data[buffer->used] = '\0';
    return ESP_OK;
}

static esp_err_t fetch_json(const char *url, char *data, size_t capacity)
{
    if (url == NULL || data == NULL || capacity < 2) return ESP_ERR_INVALID_ARG;
    http_buffer_t buffer = {.data = data, .capacity = capacity};
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event,
        .user_data = &buffer,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 10000,
        .buffer_size = 1024,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) return ESP_ERR_NO_MEM;
    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    if (buffer.overflow) return ESP_ERR_INVALID_SIZE;
    return err == ESP_OK && status == 200 ? ESP_OK : ESP_FAIL;
}

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

static void load_weather_state(void)
{
    jcnet_weather_status_t loaded = {0};
    nvs_handle_t handle = 0;
    uint8_t schema = 0;
    if (nvs_open("jc_weather", NVS_READONLY, &handle) != ESP_OK) return;
    size_t location_size = sizeof(loaded.location);
    size_t timezone_size = sizeof(loaded.timezone);
    size_t value_size = sizeof(double);
    bool valid = nvs_get_u8(handle, "schema", &schema) == ESP_OK &&
                 schema == WEATHER_SCHEMA &&
                 nvs_get_str(handle, "location", loaded.location,
                             &location_size) == ESP_OK &&
                 nvs_get_str(handle, "timezone", loaded.timezone,
                             &timezone_size) == ESP_OK &&
                 nvs_get_blob(handle, "latitude", &loaded.latitude,
                              &value_size) == ESP_OK &&
                 value_size == sizeof(double);
    value_size = sizeof(double);
    valid = valid && nvs_get_blob(handle, "longitude", &loaded.longitude,
                                  &value_size) == ESP_OK &&
            value_size == sizeof(double);
    loaded.configured = valid && isfinite(loaded.latitude) &&
                        isfinite(loaded.longitude) &&
                        loaded.latitude >= -90.0 && loaded.latitude <= 90.0 &&
                        loaded.longitude >= -180.0 && loaded.longitude <= 180.0;
    bool forecast = loaded.configured &&
        nvs_get_i16(handle, "temperature", &loaded.temperature_tenths) == ESP_OK &&
        nvs_get_i16(handle, "high", &loaded.high_tenths) == ESP_OK &&
        nvs_get_i16(handle, "low", &loaded.low_tenths) == ESP_OK &&
        nvs_get_u8(handle, "code", &loaded.weather_code) == ESP_OK &&
        nvs_get_i32(handle, "utc_offset", &loaded.utc_offset_seconds) == ESP_OK &&
        nvs_get_i64(handle, "updated", &loaded.updated_at) == ESP_OK;
    nvs_close(handle);
    loaded.available = forecast;
    loaded.stale = forecast;
    xSemaphoreTake(status_lock, portMAX_DELAY);
    weather_status = loaded;
    xSemaphoreGive(status_lock);
}

static esp_err_t store_weather_location(const char *location,
                                        const char *timezone,
                                        double latitude, double longitude)
{
    if (location == NULL || timezone == NULL || location[0] == '\0' ||
        timezone[0] == '\0' || strlen(location) >= 40 ||
        strlen(timezone) >= 40 || !isfinite(latitude) ||
        !isfinite(longitude) || latitude < -90.0 || latitude > 90.0 ||
        longitude < -180.0 || longitude > 180.0) {
        return ESP_ERR_INVALID_ARG;
    }
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open("jc_weather", NVS_READWRITE, &handle);
    if (err == ESP_OK) err = nvs_set_u8(handle, "schema", WEATHER_SCHEMA);
    if (err == ESP_OK) err = nvs_set_str(handle, "location", location);
    if (err == ESP_OK) err = nvs_set_str(handle, "timezone", timezone);
    if (err == ESP_OK) {
        err = nvs_set_blob(handle, "latitude", &latitude, sizeof(latitude));
    }
    if (err == ESP_OK) {
        err = nvs_set_blob(handle, "longitude", &longitude, sizeof(longitude));
    }
    if (err == ESP_OK) err = nvs_erase_key(handle, "temperature");
    if (err == ESP_ERR_NVS_NOT_FOUND) err = ESP_OK;
    if (err == ESP_OK) err = nvs_commit(handle);
    if (handle != 0) nvs_close(handle);
    if (err != ESP_OK) return err;
    xSemaphoreTake(status_lock, portMAX_DELAY);
    memset(&weather_status, 0, sizeof(weather_status));
    weather_status.configured = true;
    weather_status.latitude = latitude;
    weather_status.longitude = longitude;
    snprintf(weather_status.location, sizeof(weather_status.location), "%s",
             location);
    snprintf(weather_status.timezone, sizeof(weather_status.timezone), "%s",
             timezone);
    ++weather_generation;
    xSemaphoreGive(status_lock);
    ESP_LOGI(TAG, "WEATHER: location=%s latitude=%.4f longitude=%.4f timezone=%s",
             location, latitude, longitude, timezone);
    return ESP_OK;
}

static esp_err_t store_forecast(const jcnet_weather_status_t *forecast)
{
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open("jc_weather", NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        err = nvs_set_i16(handle, "temperature", forecast->temperature_tenths);
    }
    if (err == ESP_OK) err = nvs_set_i16(handle, "high", forecast->high_tenths);
    if (err == ESP_OK) err = nvs_set_i16(handle, "low", forecast->low_tenths);
    if (err == ESP_OK) err = nvs_set_u8(handle, "code", forecast->weather_code);
    if (err == ESP_OK) {
        err = nvs_set_i32(handle, "utc_offset", forecast->utc_offset_seconds);
    }
    if (err == ESP_OK) err = nvs_set_i64(handle, "updated", forecast->updated_at);
    if (err == ESP_OK) err = nvs_commit(handle);
    if (handle != 0) nvs_close(handle);
    return err;
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

static esp_err_t favicon_handler(httpd_req_t *request)
{
    httpd_resp_set_type(request, "image/svg+xml");
    httpd_resp_set_hdr(request, "Cache-Control", "public, max-age=86400");
    return httpd_resp_sendstr(request, (const char *)favicon_svg_start);
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

static esp_err_t session_storage_error(httpd_req_t *request,
                                       const char *reference, esp_err_t error)
{
    ESP_LOGE(TAG, "LOGIN: ref=%s error=%s (0x%x)", reference,
             esp_err_to_name(error), (unsigned)error);
    nvs_stats_t stats = {0};
    esp_err_t stats_error = nvs_get_stats(NULL, &stats);
    if (stats_error == ESP_OK) {
        ESP_LOGE(TAG,
                 "LOGIN: NVS used=%u free=%u available=%u total=%u namespaces=%u",
                 (unsigned)stats.used_entries, (unsigned)stats.free_entries,
                 (unsigned)stats.available_entries, (unsigned)stats.total_entries,
                 (unsigned)stats.namespace_count);
    } else {
        ESP_LOGE(TAG, "LOGIN: NVS statistics unavailable error=%s (0x%x)",
                 esp_err_to_name(stats_error), (unsigned)stats_error);
    }
    char message[96];
    snprintf(message, sizeof(message),
             "Could not save your login session. Reference: %s", reference);
    return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, message);
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
    const char *reference = "LOGIN_OPEN";
    esp_err_t err = nvs_open("jc_web", NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        reference = "LOGIN_WRITE";
        err = nvs_set_blob(handle, "session", digest, sizeof(digest));
    }
    if (err == ESP_OK) {
        reference = "LOGIN_COMMIT";
        err = nvs_commit(handle);
    }
    if (handle != 0) nvs_close(handle);
    if (err != ESP_OK) return session_storage_error(request, reference, err);
    ESP_LOGI(TAG, "LOGIN: session saved");
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

static void json_escape_copy(const char *input, char *output, size_t capacity)
{
    size_t used = 0;
    for (const char *at = input; at != NULL && *at && used + 1 < capacity; ++at) {
        if ((*at == '"' || *at == '\\') && used + 2 < capacity) {
            output[used++] = '\\';
        } else if ((unsigned char)*at < 0x20) {
            continue;
        }
        output[used++] = *at;
    }
    output[used] = '\0';
}

static bool json_number(const cJSON *object, const char *name, double *value)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    if (!cJSON_IsNumber(item) || !isfinite(item->valuedouble)) return false;
    *value = item->valuedouble;
    return true;
}

static bool parse_forecast_json(const char *json,
                                jcnet_weather_status_t *forecast)
{
    cJSON *root = cJSON_Parse(json);
    cJSON *current = root == NULL ? NULL :
        cJSON_GetObjectItemCaseSensitive(root, "current");
    cJSON *daily = root == NULL ? NULL :
        cJSON_GetObjectItemCaseSensitive(root, "daily");
    cJSON *highs = cJSON_IsObject(daily) ?
        cJSON_GetObjectItemCaseSensitive(daily, "temperature_2m_max") : NULL;
    cJSON *lows = cJSON_IsObject(daily) ?
        cJSON_GetObjectItemCaseSensitive(daily, "temperature_2m_min") : NULL;
    cJSON *high = cJSON_IsArray(highs) ? cJSON_GetArrayItem(highs, 0) : NULL;
    cJSON *low = cJSON_IsArray(lows) ? cJSON_GetArrayItem(lows, 0) : NULL;
    cJSON *offset = root == NULL ? NULL :
        cJSON_GetObjectItemCaseSensitive(root, "utc_offset_seconds");
    double temperature = 0, code = 0;
    bool valid = cJSON_IsObject(current) &&
                 json_number(current, "temperature_2m", &temperature) &&
                 json_number(current, "weather_code", &code) &&
                 cJSON_IsNumber(high) && isfinite(high->valuedouble) &&
                 cJSON_IsNumber(low) && isfinite(low->valuedouble) &&
                 cJSON_IsNumber(offset) &&
                 temperature >= -100.0 && temperature <= 100.0 &&
                 high->valuedouble >= -100.0 && high->valuedouble <= 100.0 &&
                 low->valuedouble >= -100.0 && low->valuedouble <= 100.0 &&
                 code >= 0 && code <= 99 && code == (double)(int)code &&
                 offset->valuedouble >= -86400 &&
                 offset->valuedouble <= 86400;
    if (valid) {
        forecast->temperature_tenths = (int16_t)(temperature * 10.0 +
                                                  (temperature >= 0 ? 0.5 : -0.5));
        forecast->high_tenths = (int16_t)(high->valuedouble * 10.0 +
                                           (high->valuedouble >= 0 ? 0.5 : -0.5));
        forecast->low_tenths = (int16_t)(low->valuedouble * 10.0 +
                                          (low->valuedouble >= 0 ? 0.5 : -0.5));
        forecast->weather_code = (uint8_t)code;
        forecast->utc_offset_seconds = offset->valueint;
        forecast->available = true;
        forecast->stale = false;
        time_t now = time(NULL);
        forecast->updated_at = now > 1704067200 ? now : 0;
    }
    cJSON_Delete(root);
    return valid;
}

static esp_err_t verify_weather_parsers(void)
{
    static const char fixture[] =
        "{\"utc_offset_seconds\":7200,\"current\":{"
        "\"temperature_2m\":18.4,\"weather_code\":3},\"daily\":{"
        "\"temperature_2m_max\":[21.6],\"temperature_2m_min\":[10.2]}}";
    jcnet_weather_status_t parsed = {0};
    if (!parse_forecast_json(fixture, &parsed) ||
        parsed.temperature_tenths != 184 || parsed.high_tenths != 216 ||
        parsed.low_tenths != 102 || parsed.weather_code != 3 ||
        parsed.utc_offset_seconds != 7200) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    ESP_LOGI(TAG, "VERIFY: Open-Meteo forecast parser fixture PASS");
    return ESP_OK;
}

static bool url_encode(const char *input, char *output, size_t capacity)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t used = 0;
    for (const unsigned char *at = (const unsigned char *)input; *at; ++at) {
        bool safe = (*at >= 'a' && *at <= 'z') ||
                    (*at >= 'A' && *at <= 'Z') ||
                    (*at >= '0' && *at <= '9') || *at == '-' || *at == '_';
        size_t needed = safe ? 1 : 3;
        if (used + needed >= capacity) return false;
        if (safe) {
            output[used++] = (char)*at;
        } else {
            output[used++] = '%';
            output[used++] = hex[*at >> 4];
            output[used++] = hex[*at & 15];
        }
    }
    output[used] = '\0';
    return true;
}

static esp_err_t status_response(httpd_req_t *request)
{
    jccontrol_snapshot_t playback = {0};
    jcnet_status_t net = {0};
    jcnet_weather_status_t weather = {0};
    jccontrol_snapshot(&playback);
    jcnet_status(&net);
    jcnet_weather_status(&weather);
    const jcengine_story_scene_t *scene =
        jcengine_story_scene(playback.scene_index);
    const jcengine_scene_menu_entry_t *menu =
        jcengine_scene_menu_entry(playback.scene_index);
    char location[80] = {0}, timezone[80] = {0};
    json_escape_copy(weather.location, location, sizeof(location));
    json_escape_copy(weather.timezone, timezone, sizeof(timezone));
    char json[1600];
    snprintf(json, sizeof(json),
             "{\"hostname\":\"%s\",\"ip\":\"%s\",\"rssi\":%d,"
             "\"connected\":%s,\"time_synced\":%s,\"uptime_seconds\":%u,"
             "\"frame\":%u,\"shuffle_remaining\":%u,\"paused\":%s,"
             "\"playback_mode\":\"%s\",\"sidebar_mode\":\"%s\","
             "\"sky\":\"%s\",\"holiday\":\"%s\","
             "\"effective_night\":%s,\"effective_holiday\":%u,"
             "\"cycle\":{\"block\":%u,\"position\":%u},\"story_day\":%u,"
             "\"island\":{\"x\":%d,\"y\":%d,\"low_tide\":%s,\"raft_stage\":%u},"
             "\"catalog_fingerprint\":\"%016" PRIx64 "\",\"firmware\":\"%s\","
             "\"weather\":{\"configured\":%s,\"available\":%s,\"stale\":%s,"
             "\"location\":\"%s\",\"timezone\":\"%s\","
             "\"latitude\":%.6f,\"longitude\":%.6f,"
             "\"temperature_tenths\":%d,\"high_tenths\":%d,"
             "\"low_tenths\":%d,\"weather_code\":%u,"
             "\"updated_at\":%" PRId64 "},"
             "\"bug_count\":%u,\"current_scene\":{\"id\":\"SCENE_%02u\","
             "\"title\":\"%s\",\"category\":\"%s\",\"ads\":\"%s\",\"tag\":%u}}",
             net.hostname, net.ip, net.rssi, net.connected ? "true" : "false",
             net.time_synced ? "true" : "false", (unsigned)(esp_timer_get_time() / 1000000),
             (unsigned)playback.frame, (unsigned)playback.shuffle_remaining,
             playback.paused ? "true" : "false",
             jccontrol_playback_mode_name(playback.settings.playback_mode),
             jccontrol_sidebar_mode_name(playback.settings.sidebar_mode),
             jccontrol_sky_name(playback.settings.sky),
             jccontrol_holiday_name(playback.settings.holiday),
             playback.effective_night ? "true" : "false",
             playback.effective_holiday, (unsigned)playback.cycle_block,
             (unsigned)(playback.cycle_position + 1), playback.story_day,
             playback.island_x, playback.island_y,
             playback.low_tide ? "true" : "false", playback.raft_stage,
             playback.catalog_fingerprint, playback.firmware_version,
             weather.configured ? "true" : "false",
             weather.available ? "true" : "false",
             weather.stale ? "true" : "false", location, timezone,
             weather.latitude, weather.longitude,
             weather.temperature_tenths, weather.high_tenths,
             weather.low_tenths, weather.weather_code, weather.updated_at,
             (unsigned)jccontrol_bug_count(),
             (unsigned)(playback.scene_index + 1),
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

static esp_err_t weather_search_handler(httpd_req_t *request)
{
    if (!request_authenticated(request)) return unauthorized(request);
    char body[HTTP_BODY_MAX] = {0};
    if (read_body(request, body, sizeof(body)) != ESP_OK) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "bad body");
    }
    cJSON *root = cJSON_Parse(body);
    cJSON *query = root == NULL ? NULL : cJSON_GetObjectItem(root, "query");
    size_t query_length = cJSON_IsString(query) ? strlen(query->valuestring) : 0;
    char encoded[256] = {0};
    bool valid = query_length >= 2 && query_length <= 64 &&
                 url_encode(query->valuestring, encoded, sizeof(encoded));
    cJSON_Delete(root);
    if (!valid) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                                   "city query must contain 2-64 characters");
    }
    char url[384];
    snprintf(url, sizeof(url),
             "https://geocoding-api.open-meteo.com/v1/search?name=%s&count=3&language=en&format=json",
             encoded);
    char *json = calloc(1, WEATHER_JSON_MAX);
    if (json == NULL) return httpd_resp_send_500(request);
    esp_err_t err = fetch_json(url, json, WEATHER_JSON_MAX);
    if (err != ESP_OK) {
        free(json);
        httpd_resp_set_status(request, "502 Bad Gateway");
        return httpd_resp_sendstr(request, "location service unavailable");
    }
    root = cJSON_Parse(json);
    free(json);
    cJSON *results = root == NULL ? NULL :
        cJSON_GetObjectItemCaseSensitive(root, "results");
    if (root == NULL || (results != NULL && !cJSON_IsArray(results))) {
        cJSON_Delete(root);
        httpd_resp_set_status(request, "502 Bad Gateway");
        return httpd_resp_sendstr(request, "invalid location response");
    }
    httpd_resp_set_type(request, "application/json");
    httpd_resp_sendstr_chunk(request, "{\"locations\":[");
    bool first = true;
    size_t emitted = 0;
    cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, results) {
        cJSON *name = cJSON_GetObjectItemCaseSensitive(entry, "name");
        cJSON *country = cJSON_GetObjectItemCaseSensitive(entry, "country");
        cJSON *admin = cJSON_GetObjectItemCaseSensitive(entry, "admin1");
        cJSON *timezone = cJSON_GetObjectItemCaseSensitive(entry, "timezone");
        double latitude = 0, longitude = 0;
        if (!cJSON_IsString(name) || !cJSON_IsString(timezone) ||
            strlen(name->valuestring) >= 40 ||
            strlen(timezone->valuestring) >= 40 ||
            !json_number(entry, "latitude", &latitude) ||
            !json_number(entry, "longitude", &longitude)) {
            continue;
        }
        httpd_resp_sendstr_chunk(request, first ? "{" : ",{");
        first = false;
        httpd_resp_sendstr_chunk(request, "\"name\":");
        send_json_string(request, name->valuestring);
        httpd_resp_sendstr_chunk(request, ",\"country\":");
        send_json_string(request, cJSON_IsString(country) ? country->valuestring : "");
        httpd_resp_sendstr_chunk(request, ",\"region\":");
        send_json_string(request, cJSON_IsString(admin) ? admin->valuestring : "");
        httpd_resp_sendstr_chunk(request, ",\"timezone\":");
        send_json_string(request, timezone->valuestring);
        char coordinates[96];
        snprintf(coordinates, sizeof(coordinates),
                 ",\"latitude\":%.6f,\"longitude\":%.6f}",
                 latitude, longitude);
        httpd_resp_sendstr_chunk(request, coordinates);
        if (++emitted == 3) break;
    }
    cJSON_Delete(root);
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
    cJSON *playback_mode =
        root == NULL ? NULL : cJSON_GetObjectItem(root, "playback_mode");
    cJSON *sidebar_mode =
        root == NULL ? NULL : cJSON_GetObjectItem(root, "sidebar_mode");
    cJSON *location =
        root == NULL ? NULL : cJSON_GetObjectItem(root, "location");
    if (sky != NULL) {
        if (!cJSON_IsString(sky)) goto invalid;
        command.has_sky = jccontrol_parse_sky(sky->valuestring,
                                              &command.settings.sky);
        if (!command.has_sky) goto invalid;
    }
    if (holiday != NULL) {
        if (!cJSON_IsString(holiday)) goto invalid;
        command.has_holiday = jccontrol_parse_holiday(
            holiday->valuestring, &command.settings.holiday);
        if (!command.has_holiday) goto invalid;
    }
    if (playback_mode != NULL) {
        if (!cJSON_IsString(playback_mode)) goto invalid;
        command.has_playback_mode = jccontrol_parse_playback_mode(
            playback_mode->valuestring, &command.settings.playback_mode);
        if (!command.has_playback_mode) goto invalid;
    }
    if (sidebar_mode != NULL) {
        if (!cJSON_IsString(sidebar_mode)) goto invalid;
        command.has_sidebar_mode = jccontrol_parse_sidebar_mode(
            sidebar_mode->valuestring, &command.settings.sidebar_mode);
        if (!command.has_sidebar_mode) goto invalid;
    }
    char location_name[40] = {0}, location_timezone[40] = {0};
    double latitude = 0, longitude = 0;
    bool has_location = location != NULL;
    if (has_location) {
        cJSON *name = cJSON_IsObject(location) ?
            cJSON_GetObjectItemCaseSensitive(location, "name") : NULL;
        cJSON *timezone = cJSON_IsObject(location) ?
            cJSON_GetObjectItemCaseSensitive(location, "timezone") : NULL;
        if (!cJSON_IsString(name) || !cJSON_IsString(timezone) ||
            strlen(name->valuestring) == 0 || strlen(name->valuestring) >= 40 ||
            strlen(timezone->valuestring) == 0 ||
            strlen(timezone->valuestring) >= 40 ||
            !json_number(location, "latitude", &latitude) ||
            !json_number(location, "longitude", &longitude) ||
            latitude < -90.0 || latitude > 90.0 ||
            longitude < -180.0 || longitude > 180.0) goto invalid;
        snprintf(location_name, sizeof(location_name), "%s", name->valuestring);
        snprintf(location_timezone, sizeof(location_timezone), "%s",
                 timezone->valuestring);
    }
    bool has_command = command.has_sky || command.has_holiday ||
                       command.has_playback_mode || command.has_sidebar_mode;
    if (!has_command && !has_location) goto invalid;
    cJSON_Delete(root);
    esp_err_t err = has_command ? jccontrol_submit(&command, 2000) : ESP_OK;
    if (err == ESP_OK && has_location) {
        err = store_weather_location(location_name, location_timezone,
                                     latitude, longitude);
    }
    return command_result(request, err);
invalid:
    cJSON_Delete(root);
    return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                               "invalid sky, holiday, playback, sidebar or location");
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

static esp_err_t review_handler(httpd_req_t *request)
{
    if (!request_authenticated(request)) return unauthorized(request);
    char body[HTTP_BODY_MAX] = {0};
    if (read_body(request, body, sizeof(body)) != ESP_OK) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "bad body");
    }
    cJSON *root = cJSON_Parse(body);
    cJSON *action = root == NULL ? NULL : cJSON_GetObjectItem(root, "action");
    jccontrol_command_t command = {0};
    bool valid = cJSON_IsString(action);
    if (valid && strcmp(action->valuestring, "ok") == 0) {
        command.type = JCCONTROL_COMMAND_REVIEW_OK;
    } else if (valid && strcmp(action->valuestring, "bug") == 0) {
        command.type = JCCONTROL_COMMAND_REVIEW_BUG;
    } else if (valid && strcmp(action->valuestring, "previous") == 0) {
        command.type = JCCONTROL_COMMAND_REVIEW_PREVIOUS;
    } else if (valid && strcmp(action->valuestring, "next") == 0) {
        command.type = JCCONTROL_COMMAND_REVIEW_NEXT;
    } else {
        valid = false;
    }
    cJSON_Delete(root);
    if (!valid) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                                   "invalid review action");
    }
    return command_result(request, jccontrol_submit(&command, 2000));
}

static const char *effective_holiday_name(uint8_t holiday)
{
    static const char *const names[] = {
        "none", "halloween", "st_patrick", "christmas", "new_year",
    };
    return holiday < sizeof(names) / sizeof(names[0]) ? names[holiday]
                                                       : "unknown";
}

static esp_err_t bugs_handler(httpd_req_t *request)
{
    if (!request_authenticated(request)) return unauthorized(request);
    if (request->method == HTTP_POST) {
        jccontrol_command_t command = {.type = JCCONTROL_COMMAND_REVIEW_BUG};
        return command_result(request, jccontrol_submit(&command, 2000));
    }
    if (request->method == HTTP_DELETE) {
        esp_err_t err = jccontrol_bug_clear();
        if (err != ESP_OK) return httpd_resp_send_500(request);
        httpd_resp_set_type(request, "application/json");
        return httpd_resp_sendstr(request, "{\"cleared\":true}");
    }
    jccontrol_bug_record_t *records = calloc(
        JCENGINE_STORY_SCENE_COUNT, sizeof(*records));
    if (records == NULL) return httpd_resp_send_500(request);
    esp_err_t err = jccontrol_bug_records(records, JCENGINE_STORY_SCENE_COUNT);
    if (err != ESP_OK) {
        free(records);
        return httpd_resp_send_500(request);
    }
    size_t count = 0;
    for (size_t index = 0; index < JCENGINE_STORY_SCENE_COUNT; ++index) {
        if (records[index].present) ++count;
    }
    char prefix[48];
    snprintf(prefix, sizeof(prefix), "{\"count\":%u,\"bugs\":[",
             (unsigned)count);
    httpd_resp_set_type(request, "application/json");
    httpd_resp_sendstr_chunk(request, prefix);
    bool first = true;
    for (size_t index = 0; index < JCENGINE_STORY_SCENE_COUNT; ++index) {
        const jccontrol_bug_record_t *record = &records[index];
        if (!record->present) continue;
        const jcengine_story_scene_t *scene =
            jcengine_story_scene(record->scene_index);
        const jcengine_scene_menu_entry_t *menu =
            jcengine_scene_menu_entry(record->scene_index);
        char report[768];
        snprintf(
            report, sizeof(report),
            "SCENE_%02u - %s - %s tag %u - frame %04u | mode=%s sky=%s effective_sky=%s holiday=%s effective_holiday=%s story_day=%u cycle_block=%u cycle_position=%u/10 island=%d,%d tide=%s raft=%u firmware=%s catalog=%016" PRIx64 " captured_at=%" PRId64 " uptime=%" PRIu32 "s error=%" PRId32,
            (unsigned)(record->scene_index + 1),
            menu == NULL ? "Unknown" : menu->title,
            scene == NULL ? "UNKNOWN.ADS" : scene->ads_name,
            scene == NULL ? 0U : (unsigned)scene->ads_tag,
            (unsigned)record->frame,
            jccontrol_playback_mode_name(record->settings.playback_mode),
            jccontrol_sky_name(record->settings.sky),
            record->effective_night ? "night" : "day",
            jccontrol_holiday_name(record->settings.holiday),
            effective_holiday_name(record->effective_holiday),
            record->story_day, (unsigned)record->cycle_block,
            (unsigned)(record->cycle_position + 1), record->island_x,
            record->island_y, record->low_tide ? "low" : "high",
            record->raft_stage, record->firmware_version,
            record->catalog_fingerprint, record->captured_at,
            record->uptime_seconds, record->runtime_error);
        httpd_resp_sendstr_chunk(request,
                                 first ? "{\"scene_index\":" : ",{\"scene_index\":");
        first = false;
        snprintf(prefix, sizeof(prefix), "%u,\"report\":",
                 (unsigned)record->scene_index);
        httpd_resp_sendstr_chunk(request, prefix);
        send_json_string(request, report);
        httpd_resp_sendstr_chunk(request, "}");
    }
    free(records);
    httpd_resp_sendstr_chunk(request, "]}");
    return httpd_resp_sendstr_chunk(request, NULL);
}

static esp_err_t bug_resolve_handler(httpd_req_t *request)
{
    if (!request_authenticated(request)) return unauthorized(request);
    char body[HTTP_BODY_MAX] = {0};
    if (read_body(request, body, sizeof(body)) != ESP_OK) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "bad body");
    }
    cJSON *root = cJSON_Parse(body);
    cJSON *index = root == NULL ? NULL : cJSON_GetObjectItem(root, "scene_index");
    bool valid = cJSON_IsNumber(index) && index->valuedouble >= 0 &&
                 index->valuedouble < JCENGINE_STORY_SCENE_COUNT &&
                 index->valuedouble == (double)index->valueint;
    size_t scene_index = valid ? (size_t)index->valueint : 0;
    cJSON_Delete(root);
    if (!valid) {
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                                   "invalid scene_index");
    }
    if (jccontrol_bug_resolve(scene_index) != ESP_OK) {
        return httpd_resp_send_500(request);
    }
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_sendstr(request, "{\"resolved\":true}");
}

static esp_err_t start_server(void)
{
    if (server != NULL) return ESP_OK;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16;
    config.stack_size = 8192;
    esp_err_t err = httpd_start(&server, &config);
    if (err != ESP_OK) return err;
    const httpd_uri_t routes[] = {
        {.uri = "/", .method = HTTP_GET, .handler = root_handler},
        {.uri = "/favicon.svg", .method = HTTP_GET, .handler = favicon_handler},
        {.uri = "/api/v1/setup", .method = HTTP_POST, .handler = setup_handler},
        {.uri = "/api/v1/session", .method = HTTP_POST, .handler = session_handler},
        {.uri = "/api/v1/session", .method = HTTP_DELETE, .handler = logout_handler},
        {.uri = "/api/v1/status", .method = HTTP_GET, .handler = status_handler},
        {.uri = "/api/v1/scenes", .method = HTTP_GET, .handler = scenes_handler},
        {.uri = "/api/v1/weather/search", .method = HTTP_POST, .handler = weather_search_handler},
        {.uri = "/api/v1/settings", .method = HTTP_PUT, .handler = settings_handler},
        {.uri = "/api/v1/playback/scene", .method = HTTP_POST, .handler = scene_handler},
        {.uri = "/api/v1/playback/random", .method = HTTP_POST, .handler = random_handler},
        {.uri = "/api/v1/playback/review", .method = HTTP_POST, .handler = review_handler},
        {.uri = "/api/v1/bugs", .method = HTTP_GET, .handler = bugs_handler},
        {.uri = "/api/v1/bugs", .method = HTTP_POST, .handler = bugs_handler},
        {.uri = "/api/v1/bugs", .method = HTTP_DELETE, .handler = bugs_handler},
        {.uri = "/api/v1/bugs/resolve", .method = HTTP_POST, .handler = bug_resolve_handler},
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

static void weather_monitor(void *context)
{
    (void)context;
    int64_t last_attempt_us = -(int64_t)WEATHER_REFRESH_SECONDS * 1000000;
    uint32_t seen_generation = UINT32_MAX;
    while (true) {
        jcnet_status_t net = {0};
        jcnet_weather_status_t current = {0};
        uint32_t generation = 0;
        jcnet_status(&net);
        xSemaphoreTake(status_lock, portMAX_DELAY);
        current = weather_status;
        generation = weather_generation;
        xSemaphoreGive(status_lock);
        int64_t now_us = esp_timer_get_time();
        bool due = generation != seen_generation ||
                   now_us - last_attempt_us >=
                       (int64_t)WEATHER_REFRESH_SECONDS * 1000000;
        if (net.connected && current.configured && due) {
            seen_generation = generation;
            last_attempt_us = now_us;
            char url[512];
            snprintf(url, sizeof(url),
                     "https://api.open-meteo.com/v1/forecast?latitude=%.6f&longitude=%.6f&current=temperature_2m,weather_code&daily=temperature_2m_max,temperature_2m_min&timezone=auto&forecast_days=1",
                     current.latitude, current.longitude);
            char *json = calloc(1, WEATHER_JSON_MAX);
            esp_err_t err = json == NULL ? ESP_ERR_NO_MEM
                                         : fetch_json(url, json, WEATHER_JSON_MAX);
            jcnet_weather_status_t updated = current;
            if (err == ESP_OK && parse_forecast_json(json, &updated)) {
                err = store_forecast(&updated);
            } else if (err == ESP_OK) {
                err = ESP_ERR_INVALID_RESPONSE;
            }
            free(json);
            xSemaphoreTake(status_lock, portMAX_DELAY);
            if (weather_generation == generation) {
                if (err == ESP_OK) {
                    weather_status = updated;
                } else {
                    weather_status.stale = weather_status.available;
                }
            }
            xSemaphoreGive(status_lock);
            if (err == ESP_OK) {
                ESP_LOGI(TAG,
                         "WEATHER: updated location=%s temp=%.1fC high=%.1fC low=%.1fC code=%u",
                         updated.location, updated.temperature_tenths / 10.0,
                         updated.high_tenths / 10.0,
                         updated.low_tenths / 10.0, updated.weather_code);
            } else {
                ESP_LOGW(TAG, "WEATHER: refresh failed error=%s retained=%u",
                         esp_err_to_name(err), current.available);
            }
        }
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
    ESP_ERROR_CHECK(verify_weather_parsers());
    load_weather_state();
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
    xTaskCreate(weather_monitor, "jc_weather", 8192, NULL, 3, NULL);
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

void jcnet_weather_status(jcnet_weather_status_t *status)
{
    if (status == NULL) return;
    memset(status, 0, sizeof(*status));
    if (status_lock != NULL &&
        xSemaphoreTake(status_lock, pdMS_TO_TICKS(20)) == pdTRUE) {
        *status = weather_status;
        xSemaphoreGive(status_lock);
    }
}
