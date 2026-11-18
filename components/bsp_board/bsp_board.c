#include "bsp_board.h"
#include "bsp_epd.h"
#include "bsp_audio.h"
#include "bsp_input.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "esp_vfs_fat.h"
#include "esp_partition.h"
#include "wear_levelling.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "driver/gpio.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "tinyusb_msc.h"
#include <sys/stat.h>
#include <dirent.h>

static const char *TAG = "BSP_BOARD";

static i2c_master_bus_handle_t s_i2c_bus = NULL;
static i2c_master_dev_handle_t s_dev_pmic = NULL;
static i2c_master_dev_handle_t s_dev_rtc  = NULL;
static i2c_master_dev_handle_t s_dev_shtc = NULL;
static i2c_master_dev_handle_t s_dev_imu  = NULL;
static SemaphoreHandle_t s_i2c_mtx = NULL;

static sdmmc_card_t *s_sd_card = NULL;
static wl_handle_t   s_wl_hdl  = WL_INVALID_HANDLE;
static tinyusb_msc_storage_handle_t s_msc_hdl = NULL;
static bool s_sd_mounted = false;
static bool s_flash_mounted = false;
static bool s_usb_msc_active = false;
static bool s_pmic_ok = false;
static bool s_imu_ok = false;
static int64_t s_last_shake_us = 0;

static i2c_master_dev_handle_t get_or_add_dev(uint8_t addr)
{
    if (addr == BSP_ADDR_TG28_PMIC && s_dev_pmic) return s_dev_pmic;
    if (addr == BSP_ADDR_PCF85063  && s_dev_rtc)  return s_dev_rtc;
    if (addr == BSP_ADDR_SHTC3     && s_dev_shtc) return s_dev_shtc;
    if (addr == BSP_ADDR_QMI8658   && s_dev_imu)  return s_dev_imu;
    return NULL;
}

i2c_master_bus_handle_t bsp_i2c_get_bus(void)
{
    return s_i2c_bus;
}

esp_err_t bsp_i2c_write_reg(uint8_t addr, uint8_t reg, const uint8_t *data, size_t len)
{
    if (!s_i2c_bus || len > 32) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(s_i2c_mtx, portMAX_DELAY);
    i2c_master_dev_handle_t dev = get_or_add_dev(addr);
    bool temp_dev = false;
    if (!dev) {
        i2c_device_config_t cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = addr,
            .scl_speed_hz = BSP_I2C_FREQ_HZ,
        };
        if (i2c_master_bus_add_device(s_i2c_bus, &cfg, &dev) != ESP_OK) {
            xSemaphoreGive(s_i2c_mtx);
            return ESP_FAIL;
        }
        temp_dev = true;
    }
    uint8_t buf[33];
    buf[0] = reg;
    if (len > 0 && data) {
        memcpy(&buf[1], data, len);
    }
    esp_err_t ret = i2c_master_transmit(dev, buf, len + 1, pdMS_TO_TICKS(200));
    if (temp_dev) {
        i2c_master_bus_rm_device(dev);
    }
    xSemaphoreGive(s_i2c_mtx);
    return ret;
}

esp_err_t bsp_i2c_read_reg(uint8_t addr, uint8_t reg, uint8_t *data, size_t len)
{
    if (!s_i2c_bus || !data || len == 0) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(s_i2c_mtx, portMAX_DELAY);
    i2c_master_dev_handle_t dev = get_or_add_dev(addr);
    bool temp_dev = false;
    if (!dev) {
        i2c_device_config_t cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = addr,
            .scl_speed_hz = BSP_I2C_FREQ_HZ,
        };
        if (i2c_master_bus_add_device(s_i2c_bus, &cfg, &dev) != ESP_OK) {
            xSemaphoreGive(s_i2c_mtx);
            return ESP_FAIL;
        }
        temp_dev = true;
    }
    esp_err_t ret = i2c_master_transmit_receive(dev, &reg, 1, data, len, pdMS_TO_TICKS(200));
    if (temp_dev) {
        i2c_master_bus_rm_device(dev);
    }
    xSemaphoreGive(s_i2c_mtx);
    return ret;
}

