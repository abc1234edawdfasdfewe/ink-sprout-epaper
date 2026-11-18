#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_EPD_WIDTH       800
#define BSP_EPD_HEIGHT      480
#define BSP_EPD_BUF_SIZE    ((BSP_EPD_WIDTH / 8) * BSP_EPD_HEIGHT) /* 48,000 bytes */

typedef enum {
    EPD_REFRESH_PARTIAL = 0,  /* ~300ms zero-flash update for thumbwheel & games */
    EPD_REFRESH_FAST    = 1,  /* ~1.2s clean page transition */
    EPD_REFRESH_FULL    = 2,  /* ~2.5s full ghosting scrub */
} epd_refresh_t;

void bsp_epd_init_hw(void);
void bsp_epd_init_full(void);
void bsp_epd_init_fast(void);

void bsp_epd_present(const uint8_t *fb_mono, epd_refresh_t mode);
void bsp_epd_smart_present(const uint8_t *fb_mono, bool scene_changed);
void bsp_epd_sleep(void);
void bsp_epd_wake(void);

#ifdef __cplusplus
}
#endif
