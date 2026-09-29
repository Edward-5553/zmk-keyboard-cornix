#pragma once
#include <stdint.h>
#include <assert.h>
typedef struct {
    struct {const uint8_t *full_speed_config;const char **string;unsigned string_count;} descriptor;
} tinyusb_config_t;
int tinyusb_driver_install(const tinyusb_config_t *cfg);
#define ESP_ERROR_CHECK(expr) assert((expr)==0)
