#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "bsp_epd.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GFX_W       BSP_EPD_WIDTH    /* 800 */
#define GFX_H       BSP_EPD_HEIGHT   /* 480 */

typedef enum {
    GFX_WHITE = 0,
    GFX_BLACK = 1,
    GFX_GRAY_LIGHT = 2,  /* 25% halftone dither */
    GFX_GRAY_MID   = 3,  /* 50% checkerboard dither */
} gfx_color_t;

typedef enum {
    FONT_16 = 16,
    FONT_24 = 24,
    FONT_32 = 32,
} gfx_font_t;

typedef enum {
    ART_RABBIT = 0,
    ART_TURTLE,
    ART_TADPOLE_FROG,
    ART_THREE_PIGS,
    ART_ELEPHANT,
    ART_SIMAGUANG,
    ART_MOON_STARS,
    ART_KNIGHT_FOREST,
    ART_SUBMARINE,
    ART_FERRIS_WHEEL,
    ART_CARDS_MAGIC,
    ART_BREAKOUT,
    ART_LOCK_TREASURE,
    ART_PARROT_MIC,
    ART_TROPHY_STARS,
    ART_OWL_GUARD,
    ART_SLEEP_BEAR,
    ART_GACHA_MAGIC,
    ART_ALARM_CLOCK,
    ART_DINO,
    ART_PANDA,
    ART_FOX,
    ART_PENGUIN,
    ART_UNICORN,
    ART_ROBOT,
    ART_SQUIRREL,
    ART_HEDGEHOG,
    ART_BUTTERFLY,
    ART_MONKEY,
    ART_CAT,
    ART_LAPTOP_USB,
    ART_WIFI_PHONE,
    ART_XYLOPHONE,
    ART_TRAIN,
    ART_CAKE_TOWER,
    ART_CONNECT4,
    ART_IDIOM_SCROLL,
    ART_PLANET_TURRET,
    ART_SLIDE_PUZZLE,
    ART_COUNT_MAX
} gfx_art_id_t;

void     gfx_init(void);
uint8_t *gfx_get_fb(void);
void     gfx_set_flipped(bool flip_180);
bool     gfx_is_flipped(void);

void gfx_clear(gfx_color_t c);
void gfx_pixel(int x, int y, gfx_color_t c);
void gfx_hline(int x, int y, int w, gfx_color_t c);
void gfx_vline(int x, int y, int h, gfx_color_t c);
void gfx_line(int x0, int y0, int x1, int y1, int thickness, gfx_color_t c);
void gfx_rect(int x, int y, int w, int h, int border, gfx_color_t c);
void gfx_fill_rect(int x, int y, int w, int h, gfx_color_t c);
void gfx_round_rect(int x, int y, int w, int h, int r, int border, gfx_color_t c);
void gfx_fill_round_rect(int x, int y, int w, int h, int r, gfx_color_t c);
void gfx_circle(int cx, int cy, int r, int border, gfx_color_t c);
void gfx_fill_circle(int cx, int cy, int r, gfx_color_t c);

/* UTF-8 Typography (Full 20,902 CJK + Pinyin + ASCII + Symbols) */
int  gfx_measure_text(const char *utf8, gfx_font_t font);
int  gfx_draw_text(int x, int y, const char *utf8, gfx_font_t font, gfx_color_t fg);
void gfx_draw_text_center(int cx, int y, const char *utf8, gfx_font_t font, gfx_color_t fg);
/* Returns total lines or next page byte offset when max_lines reached */
int  gfx_draw_paragraph(int x, int y, int max_w, int max_h, int line_gap,
                        const char *utf8, gfx_font_t font, gfx_color_t fg,
                        const char **out_next_ptr);

/* Kid-friendly High-Detail Woodcut Illustrations, Clock Digits, Scannable QR Code & UI Chrome */
void gfx_draw_art(int cx, int cy, int scale_pct, gfx_art_id_t art, gfx_color_t fg);
void gfx_draw_clock_digits(int x, int y, const char *time_str);
void gfx_draw_qrcode(int x, int y, int module_px, const char *text);
void gfx_draw_top_bar(const char *title, int stars, int eye_mins_left);
void gfx_draw_bottom_hints(const char *hint_left_right, const char *hint_ok, const char *hint_back);

/* Textbook Hanzi Study: 20,902 Pinyin + 4-Line Rulings + 104px Kaiti Tianzige + Offline Opus Voice */
bool     gfx_hanzi_lookup(uint16_t cp, char *out_pinyin, int py_sz, uint8_t *out_tone, uint16_t *out_syl_id);
uint16_t gfx_hanzi_find_syl_id(const char *pinyin_utf8, uint8_t *out_tone);
bool     gfx_hanzi_get_opus_clip(uint16_t syl_id, const uint8_t **out_data, uint16_t *out_len);
void     gfx_draw_pinyin_4line(int x, int y, int w, const char *pinyin_utf8);
void     gfx_draw_tianzige_hanzi(int x, int y, int box_size, uint16_t cp);

#ifdef __cplusplus
}
#endif
