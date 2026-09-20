// SPDX-License-Identifier: MIT
// Panel sequence adapted from M5Stack M5StopWatch-UserDemo (MIT).
// Copyright (c) 2026 M5Stack Technology CO LTD (initialization sequence).
#include "display_hw.h"
#include "orientation_imu.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_co5300.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG="display_hw";
static esp_lcd_panel_handle_t panel;
static esp_lcd_panel_io_handle_t panel_io;
static SemaphoreHandle_t transfer_done;
static i2c_master_bus_handle_t bus;
static i2c_master_dev_handle_t ioe;
static bool spi_started;

static esp_err_t reg_update(uint8_t reg,uint8_t clear,uint8_t set) {
    uint8_t value;
    ESP_RETURN_ON_ERROR(i2c_master_transmit_receive(ioe,&reg,1,&value,1,100),TAG,"IOE read");
    uint8_t data[]={reg,(value & ~clear) | set};
    ESP_RETURN_ON_ERROR(i2c_master_transmit(ioe,data,2,100),TAG,"IOE write");
    return ESP_OK;
}
static esp_err_t screen_power(void) {
    i2c_master_bus_config_t cfg={.i2c_port=I2C_NUM_0,.sda_io_num=47,.scl_io_num=48,
        .clk_source=I2C_CLK_SRC_DEFAULT,.glitch_ignore_cnt=7,.flags.enable_internal_pullup=true};
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&cfg,&bus),TAG,"I2C bus");
    // IOE may sleep; first probe wakes it. Try the official fallback address too.
    int address=0;
    const int candidates[]={0x4f,0x6f};
    for(unsigned i=0;i<2 && !address;i++) for(unsigned attempt=0;attempt<3;attempt++) {
        if(i2c_master_probe(bus,candidates[i],100)==ESP_OK) {address=candidates[i];break;}
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    if(!address)return ESP_ERR_NOT_FOUND;
    i2c_device_config_t dev={.dev_addr_length=I2C_ADDR_BIT_LEN_7,.device_address=address,.scl_speed_hz=100000};
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus,&dev,&ioe),TAG,"IOE device");
    ESP_LOGI(TAG,"M5IOE1 address 0x%02x",address);
    // PYG5 = bit4, PYG8 = bit7. Preserve USB mux, charging, audio and all other GPIOs.
    const uint8_t mask=0x90;
    ESP_RETURN_ON_ERROR(reg_update(0x23,0x0f,0),TAG,"IOE sleep off");
    ESP_RETURN_ON_ERROR(reg_update(0x09,mask,0),TAG,"pull up off");
    ESP_RETURN_ON_ERROR(reg_update(0x0b,mask,0),TAG,"pull down off");
    ESP_RETURN_ON_ERROR(reg_update(0x13,mask,0),TAG,"push-pull");
    ESP_RETURN_ON_ERROR(reg_update(0x05,0x10,0x80),TAG,"power on/reset low");
    ESP_RETURN_ON_ERROR(reg_update(0x03,0,mask),TAG,"output mode");
    vTaskDelay(pdMS_TO_TICKS(80));
    ESP_RETURN_ON_ERROR(reg_update(0x05,0,mask),TAG,"reset high");
    vTaskDelay(pdMS_TO_TICKS(150));
    uint8_t reg=0x05,value=0;
    ESP_RETURN_ON_ERROR(i2c_master_transmit_receive(ioe,&reg,1,&value,1,100),TAG,"power readback");
    return (value & mask)==mask?ESP_OK:ESP_FAIL;
}
static bool IRAM_ATTR transfer_finished(esp_lcd_panel_io_handle_t io,
                                       esp_lcd_panel_io_event_data_t *event,void *context) {
    BaseType_t wake=pdFALSE;
    xSemaphoreGiveFromISR(transfer_done,&wake);
    return wake==pdTRUE;
}
static const co5300_lcd_init_cmd_t commands[]={
    {0x11,NULL,0,150}, {0xc4,(uint8_t[]){0x80},1,0},
    {0x35,(uint8_t[]){0x80},1,0}, {0x44,(uint8_t[]){0x01,0xd2},2,0},
    {0x53,(uint8_t[]){0x20},1,0}, {0x20,NULL,0,0},
    {0x36,(uint8_t[]){0x00},1,0}, {0x51,(uint8_t[]){0x00},1,0},
    {0x29,NULL,0,0},
};
esp_err_t display_hw_init(void) {
    esp_err_t err=screen_power();
    if(err!=ESP_OK)goto failed;
    transfer_done=xSemaphoreCreateBinary();
    if(!transfer_done){err=ESP_ERR_NO_MEM;goto failed;}
    spi_bus_config_t spi=CO5300_PANEL_BUS_QSPI_CONFIG(40,41,42,46,45,DISPLAY_BUFFER_BYTES);
    err=spi_bus_initialize(SPI2_HOST,&spi,SPI_DMA_CH_AUTO);
    if(err!=ESP_OK)goto failed;
    spi_started=true;
    esp_lcd_panel_io_spi_config_t io=CO5300_PANEL_IO_QSPI_CONFIG(39,transfer_finished,NULL);
    io.trans_queue_depth=1;
    io.pclk_hz=CONFIG_STOPWATCH_LCD_CLOCK_MHZ*1000000;
    err=esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST,&io,&panel_io);
    if(err!=ESP_OK)goto failed;
    co5300_vendor_config_t vendor={.init_cmds=commands,.init_cmds_size=sizeof(commands)/sizeof(commands[0]),
        .flags.use_qspi_interface=true};
    esp_lcd_panel_dev_config_t device={.reset_gpio_num=-1,.rgb_ele_order=LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel=16,.vendor_config=&vendor};
    err=esp_lcd_new_panel_co5300(panel_io,&device,&panel);
    if(err!=ESP_OK)goto failed;
    err=esp_lcd_panel_init(panel);
    if(err!=ESP_OK)goto failed;
    err=esp_lcd_panel_set_gap(panel,CONFIG_STOPWATCH_LCD_X_GAP,0);
    if(err!=ESP_OK)goto failed;
    return ESP_OK;