static esp_err_t pmic_write_u8(uint8_t reg, uint8_t val)
{
    return bsp_i2c_write_reg(BSP_ADDR_TG28_PMIC, reg, &val, 1);
}

static esp_err_t pmic_read_u8(uint8_t reg, uint8_t *val)
{
    return bsp_i2c_read_reg(BSP_ADDR_TG28_PMIC, reg, val, 1);
}

static void pmic_init_tg28(void)
{
    uint8_t st = 0;
    if (pmic_read_u8(0x00, &st) != ESP_OK) {
        ESP_LOGW(TAG, "TG28/AXP2101 PMIC not detected on I2C");
        s_pmic_ok = false;
        return;
    }
    s_pmic_ok = true;

    /* VBUS input limit: 4.36V (0x06), 1500mA (0x04), VOFF=2.6V (0x00) */
    pmic_write_u8(0x15, 0x06);
    pmic_write_u8(0x16, 0x04);
    pmic_write_u8(0x24, 0x00);

    /* DCDC1 = 3.3V ((3300-1500)/100 = 18 = 0x12) */
    pmic_write_u8(0x82, 0x12);

    /* ALDO1 (Audio Ctrl), ALDO2 (ES8311/Mic AVDD), ALDO3 (e-Paper VCC) = 3.3V ((3300-500)/100 = 28 = 0x1C) */
    pmic_write_u8(0x92, 0x1C);
    pmic_write_u8(0x93, 0x1C);
    pmic_write_u8(0x94, 0x1C);

    /* Enable ALDO1 (bit0), ALDO2 (bit1), ALDO3 (bit2) while preserving existing LDO bits */
    uint8_t ldo_en = 0;
    if (pmic_read_u8(0x90, &ldo_en) == ESP_OK) {
        ldo_en |= 0x07;
        pmic_write_u8(0x90, ldo_en);
    }

    /* POK timing: IRQ=1.5s, OFF=4s, ON=1s -> (01 << 4) | (00 << 2) | 10 = 0x12 */
    pmic_write_u8(0x27, 0x12);

    /* Enable ADC channels: VBAT(bit0), TS(bit1), VBUS(bit2), VSYS(bit3), TDIE(bit4) */
    pmic_write_u8(0x30, 0x1F);
    /* Enable battery detection */
    pmic_write_u8(0x68, 0x01);

    /* Safe charging for kids device: Pre=50mA(0x02), CC=300mA(0x09), Term=25mA+en(0x11), CV=4.2V(0x03) */
    pmic_write_u8(0x61, 0x02);
    pmic_write_u8(0x62, 0x09);
    pmic_write_u8(0x63, 0x11);
    pmic_write_u8(0x64, 0x03);

    /* RTC backup coin-cell trickle charge 3.0V (0x04), Gauge + Cell + Backup charge enable (0x0E) */
    pmic_write_u8(0x6A, 0x04);
    pmic_write_u8(0x18, 0x0E);

    /* Low battery thresholds: warn 10%, shutdown 5% */
    pmic_write_u8(0x1A, (5 << 4) | 5);

    /* Enable POK short-press IRQ (reg 0x41 bit 3) and clear pending IRQ status */
    uint8_t irq1 = 0;
    if (pmic_read_u8(0x41, &irq1) == ESP_OK) {
        pmic_write_u8(0x41, irq1 | 0x08);
    }
    uint8_t clr = 0xFF;
    pmic_write_u8(0x48, clr);
    pmic_write_u8(0x49, clr);
    pmic_write_u8(0x4A, clr);

    ESP_LOGI(TAG, "TG28 PMIC initialized (ALDO1/2/3=3.3V enabled)");
}

