#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

typedef struct {
    char ap_ssid[24];
    char setup_password[16];
    char hostname[32];
    char ip[16];
    int8_t rssi;
    bool provisioned;
    bool connected;
    bool time_synced;
} jcnet_status_t;

typedef struct {
    bool configured;
    bool available;
    bool stale;
    char location[40];
    char timezone[40];
    double latitude;
    double longitude;
    int32_t utc_offset_seconds;
    int16_t temperature_tenths;
    int16_t high_tenths;
    int16_t low_tenths;
    uint8_t weather_code;
    int64_t updated_at;
} jcnet_weather_status_t;

esp_err_t jcnet_start(void);
void jcnet_status(jcnet_status_t *status);
void jcnet_weather_status(jcnet_weather_status_t *status);