failed:
    if(panel){esp_lcd_panel_del(panel);panel=NULL;}
    if(panel_io){esp_lcd_panel_io_del(panel_io);panel_io=NULL;}
    if(spi_started){spi_bus_free(SPI2_HOST);spi_started=false;}
    if(transfer_done){vSemaphoreDelete(transfer_done);transfer_done=NULL;}
    if(ioe){i2c_master_bus_rm_device(ioe);ioe=NULL;}
    if(bus){i2c_del_master_bus(bus);bus=NULL;}
    return err;
}
esp_err_t display_hw_flush(int x1,int y1,int x2,int y2,const uint8_t *pixels) {
    ESP_RETURN_ON_ERROR(esp_lcd_panel_draw_bitmap(panel,x1,y1,x2,y2,pixels),TAG,"draw");
    return xSemaphoreTake(transfer_done,pdMS_TO_TICKS(500))==pdTRUE?ESP_OK:ESP_ERR_TIMEOUT;
}
esp_err_t display_hw_brightness(unsigned percent) {
    return esp_lcd_panel_co5300_set_brightness(panel,percent);
}

// Display output only: retain controller RAM, IOE supply and the keyboard radios.
esp_err_t display_hw_enabled(bool enabled) {
    return esp_lcd_panel_disp_on_off(panel,enabled);
}

esp_err_t display_hw_orientation_init(void) {return orientation_imu_init(bus);}

// Ensure a warm restart from the microphone firmware also cuts audio power.
void display_hw_audio_off(void) {
    gpio_set_direction(14,GPIO_MODE_OUTPUT);
    gpio_set_level(14,0);
    esp_err_t rc=reg_update(0x05,0x04,0);
    if(rc==ESP_OK)rc=reg_update(0x03,0,0x04);
    if(rc!=ESP_OK)ESP_LOGW(TAG,"Audio power-off failed: %s",esp_err_to_name(rc));
}