void bsp_pmic_epd_power(bool on)
{
    if (!s_pmic_ok) return;
    uint8_t ldo_en = 0;
    if (pmic_read_u8(0x90, &ldo_en) == ESP_OK) {
        if (on) ldo_en |= (1 << 2);
        else    ldo_en &= ~(1 << 2);
        pmic_write_u8(0x90, ldo_en);
    }
}

bool bsp_pmic_poll_pwr_short_press(void)
{
    if (!s_pmic_ok) return false;
    uint8_t st1 = 0;
    if (pmic_read_u8(0x49, &st1) == ESP_OK && (st1 & 0x08)) {
        pmic_write_u8(0x49, 0x08); /* RW1C clear */
        return true;
    }
    return false;
}

void bsp_board_power_off(void)
{
    ESP_LOGI(TAG, "Shutting down InkSprout Kids Console...");
    bsp_audio_shutdown();
    bsp_epd_sleep();
    vTaskDelay(pdMS_TO_TICKS(80));
    if (s_pmic_ok) {
        uint8_t cfg = 0;
        pmic_read_u8(0x10, &cfg);
        pmic_write_u8(0x10, cfg | 0x01); /* Soft PWROFF */
    }
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* === PCF85063 RTC === */
static uint8_t bcd2dec(uint8_t v) { return (uint8_t)((v >> 4) * 10 + (v & 0x0F)); }
static uint8_t dec2bcd(uint8_t v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }

static void rtc_init_pcf85063(void)
{
    uint8_t regs[7] = {0};
    if (bsp_i2c_read_reg(BSP_ADDR_PCF85063, 0x04, regs, 7) != ESP_OK) {
        ESP_LOGW(TAG, "PCF85063 RTC not responding");
        return;
    }
    /* Bit 7 of seconds reg = OS (Oscillator Stop) */
    uint8_t hr = bcd2dec(regs[2] & 0x3F);
    uint8_t mn = bcd2dec(regs[1] & 0x7F);
    if ((regs[0] & 0x80) || hr > 23 || mn > 59) {
        bsp_rtc_time_t def_t = {
            .year = 2026, .month = 11, .day = 18,
            .hour = 9, .minute = 45, .second = 0, .weekday = 3
        };
        bsp_rtc_set_time(&def_t);
        ESP_LOGI(TAG, "PCF85063 RTC initialized with default time");
    }
}

void bsp_rtc_get_time(bsp_rtc_time_t *out)
{
    if (!out) return;
    uint8_t r[7] = {0};
    if (bsp_i2c_read_reg(BSP_ADDR_PCF85063, 0x04, r, 7) == ESP_OK) {
        out->second  = bcd2dec(r[0] & 0x7F);
        out->minute  = bcd2dec(r[1] & 0x7F);
        out->hour    = bcd2dec(r[2] & 0x3F);
        out->day     = bcd2dec(r[3] & 0x3F);
        out->weekday = r[4] & 0x07;
        out->month   = bcd2dec(r[5] & 0x1F);
        out->year    = (uint16_t)(2000 + bcd2dec(r[6]));
    } else {
        out->year = 2026; out->month = 11; out->day = 18;
        out->hour = 9; out->minute = 45; out->second = 0; out->weekday = 3;
    }
}

void bsp_rtc_set_time(const bsp_rtc_time_t *t)
{
    if (!t) return;
    uint8_t ctrl = 0x00;
    bsp_i2c_write_reg(BSP_ADDR_PCF85063, 0x00, &ctrl, 1);
    uint8_t r[7] = {
        dec2bcd(t->second % 60),
        dec2bcd(t->minute % 60),
        dec2bcd(t->hour % 24),
        dec2bcd(t->day ? t->day : 1),
        (uint8_t)(t->weekday % 7),
        dec2bcd(t->month ? t->month : 1),
        dec2bcd((uint8_t)(t->year >= 2000 ? (t->year - 2000) : t->year)),
    };
    bsp_i2c_write_reg(BSP_ADDR_PCF85063, 0x04, r, 7);
}

/* === SHTC3 Temp & Humidity === */
static bool shtc3_read_env(float *temp_c, float *humi_rh)
{
    if (!s_dev_shtc) return false;
    xSemaphoreTake(s_i2c_mtx, portMAX_DELAY);
    uint8_t wake_cmd[2] = {0x35, 0x17};
    uint8_t meas_cmd[2] = {0x78, 0x66}; /* Normal mode, T first, no clock stretching */
    uint8_t sleep_cmd[2] = {0xB0, 0x98};
    uint8_t raw[6] = {0};

    esp_err_t err = i2c_master_transmit(s_dev_shtc, wake_cmd, 2, pdMS_TO_TICKS(100));
    xSemaphoreGive(s_i2c_mtx);
    if (err != ESP_OK) return false;
    vTaskDelay(pdMS_TO_TICKS(2));

    xSemaphoreTake(s_i2c_mtx, portMAX_DELAY);
    err = i2c_master_transmit(s_dev_shtc, meas_cmd, 2, pdMS_TO_TICKS(100));
    xSemaphoreGive(s_i2c_mtx);
    if (err != ESP_OK) return false;

    vTaskDelay(pdMS_TO_TICKS(15));

    xSemaphoreTake(s_i2c_mtx, portMAX_DELAY);
    err = i2c_master_receive(s_dev_shtc, raw, 6, pdMS_TO_TICKS(100));
    i2c_master_transmit(s_dev_shtc, sleep_cmd, 2, pdMS_TO_TICKS(100));
    xSemaphoreGive(s_i2c_mtx);
    if (err != ESP_OK) return false;

    uint16_t raw_t = ((uint16_t)raw[0] << 8) | raw[1];
    uint16_t raw_h = ((uint16_t)raw[3] << 8) | raw[4];
    *temp_c  = -45.0f + 175.0f * ((float)raw_t / 65535.0f);
    *humi_rh = 100.0f * ((float)raw_h / 65535.0f);
    return true;
}

/* === QMI8658A 6-Axis IMU (GPIO40 INT2 only — preserves GPIO39 PA_CTRL!) === */
static void imu_init_qmi8658(void)
{
    gpio_config_t io = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BSP_IMU_INT2_PIN),
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    gpio_config(&io);

    uint8_t who = 0;
    if (bsp_i2c_read_reg(BSP_ADDR_QMI8658, 0x00, &who, 1) != ESP_OK) {
        s_imu_ok = false;
        return;
    }
    uint8_t ctrl1 = 0x60; /* Address auto-increment + little-endian */
    uint8_t ctrl2 = 0x15; /* +/-4g, 250Hz */
    uint8_t ctrl7 = 0x01; /* Enable accelerometer */
    bsp_i2c_write_reg(BSP_ADDR_QMI8658, 0x02, &ctrl1, 1);
    bsp_i2c_write_reg(BSP_ADDR_QMI8658, 0x03, &ctrl2, 1);
    bsp_i2c_write_reg(BSP_ADDR_QMI8658, 0x08, &ctrl7, 1);
    s_imu_ok = true;
    ESP_LOGI(TAG, "QMI8658 IMU initialized (WHO_AM_I=0x%02X)", who);
}

