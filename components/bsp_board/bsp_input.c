#include "bsp_input.h"
#include "bsp_board.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_timer.h"

static QueueHandle_t s_key_q = NULL;
static bool s_invert_wheel = false;

typedef struct {
    int      pin;
    bool     pressed;
    uint32_t ticks_held;
    bool     long_fired;
} btn_state_t;

static btn_state_t s_up   = { .pin = BSP_KEY_WHEEL_UP_PIN };
static btn_state_t s_down = { .pin = BSP_KEY_WHEEL_DOWN_PIN };
static btn_state_t s_ok   = { .pin = BSP_KEY_WHEEL_OK_PIN };
static btn_state_t s_boot = { .pin = BSP_KEY_BOOT_PIN };

static void push_evt(bsp_key_t k)
{
    if (s_key_q && k != KEY_NONE) {
        xQueueSend(s_key_q, &k, 0);
    }
}

/* 10ms (100Hz) deterministic debounce + repeat timer */
static void key_scan_cb(void *arg)
{
    /* 1. Thumbwheel UP/LEFT & DOWN/RIGHT: fire immediately on press + repeat every 160ms after 340ms */
    btn_state_t *wheels[2] = { &s_up, &s_down };
    for (int i = 0; i < 2; i++) {
        btn_state_t *b = wheels[i];
        bool low = (gpio_get_level(b->pin) == 0);
        if (low) {
            b->ticks_held++;
            if (!b->pressed && b->ticks_held >= 2) {
                b->pressed = true;
                bsp_key_t k = (i == 0) ? (s_invert_wheel ? KEY_NEXT : KEY_PREV)
                                       : (s_invert_wheel ? KEY_PREV : KEY_NEXT);
                push_evt(k);
            } else if (b->pressed && b->ticks_held >= 36 && ((b->ticks_held - 36) % 16 == 0)) {
                bsp_key_t k = (i == 0) ? (s_invert_wheel ? KEY_NEXT : KEY_PREV)
                                       : (s_invert_wheel ? KEY_PREV : KEY_NEXT);
                push_evt(k);
            }
        } else {
            b->pressed = false;
            b->ticks_held = 0;
        }
    }

    /* 2. Thumbwheel Center (OK): short click = KEY_OK, long press (750ms) = KEY_BACK */
    bool ok_low = (gpio_get_level(s_ok.pin) == 0);
    if (ok_low) {
        s_ok.ticks_held++;
        if (!s_ok.pressed && s_ok.ticks_held >= 2) {
            s_ok.pressed = true;
            s_ok.long_fired = false;
        } else if (s_ok.pressed && !s_ok.long_fired && s_ok.ticks_held >= 75) {
            s_ok.long_fired = true;
            push_evt(KEY_BACK);
        }
    } else {
        if (s_ok.pressed && !s_ok.long_fired) {
            push_evt(KEY_OK);
        }
        s_ok.pressed = false;
        s_ok.ticks_held = 0;
        s_ok.long_fired = false;
    }

    /* 3. Side BOOT button: immediate KEY_BACK on press */
    bool boot_low = (gpio_get_level(s_boot.pin) == 0);
    if (boot_low) {
        s_boot.ticks_held++;
        if (!s_boot.pressed && s_boot.ticks_held >= 2) {
            s_boot.pressed = true;
            push_evt(KEY_BACK);
        }
    } else {
        s_boot.pressed = false;
        s_boot.ticks_held = 0;
    }
}

void bsp_input_init(void)
{
    s_key_q = xQueueCreate(16, sizeof(bsp_key_t));
    gpio_config_t cfg = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BSP_KEY_WHEEL_UP_PIN) |
                        (1ULL << BSP_KEY_WHEEL_OK_PIN) |
                        (1ULL << BSP_KEY_WHEEL_DOWN_PIN) |
                        (1ULL << BSP_KEY_BOOT_PIN),
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    gpio_config(&cfg);

    const esp_timer_create_args_t targs = {
        .callback = &key_scan_cb,
        .name = "kid_wheel",
    };
    esp_timer_handle_t timer = NULL;
    esp_timer_create(&targs, &timer);
    esp_timer_start_periodic(timer, 10 * 1000); /* 10ms */
}

void bsp_input_set_invert_wheel(bool invert) { s_invert_wheel = invert; }
bool bsp_input_get_invert_wheel(void)        { return s_invert_wheel; }

void bsp_input_flush(void)
{
    if (s_key_q) xQueueReset(s_key_q);
}

bsp_key_t bsp_input_wait(uint32_t timeout_ms)
{
    if (!s_key_q) return KEY_NONE;
    bsp_key_t k = KEY_NONE;
    uint32_t waited = 0;
    const uint32_t step = 80;
    while (waited <= timeout_ms) {
        uint32_t slice = (timeout_ms - waited > step) ? step : (timeout_ms - waited);
        if (xQueueReceive(s_key_q, &k, pdMS_TO_TICKS(slice)) == pdTRUE) {
            return k;
        }
        if (bsp_pmic_poll_pwr_short_press()) return KEY_PWR_SHORT;
        if (bsp_imu_poll_shake())            return KEY_SHAKE;
        if (timeout_ms == 0) break;
        waited += slice;
        if (slice == 0) break;
    }
    return KEY_NONE;
}

bool bsp_input_wheel_prev_held(void)
{
    int pin = s_invert_wheel ? BSP_KEY_WHEEL_DOWN_PIN : BSP_KEY_WHEEL_UP_PIN;
    return gpio_get_level(pin) == 0;
}

bool bsp_input_wheel_next_held(void)
{
    int pin = s_invert_wheel ? BSP_KEY_WHEEL_UP_PIN : BSP_KEY_WHEEL_DOWN_PIN;
    return gpio_get_level(pin) == 0;
}
