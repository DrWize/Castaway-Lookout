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

esp_err_t jcnet_start(void);
void jcnet_status(jcnet_status_t *status);