bool bsp_imu_poll_shake(void)
{
    if (!s_imu_ok) return false;
    int64_t now = esp_timer_get_time();
    if (now - s_last_shake_us < 1200000LL) return false;

    uint8_t buf[6] = {0};
    if (bsp_i2c_read_reg(BSP_ADDR_QMI8658, 0x35, buf, 6) != ESP_OK) return false;
    int16_t ax = (int16_t)((buf[1] << 8) | buf[0]);
    int16_t ay = (int16_t)((buf[3] << 8) | buf[2]);
    int16_t az = (int16_t)((buf[5] << 8) | buf[4]);
    /* +/-4g scale -> 8192 LSB/g. Threshold ~1.85g magnitude */
    float gx = (float)ax / 8192.0f;
    float gy = (float)ay / 8192.0f;
    float gz = (float)az / 8192.0f;
    float mag_sq = gx * gx + gy * gy + gz * gz;
    if (mag_sq > 3.45f) {
        s_last_shake_us = now;
        return true;
    }
    return false;
}

/* === Unified Dual-Medium FAT Storage (SDMMC Priority + Internal 3.8MB Flash Fallback) & On-Demand USB MSC === */
#define BSP_FLASH_MOUNT_POINT  "/flash"

static void ensure_stories_folder_and_welcome(const char *root, bool seed_welcome)
{
    char dir_path[64];
    snprintf(dir_path, sizeof(dir_path), "%s/stories", root);
    struct stat st;
    if (stat(dir_path, &st) != 0) {
        mkdir(dir_path, 0777);
    }
    if (seed_welcome) {
        char welcome_file[112];
        snprintf(welcome_file, sizeof(welcome_file), "%s/01_欢迎使用传书工坊.txt", dir_path);
        if (stat(welcome_file, &st) != 0) {
            FILE *fp = fopen(welcome_file, "wb");
            if (fp) {
                const char *msg =
                    "欢迎来到墨芽童伴传书工坊！\n\n"
                    "爸爸妈妈可以通过两种方式把新故事装进机器里：\n"
                    "第一种：在机器进入【USB电脑优盘传书】，用 Type-C 线连接电脑，直接把 .txt 绘本或 16kHz .wav 有声故事拖进优盘的 stories 文件夹！\n\n"
                    "第二种：在机器进入【设备热点扫码传书】，手机扫码连接 InkSprout-Kids 热点，即可在网页里在线写故事、上传 MP3/WAV 音频，或者亲自录下爸爸妈妈讲故事的声音哟！\n";
                fwrite(msg, 1, strlen(msg), fp);
                fclose(fp);
            }
        }
    }
}

