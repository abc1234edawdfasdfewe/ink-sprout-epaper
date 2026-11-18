#include "bsp_epd.h"
#include "bsp_board.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "BSP_EPD";
static spi_device_handle_t s_spi = NULL;
static bool s_sleeping = false;
static int  s_partial_cnt = 0;

#define EPD_CS_LOW()    gpio_set_level(BSP_EPD_CS_PIN, 0)
#define EPD_CS_HIGH()   gpio_set_level(BSP_EPD_CS_PIN, 1)
#define EPD_DC_CMD()    gpio_set_level(BSP_EPD_DC_PIN, 0)
#define EPD_DC_DATA()   gpio_set_level(BSP_EPD_DC_PIN, 1)
#define EPD_RST_LOW()   gpio_set_level(BSP_EPD_RST_PIN, 0)
#define EPD_RST_HIGH()  gpio_set_level(BSP_EPD_RST_PIN, 1)

static void epd_wait_busy(uint32_t timeout_ms)
{
    int64_t t0 = esp_timer_get_time();
    vTaskDelay(pdMS_TO_TICKS(5));
    while (gpio_get_level(BSP_EPD_BUSY_PIN) != 0) {
        if ((esp_timer_get_time() - t0) > (int64_t)timeout_ms * 1000LL) {
            ESP_LOGW(TAG, "EPD BUSY timeout (%lu ms)", (unsigned long)timeout_ms);
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(8));
    }
}

static void epd_hw_reset(bool fast_reset)
{
    EPD_RST_HIGH();
    vTaskDelay(pdMS_TO_TICKS(fast_reset ? 4 : 15));
    EPD_RST_LOW();
    vTaskDelay(pdMS_TO_TICKS(2));
    EPD_RST_HIGH();
    vTaskDelay(pdMS_TO_TICKS(fast_reset ? 6 : 15));
}

static void epd_cmd(uint8_t cmd)
{
    EPD_DC_CMD();
    EPD_CS_LOW();
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd,
    };
    spi_device_polling_transmit(s_spi, &t);
    EPD_CS_HIGH();
}

static void epd_data(uint8_t d)
{
    EPD_DC_DATA();
    EPD_CS_LOW();
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &d,
    };
    spi_device_polling_transmit(s_spi, &t);
    EPD_CS_HIGH();
}

static void epd_data_buf(const uint8_t *buf, size_t len)
{
    EPD_DC_DATA();
    EPD_CS_LOW();
    const size_t chunk = 4096;
    for (size_t off = 0; off < len; off += chunk) {
        size_t n = (off + chunk > len) ? (len - off) : chunk;
        spi_transaction_t t = {
            .length = n * 8,
            .tx_buffer = buf + off,
        };
        spi_device_polling_transmit(s_spi, &t);
    }
    EPD_CS_HIGH();
}

static void epd_set_full_window(void)
{
    epd_cmd(0x11); /* Data entry mode: Y decrement, X increment */
    epd_data(0x01);

    epd_cmd(0x44); /* RAM X: 0 .. 799 */
    epd_data(0x00);
    epd_data(0x00);
    epd_data((BSP_EPD_WIDTH - 1) & 0xFF);
    epd_data(((BSP_EPD_WIDTH - 1) >> 8) & 0xFF);

    epd_cmd(0x45); /* RAM Y: 479 .. 0 */
    epd_data((BSP_EPD_HEIGHT - 1) & 0xFF);
    epd_data(((BSP_EPD_HEIGHT - 1) >> 8) & 0xFF);
    epd_data(0x00);
    epd_data(0x00);

    epd_cmd(0x4E);
    epd_data(0x00);
    epd_data(0x00);

    epd_cmd(0x4F);
    epd_data((BSP_EPD_HEIGHT - 1) & 0xFF);
    epd_data(((BSP_EPD_HEIGHT - 1) >> 8) & 0xFF);
}

void bsp_epd_init_hw(void)
{
    gpio_config_t out_cfg = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << BSP_EPD_RST_PIN) | (1ULL << BSP_EPD_DC_PIN) | (1ULL << BSP_EPD_CS_PIN),
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    gpio_config(&out_cfg);

    gpio_config_t in_cfg = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BSP_EPD_BUSY_PIN),
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
    };
    gpio_config(&in_cfg);

    EPD_CS_HIGH();
    EPD_RST_HIGH();

    spi_bus_config_t buscfg = {
        .miso_io_num = -1,
        .mosi_io_num = BSP_EPD_MOSI_PIN,
        .sclk_io_num = BSP_EPD_SCLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 16384,
    };
    spi_device_interface_config_t devcfg = {
        .spics_io_num = -1,
        .clock_speed_hz = 20 * 1000 * 1000,
        .mode = 0,
        .queue_size = 1,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    ESP_ERROR_CHECK(spi_bus_add_device(SPI3_HOST, &devcfg, &s_spi));
}

