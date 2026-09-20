// SPDX-License-Identifier: MIT
#pragma once
#include "driver/i2c_master.h"
esp_err_t orientation_imu_init(i2c_master_bus_handle_t bus);
esp_err_t orientation_imu_read(float *x,float *y,float *z);