bool bsp_sdcard_try_mount(void)
{
    if (s_msc_hdl != NULL) {
        return s_sd_mounted || s_flash_mounted;
    }

    tinyusb_msc_driver_config_t drv_cfg = {
        .user_flags.auto_mount_off = 1,
    };
    tinyusb_msc_install_driver(&drv_cfg);

    /* 1. Probe external 4-bit MicroSD card first */
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;

    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 4;
    slot.clk = BSP_SD_CLK_PIN;
    slot.cmd = BSP_SD_CMD_PIN;
    slot.d0  = BSP_SD_D0_PIN;
    slot.d1  = BSP_SD_D1_PIN;
    slot.d2  = BSP_SD_D2_PIN;
    slot.d3  = BSP_SD_D3_PIN;
    slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    if (sdmmc_host_init() == ESP_OK && sdmmc_host_init_slot(SDMMC_HOST_SLOT_1, &slot) == ESP_OK) {
        s_sd_card = (sdmmc_card_t *)calloc(1, sizeof(sdmmc_card_t));
        if (s_sd_card && sdmmc_card_init(&host, s_sd_card) == ESP_OK) {
            tinyusb_msc_storage_config_t st_cfg = {
                .medium.card = s_sd_card,
                .fat_fs = {
                    .base_path = BSP_SD_MOUNT_POINT,
                    .config = {
                        .max_files = 6,
                        .format_if_mount_failed = false,
                        .allocation_unit_size = 16 * 1024,
                    },
                    .do_not_format = true,
                },
                .mount_point = TINYUSB_MSC_STORAGE_MOUNT_APP,
            };
            if (tinyusb_msc_new_storage_sdmmc(&st_cfg, &s_msc_hdl) == ESP_OK) {
                s_sd_mounted = true;
                ensure_stories_folder_and_welcome(BSP_SD_MOUNT_POINT, false);
                ESP_LOGI(TAG, "MicroSD mounted at %s (USB MSC ready)", BSP_SD_MOUNT_POINT);
                return true;
            }
        }
        free(s_sd_card);
        s_sd_card = NULL;
        sdmmc_host_deinit();
    }

    /* 2. Fallback to internal 3.8MB Wear-Levelled FAT partition ("storage") so USB Drive & Hotspot work without SD card! */
    const esp_partition_t *part = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, "storage");
    if (part && wl_mount(part, &s_wl_hdl) == ESP_OK) {
        tinyusb_msc_storage_config_t st_cfg = {
            .medium.wl_handle = s_wl_hdl,
            .fat_fs = {
                .base_path = BSP_FLASH_MOUNT_POINT,
                .config = {
                    .max_files = 6,
                    .format_if_mount_failed = true,
                    .allocation_unit_size = 4096,
                },
                .do_not_format = false,
            },
            .mount_point = TINYUSB_MSC_STORAGE_MOUNT_APP,
        };
        if (tinyusb_msc_new_storage_spiflash(&st_cfg, &s_msc_hdl) == ESP_OK) {
            s_flash_mounted = true;
            ensure_stories_folder_and_welcome(BSP_FLASH_MOUNT_POINT, true);
            ESP_LOGI(TAG, "Internal 3.8MB Flash FAT mounted at %s (USB MSC ready)", BSP_FLASH_MOUNT_POINT);
            return true;
        }
    }

    ESP_LOGW(TAG, "No external SD or internal FAT storage available");
    return false;
}