void bsp_epd_init_full(void)
{
    bsp_pmic_epd_power(true);
    vTaskDelay(pdMS_TO_TICKS(15));
    epd_hw_reset(false);
    epd_wait_busy(1000);

    epd_cmd(0x12); /* SWRESET */
    epd_wait_busy(1000);

    epd_cmd(0x18); /* Internal temperature sensor */
    epd_data(0x80);

    epd_cmd(0x0C); /* Booster soft-start */
    epd_data(0xAE);
    epd_data(0xC7);
    epd_data(0xC3);
    epd_data(0xC0);
    epd_data(0x80);

    epd_cmd(0x01); /* Driver output control: 479 gates */
    epd_data((BSP_EPD_HEIGHT - 1) & 0xFF);
    epd_data(((BSP_EPD_HEIGHT - 1) >> 8) & 0xFF);
    epd_data(0x02);

    epd_cmd(0x3C); /* Border waveform */
    epd_data(0x01);

    epd_set_full_window();
    epd_wait_busy(500);
    s_sleeping = false;
}

void bsp_epd_init_fast(void)
{
    bsp_epd_init_full();
    epd_cmd(0x1A); /* Fast LUT temperature threshold (1.5s) */
    epd_data(0x6A);
}

void bsp_epd_present(const uint8_t *fb_mono, epd_refresh_t mode)
{
    if (!fb_mono) return;
    if (s_sleeping) {
        bsp_epd_init_full();
    }

    if (mode == EPD_REFRESH_FULL) {
        bsp_epd_init_full();
        epd_set_full_window();
        epd_cmd(0x24);
        epd_data_buf(fb_mono, BSP_EPD_BUF_SIZE);
        epd_cmd(0x26);
        epd_data_buf(fb_mono, BSP_EPD_BUF_SIZE);
        epd_cmd(0x22);
        epd_data(0xF7);
        epd_cmd(0x20);
        epd_wait_busy(4000);
        s_partial_cnt = 0;
    } else if (mode == EPD_REFRESH_FAST) {
        bsp_epd_init_fast();
        epd_set_full_window();
        epd_cmd(0x24);
        epd_data_buf(fb_mono, BSP_EPD_BUF_SIZE);
        epd_cmd(0x26);
        epd_data_buf(fb_mono, BSP_EPD_BUF_SIZE);
        epd_cmd(0x22);
        epd_data(0xD7);
        epd_cmd(0x20);
        epd_wait_busy(2500);
        s_partial_cnt = 0;
    } else {
        /* Snappy ~300ms Partial Refresh */
        epd_hw_reset(true);
        epd_cmd(0x18);
        epd_data(0x80);
        epd_cmd(0x3C);
        epd_data(0x80);

        epd_cmd(0x01);
        epd_data((BSP_EPD_HEIGHT - 1) & 0xFF);
        epd_data(((BSP_EPD_HEIGHT - 1) >> 8) & 0xFF);
        epd_data(0x02);

        epd_set_full_window();
        epd_cmd(0x24);
        epd_data_buf(fb_mono, BSP_EPD_BUF_SIZE);
        epd_cmd(0x22);
        epd_data(0xFF);
        epd_cmd(0x20);
        epd_wait_busy(1200);
        s_partial_cnt++;
    }
}

void bsp_epd_smart_present(const uint8_t *fb_mono, bool scene_changed)
{
    if (scene_changed || s_partial_cnt >= 18) {
        bsp_epd_present(fb_mono, EPD_REFRESH_FAST);
    } else {
        bsp_epd_present(fb_mono, EPD_REFRESH_PARTIAL);
    }
}

void bsp_epd_sleep(void)
{
    if (s_sleeping) return;
    epd_cmd(0x10);
    epd_data(0x01);
    vTaskDelay(pdMS_TO_TICKS(10));
    EPD_RST_LOW();
    EPD_CS_LOW();
    EPD_DC_CMD();
    bsp_pmic_epd_power(false);
    s_sleeping = true;
}

void bsp_epd_wake(void)
{
    if (s_sleeping) {
        bsp_epd_init_full();
    }
}
