#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    KEY_NONE = 0,
    KEY_PREV,       /* Thumbwheel UP / LEFT (instant on press + smooth repeat) */
    KEY_NEXT,       /* Thumbwheel DOWN / RIGHT (instant on press + smooth repeat) */
    KEY_OK,         /* Thumbwheel center click */
    KEY_BACK,       /* Side BOOT key click OR Thumbwheel center long-press (0.75s) */
    KEY_PWR_SHORT,  /* Side PWR key short press */
    KEY_SHAKE,      /* QMI8658 gentle shake gesture */
} bsp_key_t;

void      bsp_input_init(void);
void      bsp_input_set_invert_wheel(bool invert);
bool      bsp_input_get_invert_wheel(void);
void      bsp_input_flush(void);
bsp_key_t bsp_input_wait(uint32_t timeout_ms);
/* Direct live state for real-time action games (Submarine / Breakout) */
bool      bsp_input_wheel_prev_held(void);
bool      bsp_input_wheel_next_held(void);

#ifdef __cplusplus
}
#endif