bool bsp_sdcard_is_mounted(void)
{
    return s_sd_mounted || s_flash_mounted;
}

const char *bsp_storage_get_root(void)
{
    if (s_sd_mounted) return BSP_SD_MOUNT_POINT;
    return BSP_FLASH_MOUNT_POINT;
}

const char *bsp_storage_get_stories_dir(void)
{
    if (s_sd_mounted) return BSP_SD_MOUNT_POINT "/stories";
    return BSP_FLASH_MOUNT_POINT "/stories";
}

const char *bsp_storage_get_medium_label(void)
{
    if (s_sd_mounted) return "MicroSD 存储卡";
    if (s_flash_mounted) return "内置闪存盘 (免插卡)";
    return "未挂载存储";
}

void bsp_storage_get_space_kb(uint32_t *total_kb, uint32_t *free_kb)
{
    uint64_t tot = 0, fr = 0;
    if (!s_usb_msc_active && bsp_sdcard_is_mounted()) {
        esp_vfs_fat_info(bsp_storage_get_root(), &tot, &fr);
    }
    if (total_kb) *total_kb = (uint32_t)(tot / 1024ULL);
    if (free_kb)  *free_kb  = (uint32_t)(fr / 1024ULL);
}

int bsp_storage_count_stories(void)
{
    if (s_usb_msc_active || !bsp_sdcard_is_mounted()) return 0;
    const char *dirs[2] = { bsp_storage_get_stories_dir(), bsp_storage_get_root() };
    int count = 0;
    for (int d = 0; d < 2; d++) {
        DIR *dir = opendir(dirs[d]);
        if (!dir) continue;
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            if (ent->d_name[0] == '.') continue;
            size_t len = strlen(ent->d_name);
            if ((len > 4 && strcasecmp(ent->d_name + len - 4, ".txt") == 0) ||
                (len > 4 && strcasecmp(ent->d_name + len - 4, ".wav") == 0)) {
                count++;
            }
        }
        closedir(dir);
        if (count > 0) break;
    }
    return count;
}

esp_err_t bsp_usb_msc_start(void)
{
    if (s_usb_msc_active) return ESP_OK;
    if (!s_msc_hdl) return ESP_ERR_INVALID_STATE;

    bsp_audio_stop_all();
    vTaskDelay(pdMS_TO_TICKS(50));

    ESP_ERROR_CHECK_WITHOUT_ABORT(tinyusb_msc_set_storage_mount_point(s_msc_hdl, TINYUSB_MSC_STORAGE_MOUNT_USB));
    const tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();
    esp_err_t ret = tinyusb_driver_install(&tusb_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "tinyusb_driver_install failed: %s", esp_err_to_name(ret));
        tinyusb_msc_set_storage_mount_point(s_msc_hdl, TINYUSB_MSC_STORAGE_MOUNT_APP);
        return ret;
    }
    s_usb_msc_active = true;
    ESP_LOGI(TAG, "USB Mass Storage Drive exposed to host PC");
    return ESP_OK;
}

