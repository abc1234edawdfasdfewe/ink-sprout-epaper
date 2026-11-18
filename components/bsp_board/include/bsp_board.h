#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/* === Waveshare ESP32-S3-ePaper-3.97 Verified Pin Assignment === */
#define BSP_I2C_SDA_PIN         41
#define BSP_I2C_SCL_PIN         42
#define BSP_I2C_FREQ_HZ         400000

/* I2C 7-bit Slave Addresses */
#define BSP_ADDR_TG28_PMIC      0x34   /* TG28 / AXP2101 compatible PMU */
#define BSP_ADDR_ES8311         0x18   /* ES8311 Low-Power Audio Codec */
#define BSP_ADDR_PCF85063       0x51   /* PCF85063 Real-Time Clock */
#define BSP_ADDR_QMI8658        0x6A   /* QMI8658A 6-Axis IMU */
#define BSP_ADDR_SHTC3          0x70   /* SHTC3 Temp & Humidity Sensor */

/* 3.97" e-Paper (SSD1677, 800x480) */
#define BSP_EPD_BUSY_PIN        3
#define BSP_EPD_DC_PIN          9
#define BSP_EPD_CS_PIN          10
#define BSP_EPD_SCLK_PIN        11
#define BSP_EPD_MOSI_PIN        12
#define BSP_EPD_RST_PIN         46

/* 3-Way Thumbwheel + Side Buttons */
#define BSP_KEY_WHEEL_UP_PIN    4      /* Active LOW, 10K pull-up */
#define BSP_KEY_WHEEL_OK_PIN    5      /* Active LOW, 10K pull-up */
#define BSP_KEY_WHEEL_DOWN_PIN  6      /* Active LOW, 10K pull-up */
#define BSP_KEY_BOOT_PIN        0      /* Active LOW, side return key */
#define BSP_PMIC_IRQ_PIN        38     /* TG28 IRQ */
#define BSP_IMU_INT2_PIN        40     /* QMI8658 INT2 (GPIO39 reserved for PA!) */

/* Audio ES8311 + NS4150B Speaker PA */
#define BSP_I2S_MCLK_PIN        13
#define BSP_I2S_BCLK_PIN        14
#define BSP_I2S_WS_PIN          47
#define BSP_I2S_DOUT_PIN        48
#define BSP_I2S_DIN_PIN         21
#define BSP_PA_CTRL_PIN         39

/* MicroSD 4-bit SDMMC */
#define BSP_SD_CLK_PIN          16
#define BSP_SD_CMD_PIN          17
#define BSP_SD_D0_PIN           15
#define BSP_SD_D1_PIN           7
#define BSP_SD_D2_PIN           8
#define BSP_SD_D3_PIN           18
#define BSP_SD_MOUNT_POINT      "/sdcard"

typedef struct {
    uint16_t year;
    uint8_t  month;
    uint8_t  day;
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;
    uint8_t  weekday;
} bsp_rtc_time_t;

typedef struct {
    int   battery_percent;   /* 0..100, or -1 if no battery */
    int   vbat_mv;
    bool  charging;
    bool  usb_online;
    float temp_c;
    float humi_rh;
    bool  env_valid;
    bool  sd_mounted;
} bsp_status_t;

/* Master Initialization & Power Control */
esp_err_t bsp_board_init(void);
i2c_master_bus_handle_t bsp_i2c_get_bus(void);
esp_err_t bsp_i2c_write_reg(uint8_t addr, uint8_t reg, const uint8_t *data, size_t len);
esp_err_t bsp_i2c_read_reg(uint8_t addr, uint8_t reg, uint8_t *data, size_t len);

/* PMIC / Power */
void bsp_pmic_epd_power(bool on);
bool bsp_pmic_poll_pwr_short_press(void);
void bsp_board_power_off(void);

/* Sensors, RTC & Status */
void bsp_rtc_get_time(bsp_rtc_time_t *out);
void bsp_rtc_set_time(const bsp_rtc_time_t *t);
void bsp_board_read_status(bsp_status_t *st);
bool bsp_imu_poll_shake(void);

/* Unified Story Storage (MicroSD priority + 3.8MB Internal Flash FAT fallback) & USB MSC */
bool        bsp_sdcard_is_mounted(void);
bool        bsp_sdcard_try_mount(void);
const char *bsp_storage_get_root(void);
const char *bsp_storage_get_stories_dir(void);
const char *bsp_storage_get_medium_label(void);
void        bsp_storage_get_space_kb(uint32_t *total_kb, uint32_t *free_kb);
int         bsp_storage_count_stories(void);

esp_err_t   bsp_usb_msc_start(void);
void        bsp_usb_msc_stop(void);
bool        bsp_usb_msc_is_host_connected(void);

#ifdef __cplusplus
}
#endif
