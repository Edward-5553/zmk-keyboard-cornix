// SPDX-License-Identifier: MIT
#include "orientation_imu.h"
#include "vendor/bmi270/bmi270.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"
#include "esp_log.h"
#include <string.h>
static i2c_master_dev_handle_t imu;
static struct bmi2_dev dev;
static BMI2_INTF_RETURN_TYPE read_regs(uint8_t reg,uint8_t *data,uint32_t len,void *ctx) {
    return i2c_master_transmit_receive(imu,&reg,1,data,len,50)==ESP_OK?0:-1;
}
static BMI2_INTF_RETURN_TYPE write_regs(uint8_t reg,const uint8_t *data,uint32_t len,void *ctx) {
    uint8_t bytes[65];if(len>64)return -1;
    bytes[0]=reg;memcpy(bytes+1,data,len);
    return i2c_master_transmit(imu,bytes,len+1,50)==ESP_OK?0:-1;
}
static void delay_us(uint32_t us,void *ctx) {
    if(us>=1000)vTaskDelay(pdMS_TO_TICKS((us+999)/1000));else esp_rom_delay_us(us);
}
esp_err_t orientation_imu_init(i2c_master_bus_handle_t bus) {
    i2c_device_config_t cfg={.dev_addr_length=I2C_ADDR_BIT_LEN_7,.device_address=0x68,.scl_speed_hz=100000};
    esp_err_t rc=i2c_master_bus_add_device(bus,&cfg,&imu);if(rc!=ESP_OK)return rc;
    dev=(struct bmi2_dev){.intf=BMI2_I2C_INTF,.read=read_regs,.write=write_regs,
                         .delay_us=delay_us,.read_write_len=64};
    int8_t result=bmi270_init(&dev);
    struct bmi2_sens_config config={.type=BMI2_ACCEL};
    if(result==BMI2_OK)result=bmi2_get_sensor_config(&config,1,&dev);
    config.cfg.acc.odr=BMI2_ACC_ODR_25HZ;config.cfg.acc.range=BMI2_ACC_RANGE_2G;
    config.cfg.acc.bwp=BMI2_ACC_NORMAL_AVG4;config.cfg.acc.filter_perf=BMI2_PERF_OPT_MODE;
    if(result==BMI2_OK)result=bmi2_set_sensor_config(&config,1,&dev);
    uint8_t sensor=BMI2_ACCEL;
    if(result==BMI2_OK)result=bmi270_sensor_enable(&sensor,1,&dev);
    if(result!=BMI2_OK) {
        ESP_LOGW("orientation","BMI270 init failed (%d); fixed display remains",result);
        i2c_master_bus_rm_device(imu);imu=NULL;return ESP_FAIL;
    }
    return ESP_OK;
}
esp_err_t orientation_imu_read(float *x,float *y,float *z) {
    if(!imu)return ESP_ERR_INVALID_STATE;
    struct bmi2_sens_data data={0};
    if(bmi2_get_sensor_data(&data,&dev)!=BMI2_OK)return ESP_FAIL;
    // Match StopWatch official HAL's swapped X/Y mounting axes; sign/offset configurable.
    *x=data.acc.y/16384.0f;*y=data.acc.x/16384.0f;*z=data.acc.z/16384.0f;
    return ESP_OK;
}