void bsp_usb_msc_stop(void)
{
    if (!s_usb_msc_active) return;
    tinyusb_driver_uninstall();
    vTaskDelay(pdMS_TO_TICKS(100));
    if (s_msc_hdl) {
        tinyusb_msc_set_storage_mount_point(s_msc_hdl, TINYUSB_MSC_STORAGE_MOUNT_APP);
        ensure_stories_folder_and_welcome(bsp_storage_get_root(), false);
    }
    s_usb_msc_active = false;
    ESP_LOGI(TAG, "USB Mass Storage stopped; storage remounted to app");
}

bool bsp_usb_msc_is_host_connected(void)
{
    return s_usb_msc_active && tud_mounted();
}

void bsp_board_read_status(bsp_status_t *st)
{
    if (!st) return;
    memset(st, 0, sizeof(*st));
    st->battery_percent = 100;
    st->vbat_mv = 4100;
    st->sd_mounted = s_sd_mounted;

    if (s_pmic_ok) {
        uint8_t s1 = 0, s2 = 0, pct = 0, vh = 0, vl = 0;
        if (pmic_read_u8(0x00, &s1) == ESP_OK) {
            st->usb_online = (s1 & 0x20) != 0;
        }
        if (pmic_read_u8(0x01, &s2) == ESP_OK) {
            st->charging = (((s2 >> 5) & 0x03) == 0x01);
        }
        if (pmic_read_u8(0xA4, &pct) == ESP_OK && pct <= 100) {
            st->battery_percent = pct;
        }
        if (pmic_read_u8(0x34, &vh) == ESP_OK && pmic_read_u8(0x35, &vl) == ESP_OK) {
            st->vbat_mv = (((int)(vh & 0x3F) << 8) | vl);
        }
    }
    st->env_valid = shtc3_read_env(&st->temp_c, &st->humi_rh);
}

esp_err_t bsp_board_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    s_i2c_mtx = xSemaphoreCreateMutex();
    i2c_master_bus_config_t bus_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = BSP_I2C_SCL_PIN,
        .sda_io_num = BSP_I2C_SDA_PIN,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &s_i2c_bus));

    i2c_device_config_t dcfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .scl_speed_hz = BSP_I2C_FREQ_HZ,
    };
    dcfg.device_address = BSP_ADDR_TG28_PMIC;
    i2c_master_bus_add_device(s_i2c_bus, &dcfg, &s_dev_pmic);
    dcfg.device_address = BSP_ADDR_PCF85063;
    i2c_master_bus_add_device(s_i2c_bus, &dcfg, &s_dev_rtc);
    dcfg.device_address = BSP_ADDR_SHTC3;
    i2c_master_bus_add_device(s_i2c_bus, &dcfg, &s_dev_shtc);
    dcfg.device_address = BSP_ADDR_QMI8658;
    i2c_master_bus_add_device(s_i2c_bus, &dcfg, &s_dev_imu);

    /* 1. Power rails FIRST so e-Paper (ALDO3) and Audio (ALDO1/2) have 3.3V! */
    pmic_init_tg28();
    vTaskDelay(pdMS_TO_TICKS(30));

    /* 2. RTC, IMU, SD card */
    rtc_init_pcf85063();
    imu_init_qmi8658();
    bsp_sdcard_try_mount();

    /* 3. Thumbwheel + Side Key Input Engine */
    bsp_input_init();

    /* 4. Audio Engine (ES8311 + NS4150B PA on GPIO39) */
    bsp_audio_init();

    /* 5. 3.97" SSD1677 e-Paper SPI3 DMA */
    bsp_epd_init_hw();
    bsp_epd_init_full();

    return ESP_OK;
}
