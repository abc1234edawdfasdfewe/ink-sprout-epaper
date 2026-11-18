#include "bsp_gfx.h"
#include "bsp_board.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "esp_heap_caps.h"
#include "esp_log.h"

static const char *TAG = "BSP_GFX";

extern const uint8_t kidfont_pack_bin_start[] asm("_binary_kidfont_pack_bin_start");
extern const uint8_t kidfont_pack_bin_end[]   asm("_binary_kidfont_pack_bin_end");
extern const uint8_t hanzi_study_pack_bin_start[] asm("_binary_hanzi_study_pack_bin_start");
extern const uint8_t hanzi_study_pack_bin_end[]   asm("_binary_hanzi_study_pack_bin_end");
extern const uint8_t story_art_pack_bin_start[]   asm("_binary_story_art_pack_bin_start");
extern const uint8_t story_art_pack_bin_end[]     asm("_binary_story_art_pack_bin_end");

typedef struct __attribute__((packed)) {
    char     magic[4];
    uint16_t art_count;
    uint16_t clk_w;
    uint16_t clk_h;
    uint16_t reserved0;
    uint32_t off_art64;
    uint32_t off_art96;
    uint32_t off_art176;
    uint32_t off_clk;
    uint8_t  reserved[4];
} art_pack_hdr_t;

typedef struct __attribute__((packed)) {
    char     magic[4];
    uint16_t syl_count;
    uint16_t py_glyph_count;
    uint16_t kaiti_count;
    uint16_t kaiti_dim;
    uint32_t off_syl_meta;
    uint32_t off_cjk_syl_idx;
    uint32_t off_py_cps;
    uint32_t off_py_advs;
    uint32_t off_py_bmps;
    uint32_t off_kaiti_cps;
    uint32_t off_kaiti_bmps;
    uint32_t off_opus_blob;
    uint8_t  reserved[20];
} hz_pack_hdr_t;

typedef struct __attribute__((packed)) {
    char     pinyin[12];
    uint8_t  tone;
    uint8_t  pad;
    uint32_t audio_off;
    uint16_t audio_bytes;
} hz_syl_meta_t;

typedef struct __attribute__((packed)) {
    char     magic[4];
    uint16_t sym_count;
    uint16_t t32_count;
    uint32_t off_ascii16;
    uint32_t off_ascii24;
    uint32_t off_ascii32;
    uint32_t off_sym_idx;
    uint32_t off_sym16;
    uint32_t off_sym24;
    uint32_t off_t32_idx;
    uint32_t off_t32_bmp;
    uint32_t off_cjk16;
    uint32_t off_cjk24;
    uint8_t  reserved[16];
} kidfont_hdr_t;

static uint8_t *s_fb = NULL;
static bool s_flip_180 = false;
static const kidfont_hdr_t *s_fhdr = NULL;
static const hz_pack_hdr_t *s_hzhdr = NULL;

void gfx_init(void)
{
    if (!s_fb) {
        s_fb = (uint8_t *)heap_caps_malloc(BSP_EPD_BUF_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!s_fb) {
            s_fb = (uint8_t *)malloc(BSP_EPD_BUF_SIZE);
        }
        memset(s_fb, 0xFF, BSP_EPD_BUF_SIZE);
    }
    s_fhdr = (const kidfont_hdr_t *)kidfont_pack_bin_start;
    if (memcmp(s_fhdr->magic, "KIDF", 4) != 0) {
        ESP_LOGE(TAG, "Invalid kidfont_pack header!");
        s_fhdr = NULL;
    } else {
        ESP_LOGI(TAG, "KidFont loaded: syms=%u, t32=%u, total=%u KB",
                 s_fhdr->sym_count, s_fhdr->t32_count,
                 (unsigned)((kidfont_pack_bin_end - kidfont_pack_bin_start) / 1024));
    }

    s_hzhdr = (const hz_pack_hdr_t *)hanzi_study_pack_bin_start;
    if (memcmp(s_hzhdr->magic, "HZPK", 4) != 0) {
        ESP_LOGE(TAG, "Invalid hanzi_study_pack header!");
        s_hzhdr = NULL;
    } else {
        ESP_LOGI(TAG, "HanziPack loaded: syls=%u, kaiti=%u (%ux%u), total=%u KB",
                 s_hzhdr->syl_count, s_hzhdr->kaiti_count, s_hzhdr->kaiti_dim, s_hzhdr->kaiti_dim,
                 (unsigned)((hanzi_study_pack_bin_end - hanzi_study_pack_bin_start) / 1024));
    }
}

uint8_t *gfx_get_fb(void) { return s_fb; }
void gfx_set_flipped(bool flip_180) { s_flip_180 = flip_180; }
bool gfx_is_flipped(void) { return s_flip_180; }

void gfx_clear(gfx_color_t c)
{
    if (!s_fb) return;
    if (c == GFX_WHITE) {
        memset(s_fb, 0xFF, BSP_EPD_BUF_SIZE);
    } else if (c == GFX_BLACK) {
        memset(s_fb, 0x00, BSP_EPD_BUF_SIZE);
    } else {
        gfx_fill_rect(0, 0, GFX_W, GFX_H, c);
    }
}

void gfx_pixel(int x, int y, gfx_color_t c)
{
    if (!s_fb || (unsigned)x >= GFX_W || (unsigned)y >= GFX_H) return;
    bool black = false;
    if (c == GFX_BLACK) {
        black = true;
    } else if (c == GFX_WHITE) {
        black = false;
    } else if (c == GFX_GRAY_MID) {
        black = (((x + y) & 1) == 0);
    } else if (c == GFX_GRAY_LIGHT) {
        black = (((x & 1) == 0) && ((y & 1) == 0));
    }

    /* Physical SSD1677 panel orientation in landscape (top edge = U4 thumbwheel):
       X_ram=0 is physical right, Y_ram=0 is physical top.
       Standard 0°:   rx = (GFX_W - 1 - x), ry = (GFX_H - 1 - y)
       Rotated 180°:  rx = x,               ry = y */
    int rx = s_flip_180 ? x : (GFX_W - 1 - x);
    int ry = s_flip_180 ? y : (GFX_H - 1 - y);
    size_t idx = (size_t)ry * (GFX_W / 8) + (size_t)(rx >> 3);
    uint8_t mask = (uint8_t)(0x80 >> (rx & 7));
    if (black) s_fb[idx] &= ~mask;
    else       s_fb[idx] |= mask;
}

void gfx_hline(int x, int y, int w, gfx_color_t c)
{
    for (int i = 0; i < w; i++) gfx_pixel(x + i, y, c);
}

void gfx_vline(int x, int y, int h, gfx_color_t c)
{
    for (int i = 0; i < h; i++) gfx_pixel(x, y + i, c);
}

void gfx_fill_rect(int x, int y, int w, int h, gfx_color_t c)
{
    int x2 = x + w, y2 = y + h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x2 > GFX_W) x2 = GFX_W;
    if (y2 > GFX_H) y2 = GFX_H;
    for (int py = y; py < y2; py++) {
        for (int px = x; px < x2; px++) {
            gfx_pixel(px, py, c);
        }
    }
}

void gfx_rect(int x, int y, int w, int h, int border, gfx_color_t c)
{
    for (int b = 0; b < border; b++) {
        gfx_hline(x + b, y + b, w - 2 * b, c);
        gfx_hline(x + b, y + h - 1 - b, w - 2 * b, c);
        gfx_vline(x + b, y + b, h - 2 * b, c);
        gfx_vline(x + w - 1 - b, y + b, h - 2 * b, c);
    }
}

void gfx_fill_circle(int cx, int cy, int r, gfx_color_t c)
{
    int r2 = r * r;
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            if (dx * dx + dy * dy <= r2) {
                gfx_pixel(cx + dx, cy + dy, c);
            }
        }
    }
}

void gfx_circle(int cx, int cy, int r, int border, gfx_color_t c)
{
    int ro2 = r * r;
    int ri = (r > border) ? (r - border) : 0;
    int ri2 = ri * ri;
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            int d2 = dx * dx + dy * dy;
            if (d2 <= ro2 && d2 >= ri2) {
                gfx_pixel(cx + dx, cy + dy, c);
            }
        }
    }
}

void gfx_fill_round_rect(int x, int y, int w, int h, int r, gfx_color_t c)
{
    if (r <= 0) { gfx_fill_rect(x, y, w, h, c); return; }
    if (2 * r > w) r = w / 2;
    if (2 * r > h) r = h / 2;
    gfx_fill_rect(x + r, y, w - 2 * r, h, c);
    gfx_fill_rect(x, y + r, r, h - 2 * r, c);
    gfx_fill_rect(x + w - r, y + r, r, h - 2 * r, c);
    int r2 = r * r;
    for (int dy = 0; dy < r; dy++) {
        for (int dx = 0; dx < r; dx++) {
            int ox = r - 1 - dx, oy = r - 1 - dy;
            if (ox * ox + oy * oy <= r2) {
                gfx_pixel(x + dx, y + dy, c);
                gfx_pixel(x + w - 1 - dx, y + dy, c);
                gfx_pixel(x + dx, y + h - 1 - dy, c);
                gfx_pixel(x + w - 1 - dx, y + h - 1 - dy, c);
            }
        }
    }
}

void gfx_round_rect(int x, int y, int w, int h, int r, int border, gfx_color_t c)
{
    for (int b = 0; b < border; b++) {
        int bx = x + b, by = y + b, bw = w - 2 * b, bh = h - 2 * b;
        int br = (r > b) ? (r - b) : 0;
        gfx_hline(bx + br, by, bw - 2 * br, c);
        gfx_hline(bx + br, by + bh - 1, bw - 2 * br, c);
        gfx_vline(bx, by + br, bh - 2 * br, c);
        gfx_vline(bx + bw - 1, by + br, bh - 2 * br, c);
        if (br > 0) {
            int x0 = 0, y0 = br, d = 3 - 2 * br;
            while (y0 >= x0) {
                gfx_pixel(bx + br - x0, by + br - y0, c);
                gfx_pixel(bx + br - y0, by + br - x0, c);
                gfx_pixel(bx + bw - 1 - br + x0, by + br - y0, c);
                gfx_pixel(bx + bw - 1 - br + y0, by + br - x0, c);
                gfx_pixel(bx + br - x0, by + bh - 1 - br + y0, c);
                gfx_pixel(bx + br - y0, by + bh - 1 - br + x0, c);
                gfx_pixel(bx + bw - 1 - br + x0, by + bh - 1 - br + y0, c);
                gfx_pixel(bx + bw - 1 - br + y0, by + bh - 1 - br + x0, c);
                if (d < 0) d += 4 * x0 + 6;
                else { d += 4 * (x0 - y0) + 10; y0--; }
                x0++;
            }
        }
    }
}

void gfx_line(int x0, int y0, int x1, int y1, int thickness, gfx_color_t c)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    int r = thickness / 2;
    while (1) {
        if (r <= 0) gfx_pixel(x0, y0, c);
        else gfx_fill_circle(x0, y0, r, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/* === UTF-8 Decoding & Font Lookup === */
static uint32_t utf8_next(const char **p)
{
    const uint8_t *s = (const uint8_t *)(*p);
    if (*s == 0) return 0;
    if (*s < 0x80) { *p += 1; return *s; }
    if ((*s & 0xE0) == 0xC0 && s[1]) {
        *p += 2;
        return ((uint32_t)(s[0] & 0x1F) << 6) | (s[1] & 0x3F);
    }
    if ((*s & 0xF0) == 0xE0 && s[1] && s[2]) {
        *p += 3;
        return ((uint32_t)(s[0] & 0x0F) << 12) | ((uint32_t)(s[1] & 0x3F) << 6) | (s[2] & 0x3F);
    }
    if ((*s & 0xF8) == 0xF0 && s[1] && s[2] && s[3]) {
        *p += 4;
        return ((uint32_t)(s[0] & 0x07) << 18) | ((uint32_t)(s[1] & 0x3F) << 12) |
               ((uint32_t)(s[2] & 0x3F) << 6) | (s[3] & 0x3F);
    }
    *p += 1;
    return '?';
}

static int bsearch_u16(const uint16_t *arr, int n, uint16_t key)
{
    int lo = 0, hi = n - 1;
    while (lo <= hi) {
        int mid = (lo + hi) >> 1;
        if (arr[mid] == key) return mid;
        if (arr[mid] < key) lo = mid + 1;
        else hi = mid - 1;
    }
    return -1;
}

static void blit_1bpp(int x, int y, int w, int h, const uint8_t *bmp, int scale, gfx_color_t fg)
{
    int row_bytes = (w + 7) / 8;
    for (int ry = 0; ry < h; ry++) {
        for (int rx = 0; rx < w; rx++) {
            if (bmp[ry * row_bytes + (rx >> 3)] & (0x80 >> (rx & 7))) {
                if (scale == 1) {
                    gfx_pixel(x + rx, y + ry, fg);
                } else {
                    for (int sy = 0; sy < scale; sy++)
                        for (int sx = 0; sx < scale; sx++)
                            gfx_pixel(x + rx * scale + sx, y + ry * scale + sy, fg);
                }
            }
        }
    }
}

static int glyph_advance(uint32_t cp, gfx_font_t font)
{
    bool half = (cp >= 0x20 && cp <= 0x7E);
    /* Pinyin vowels are compact ~half+ width in UI, treat as 3/4 or full width? Wait: in gen_kidfont we rendered sym at 16x16 / 24x24,
       for Latin pinyin vowels let's give natural advance */
    if (cp >= 0x00C0 && cp <= 0x024F) {
        return (font == FONT_16) ? 10 : (font == FONT_24 ? 14 : 20);
    }
    if (half) {
        return (font == FONT_16) ? 8 : (font == FONT_24 ? 12 : 16);
    }
    return (int)font;
}

static int draw_glyph(int x, int y, uint32_t cp, gfx_font_t font, gfx_color_t fg)
{
    if (!s_fhdr) return (int)font / 2;
    const uint8_t *base = kidfont_pack_bin_start;
    int adv = glyph_advance(cp, font);

    if (cp == ' ') return adv;

    if (cp >= 0x20 && cp <= 0x7E) {
        uint32_t idx = cp - 0x20;
        if (font == FONT_16) {
            blit_1bpp(x, y, 8, 16, base + s_fhdr->off_ascii16 + idx * 16, 1, fg);
        } else if (font == FONT_24) {
            blit_1bpp(x, y, 12, 24, base + s_fhdr->off_ascii24 + idx * 48, 1, fg);
        } else {
            blit_1bpp(x, y, 16, 32, base + s_fhdr->off_ascii32 + idx * 64, 1, fg);
        }
        return adv;
    }

    if (font == FONT_32 && cp <= 0xFFFF) {
        const uint16_t *t32_idx = (const uint16_t *)(base + s_fhdr->off_t32_idx);
        int pos = bsearch_u16(t32_idx, s_fhdr->t32_count, (uint16_t)cp);
        if (pos >= 0) {
            blit_1bpp(x, y, 32, 32, base + s_fhdr->off_t32_bmp + (size_t)pos * 128, 1, fg);
            return adv;
        }
        /* Fallback to crisp 2x of 16x16 glyph */
        if (cp >= 0x4E00 && cp <= 0x9FA5) {
            blit_1bpp(x, y, 16, 16, base + s_fhdr->off_cjk16 + (size_t)(cp - 0x4E00) * 32, 2, fg);
            return adv;
        }
    }

    if (cp >= 0x4E00 && cp <= 0x9FA5) {
        size_t idx = (size_t)(cp - 0x4E00);
        if (font == FONT_16) {
            blit_1bpp(x, y, 16, 16, base + s_fhdr->off_cjk16 + idx * 32, 1, fg);
        } else {
            blit_1bpp(x, y, 24, 24, base + s_fhdr->off_cjk24 + idx * 72, 1, fg);
        }
        return adv;
    }

    if (cp <= 0xFFFF) {
        const uint16_t *sym_idx = (const uint16_t *)(base + s_fhdr->off_sym_idx);
        int pos = bsearch_u16(sym_idx, s_fhdr->sym_count, (uint16_t)cp);
        if (pos >= 0) {
            int x_off = (cp >= 0x00C0 && cp <= 0x024F) ? -3 : 0;
            if (font == FONT_16) {
                blit_1bpp(x + x_off, y, 16, 16, base + s_fhdr->off_sym16 + (size_t)pos * 32, 1, fg);
            } else if (font == FONT_24) {
                blit_1bpp(x + x_off, y, 24, 24, base + s_fhdr->off_sym24 + (size_t)pos * 72, 1, fg);
            } else {
                blit_1bpp(x + x_off, y, 16, 16, base + s_fhdr->off_sym16 + (size_t)pos * 32, 2, fg);
            }
            return adv;
        }
    }
    return adv;
}

int gfx_measure_text(const char *utf8, gfx_font_t font)
{
    if (!utf8) return 0;
    int w = 0;
    const char *p = utf8;
    while (*p) {
        uint32_t cp = utf8_next(&p);
        if (cp == '\n' || cp == '\r') break;
        w += glyph_advance(cp, font);
    }
    return w;
}

int gfx_draw_text(int x, int y, const char *utf8, gfx_font_t font, gfx_color_t fg)
{
    if (!utf8) return x;
    const char *p = utf8;
    int cx = x;
    while (*p) {
        uint32_t cp = utf8_next(&p);
        if (cp == '\n' || cp == '\r') break;
        cx += draw_glyph(cx, y, cp, font, fg);
    }
    return cx;
}

void gfx_draw_text_center(int cx, int y, const char *utf8, gfx_font_t font, gfx_color_t fg)
{
    int w = gfx_measure_text(utf8, font);
    gfx_draw_text(cx - w / 2, y, utf8, font, fg);
}

int gfx_draw_paragraph(int x, int y, int max_w, int max_h, int line_gap,
                       const char *utf8, gfx_font_t font, gfx_color_t fg,
                       const char **out_next_ptr)
{
    if (!utf8) {
        if (out_next_ptr) *out_next_ptr = NULL;
        return 0;
    }
    const char *p = utf8;
    /* Skip UTF-8 BOM if present on SD card files */
    if ((uint8_t)p[0] == 0xEF && (uint8_t)p[1] == 0xBB && (uint8_t)p[2] == 0xBF) {
        p += 3;
    }
    int cx = x, cy = y;
    int line_h = (int)font + line_gap;
    int lines = 0;

    while (*p) {
        if (cy + (int)font > y + max_h) {
            if (out_next_ptr) *out_next_ptr = p;
            return lines;
        }
        const char *prev_p = p;
        uint32_t cp = utf8_next(&p);
        if (cp == '\r') continue;
        if (cp == '\n') {
            cx = x;
            cy += line_h;
            lines++;
            continue;
        }
        int adv = glyph_advance(cp, font);
        /* Avoid starting a line with closing CJK punctuation if possible */
        if (cx + adv > x + max_w && cx > x) {
            cx = x;
            cy += line_h;
            lines++;
            if (cy + (int)font > y + max_h) {
                if (out_next_ptr) *out_next_ptr = prev_p;
                return lines;
            }
        }
        draw_glyph(cx, cy, cp, font, fg);
        cx += adv;
    }
    if (out_next_ptr) *out_next_ptr = NULL;
    return lines + (cx > x ? 1 : 0);
}

/* === Refined 3-Size Woodcut Storybook & System Illustrations (story_art_pack.bin) === */
void gfx_draw_art(int cx, int cy, int scale_pct, gfx_art_id_t art, gfx_color_t fg)
{
    const art_pack_hdr_t *ah = (const art_pack_hdr_t *)story_art_pack_bin_start;
    if (!ah || memcmp(ah->magic, "ARTP", 4) != 0) return;

    int idx = (int)art;
    if (idx < 0 || idx >= (int)ah->art_count) idx = (int)ART_SLEEP_BEAR;

    int dim = 96;
    uint32_t sec_off = ah->off_art96;
    if (scale_pct <= 74) {
        dim = 64;
        sec_off = ah->off_art64;
    } else if (scale_pct > 122) {
        dim = 176;
        sec_off = ah->off_art176;
    }

    size_t stride = (size_t)((dim + 7) / 8) * (size_t)dim;
    const uint8_t *bmp = story_art_pack_bin_start + sec_off + (size_t)idx * stride;
    blit_1bpp(cx - dim / 2, cy - dim / 2, dim, dim, bmp, 1, fg);
}

void gfx_draw_clock_digits(int x, int y, const char *time_str)
{
    const art_pack_hdr_t *ah = (const art_pack_hdr_t *)story_art_pack_bin_start;
    if (!ah || memcmp(ah->magic, "ARTP", 4) != 0 || !time_str) return;

    int w = (int)ah->clk_w; /* 40 */
    int h = (int)ah->clk_h; /* 68 */
    size_t stride = (size_t)((w + 7) / 8) * (size_t)h;
    const uint8_t *base = story_art_pack_bin_start + ah->off_clk;

    int cx = x;
    for (size_t i = 0; time_str[i]; i++) {
        char c = time_str[i];
        if (c == ' ') {
            cx += 14;
            continue;
        }
        int gi = -1;
        if (c >= '0' && c <= '9') gi = c - '0';
        else if (c == ':')        gi = 10;
        if (gi >= 0) {
            blit_1bpp(cx, y, w, h, base + (size_t)gi * stride, 1, GFX_BLACK);
            cx += (c == ':') ? (w - 10) : (w + 2);
        }
    }
}

void gfx_draw_top_bar(const char *title, int stars, int eye_mins_left)
{
    bsp_status_t st;
    bsp_rtc_time_t tm;
    bsp_board_read_status(&st);
    bsp_rtc_get_time(&tm);

    gfx_fill_rect(0, 0, GFX_W, 42, GFX_BLACK);

    const char *ttl = title ? title : "★ 墨芽童伴";
    int full_tw = gfx_measure_text(ttl, FONT_24);

    /* Right-aligned status cluster adapts its verbosity to title width so text NEVER overlaps */
    char status_s[112];
    if (full_tw <= 290 && st.env_valid) {
        if (eye_mins_left > 0) {
            snprintf(status_s, sizeof(status_s), "★%d   %.0f℃ %.0f%%   护眼%dm   %02u:%02u  %s%d%%",
                     stars, st.temp_c, st.humi_rh, eye_mins_left,
                     tm.hour, tm.minute, st.charging ? "⚡" : "", st.battery_percent);
        } else {
            snprintf(status_s, sizeof(status_s), "★%d   %.0f℃ %.0f%%   %02u:%02u  %s%d%%",
                     stars, st.temp_c, st.humi_rh,
                     tm.hour, tm.minute, st.charging ? "⚡" : "", st.battery_percent);
        }
    } else if (full_tw <= 440 && st.env_valid) {
        snprintf(status_s, sizeof(status_s), "★%d  %.0f℃ %.0f%%  %02u:%02u %s%d%%",
                 stars, st.temp_c, st.humi_rh,
                 tm.hour, tm.minute, st.charging ? "⚡" : "", st.battery_percent);
    } else {
        snprintf(status_s, sizeof(status_s), "★%d  %02u:%02u %s%d%%",
                 stars, tm.hour, tm.minute, st.charging ? "⚡" : "", st.battery_percent);
    }

    int sw = gfx_measure_text(status_s, FONT_16);
    int sx = GFX_W - sw - 14;
    gfx_draw_text(sx, 13, status_s, FONT_16, GFX_WHITE);

    /* Draw title on the left, strictly clipped before (sx - 18) to guarantee 0 overlap */
    int max_title_right = sx - 18;
    if (16 + full_tw <= max_title_right) {
        gfx_draw_text(16, 9, ttl, FONT_24, GFX_WHITE);
    } else {
        const char *p = ttl;
        int cx = 16;
        while (*p) {
            uint32_t cp = utf8_next(&p);
            if (cp == '\n' || cp == '\r') break;
            int adv = glyph_advance(cp, FONT_24);
            if (cx + adv + 24 > max_title_right) {
                draw_glyph(cx, 9, 0x2026 /* … */, FONT_24, GFX_WHITE);
                break;
            }
            cx += draw_glyph(cx, 9, cp, FONT_24, GFX_WHITE);
        }
    }
}

void gfx_draw_bottom_hints(const char *hint_left_right, const char *hint_ok, const char *hint_back)
{
    gfx_fill_rect(0, GFX_H - 38, GFX_W, 38, GFX_GRAY_LIGHT);
    gfx_hline(0, GFX_H - 38, GFX_W, GFX_BLACK);
    gfx_hline(0, GFX_H - 37, GFX_W, GFX_BLACK);

    char bar[192];
    snprintf(bar, sizeof(bar), "◀▶ %s     ● %s     ↺ %s",
             hint_left_right ? hint_left_right : "滚轮选择",
             hint_ok ? hint_ok : "按下确认",
             hint_back ? hint_back : "侧键/长按返回");
    gfx_draw_text_center(GFX_W / 2, GFX_H - 27, bar, FONT_16, GFX_BLACK);
}

/* === Self-Contained ISO/IEC 18004 QR Code Version 3-L (29x29, up to 53 bytes) === */
static uint8_t gf256_mul(uint8_t a, uint8_t b)
{
    uint8_t p = 0;
    for (int i = 0; i < 8; i++) {
        if (b & 1) p ^= a;
        bool hi = (a & 0x80) != 0;
        a <<= 1;
        if (hi) a ^= 0x1D;
        b >>= 1;
    }
    return p;
}

void gfx_draw_qrcode(int x, int y, int module_px, const char *text)
{
    if (!text || module_px < 2) return;
    size_t len = strlen(text);
    if (len > 53) len = 53;

    /* 1. Encode 55 data codewords (Byte mode = 0100) + 15 Reed-Solomon EC codewords */
    uint8_t cw[70] = {0};
    int bit_pos = 0;
    #define PUT_BITS(val, nbits) do { \
        for (int _b = (nbits) - 1; _b >= 0; _b--) { \
            if (((val) >> _b) & 1) cw[bit_pos >> 3] |= (0x80 >> (bit_pos & 7)); \
            bit_pos++; \
        } \
    } while (0)

    PUT_BITS(0x4, 4);        /* Byte mode indicator */
    PUT_BITS((int)len, 8);   /* Character count */
    for (size_t i = 0; i < len; i++) PUT_BITS((uint8_t)text[i], 8);
    PUT_BITS(0, 4);          /* Terminator */
    while (bit_pos & 7) bit_pos++;
    const uint8_t pad_bytes[2] = { 0xEC, 0x11 };
    for (int p = 0; (bit_pos >> 3) < 55; p++) {
        cw[bit_pos >> 3] = pad_bytes[p & 1];
        bit_pos += 8;
    }
    #undef PUT_BITS

    /* Generator polynomial prod_{i=0..14}(x - 2^i) over GF(256) */
    uint8_t gen[16] = {0};
    gen[0] = 1;
    uint8_t root = 1;
    for (int i = 0; i < 15; i++) {
        for (int j = i + 1; j >= 1; j--) {
            gen[j] = gen[j - 1] ^ gf256_mul(gen[j], root);
        }
        gen[0] = gf256_mul(gen[0], root);
        root = gf256_mul(root, 2);
    }
    /* gen[15] is x^15 coeff (1), gen[14..0] are lower terms; compute remainder of cw[0..54]*x^15 / gen */
    uint8_t rem[15] = {0};
    for (int i = 0; i < 55; i++) {
        uint8_t factor = cw[i] ^ rem[0];
        memmove(&rem[0], &rem[1], 14);
        rem[14] = 0;
        for (int j = 0; j < 15; j++) {
            rem[j] ^= gf256_mul(gen[14 - j], factor);
        }
    }
    memcpy(&cw[55], rem, 15);

    /* 2. Build 29x29 Version-3 Matrix */
    const int N = 29;
    uint8_t mod[29][29] = {{0}};
    bool    is_fn[29][29] = {{false}};

    /* Finder patterns at (0,0), (0,22), (22,0) + separators */
    const int f_pos[3][2] = { {0,0}, {0,22}, {22,0} };
    for (int f = 0; f < 3; f++) {
        int fr = f_pos[f][0], fc = f_pos[f][1];
        for (int dr = -1; dr <= 7; dr++) {
            for (int dc = -1; dc <= 7; dc++) {
                int rr = fr + dr, cc = fc + dc;
                if (rr < 0 || rr >= N || cc < 0 || cc >= N) continue;
                is_fn[rr][cc] = true;
                bool dark = (dr >= 0 && dr <= 6 && dc >= 0 && dc <= 6) &&
                            (dr == 0 || dr == 6 || dc == 0 || dc == 6 ||
                             (dr >= 2 && dr <= 4 && dc >= 2 && dc <= 4));
                mod[rr][cc] = dark ? 1 : 0;
            }
        }
    }

    /* Alignment pattern at center (22, 22) */
    for (int dr = -2; dr <= 2; dr++) {
        for (int dc = -2; dc <= 2; dc++) {
            int rr = 22 + dr, cc = 22 + dc;
            is_fn[rr][cc] = true;
            mod[rr][cc] = (abs(dr) == 2 || abs(dc) == 2 || (dr == 0 && dc == 0)) ? 1 : 0;
        }
    }

    /* Timing patterns */
    for (int i = 8; i < N - 8; i++) {
        is_fn[6][i] = is_fn[i][6] = true;
        mod[6][i] = mod[i][6] = ((i & 1) == 0) ? 1 : 0;
    }

    /* Reserve format info modules & dark module */
    for (int i = 0; i < 9; i++) { is_fn[8][i] = true; is_fn[i][8] = true; }
    for (int i = 0; i < 8; i++) { is_fn[8][N - 1 - i] = true; is_fn[N - 1 - i][8] = true; }
    mod[N - 8][8] = 1; /* Fixed dark module (21, 8) */

    /* Place 70*8 = 560 bits + 7 remainder 0 bits in upward/downward 2-col zigzag with Mask 0 */
    int bidx = 0;
    bool upward = true;
    for (int right = N - 1; right >= 1; right -= 2) {
        if (right == 6) right = 5; /* Skip vertical timing column */
        for (int vert = 0; vert < N; vert++) {
            int r = upward ? (N - 1 - vert) : vert;
            for (int lr = 0; lr < 2; lr++) {
                int c = right - lr;
                if (is_fn[r][c]) continue;
                uint8_t bit = 0;
                if (bidx < 560) {
                    bit = (cw[bidx >> 3] >> (7 - (bidx & 7))) & 1;
                }
                bidx++;
                if (((r + c) & 1) == 0) bit ^= 1; /* Mask 0: (r + c) % 2 == 0 */
                mod[r][c] = bit;
            }
        }
        upward = !upward;
    }

    /* Write Format Info for Level L (01) + Mask 0 (000) -> BCH masked = 0x77C4 */
    const uint16_t fmt = 0x77C4;
    const int r_tl[15] = { 8,8,8,8,8,8,8,8, 7,5,4,3,2,1,0 };
    const int c_tl[15] = { 0,1,2,3,4,5,7,8, 8,8,8,8,8,8,8 };
    for (int i = 0; i < 15; i++) {
        uint8_t b = (fmt >> (14 - i)) & 1;
        mod[r_tl[i]][c_tl[i]] = b;
        if (i < 7) mod[N - 1 - i][8] = b;
        else       mod[8][N - 15 + i] = b;
    }

    /* Render with 3-module white quiet zone */
    int qz = 3 * module_px;
    int total_px = N * module_px + 2 * qz;
    gfx_fill_round_rect(x, y, total_px, total_px, 8, GFX_WHITE);
    gfx_round_rect(x, y, total_px, total_px, 8, 2, GFX_BLACK);
    for (int r = 0; r < N; r++) {
        for (int c = 0; c < N; c++) {
            if (mod[r][c]) {
                gfx_fill_rect(x + qz + c * module_px, y + qz + r * module_px,
                              module_px, module_px, GFX_BLACK);
            }
        }
    }
}

/* =========================================================================
 * Textbook Hanzi Study Engine: 20,902 Pinyin + 4-Line Rulings + 104px Kaiti Tianzige
 * ========================================================================= */
bool gfx_hanzi_lookup(uint16_t cp, char *out_pinyin, int py_sz, uint8_t *out_tone, uint16_t *out_syl_id)
{
    if (out_pinyin && py_sz > 0) out_pinyin[0] = '\0';
    if (out_tone) *out_tone = 5;
    if (out_syl_id) *out_syl_id = 0xFFFF;
    if (!s_hzhdr || cp < 0x4E00 || cp > 0x9FA5) return false;

    const uint8_t *base = hanzi_study_pack_bin_start;
    const uint16_t *cjk_map = (const uint16_t *)(base + s_hzhdr->off_cjk_syl_idx);
    uint16_t sid = cjk_map[cp - 0x4E00];
    if (sid >= s_hzhdr->syl_count) return false;

    const hz_syl_meta_t *metas = (const hz_syl_meta_t *)(base + s_hzhdr->off_syl_meta);
    const hz_syl_meta_t *m = &metas[sid];
    if (out_pinyin && py_sz > 0) {
        strncpy(out_pinyin, m->pinyin, (size_t)py_sz - 1);
        out_pinyin[py_sz - 1] = '\0';
    }
    if (out_tone) *out_tone = m->tone;
    if (out_syl_id) *out_syl_id = sid;
    return true;
}

uint16_t gfx_hanzi_find_syl_id(const char *pinyin_utf8, uint8_t *out_tone)
{
    if (!s_hzhdr || !pinyin_utf8 || !pinyin_utf8[0]) return 0xFFFF;
    const uint8_t *base = hanzi_study_pack_bin_start;
    const hz_syl_meta_t *metas = (const hz_syl_meta_t *)(base + s_hzhdr->off_syl_meta);

    int lo = 0, hi = (int)s_hzhdr->syl_count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) >> 1;
        int cmp = strncmp(metas[mid].pinyin, pinyin_utf8, 12);
        if (cmp == 0) {
            if (out_tone) *out_tone = metas[mid].tone;
            return (uint16_t)mid;
        }
        if (cmp < 0) lo = mid + 1;
        else         hi = mid - 1;
    }
    return 0xFFFF;
}

bool gfx_hanzi_get_opus_clip(uint16_t syl_id, const uint8_t **out_data, uint16_t *out_len)
{
    if (!s_hzhdr || syl_id >= s_hzhdr->syl_count) return false;
    const uint8_t *base = hanzi_study_pack_bin_start;
    const hz_syl_meta_t *metas = (const hz_syl_meta_t *)(base + s_hzhdr->off_syl_meta);
    const hz_syl_meta_t *m = &metas[syl_id];
    if (m->audio_bytes == 0) return false;
    if (out_data) *out_data = base + s_hzhdr->off_opus_blob + m->audio_off;
    if (out_len)  *out_len  = m->audio_bytes;
    return true;
}

void gfx_draw_pinyin_4line(int x, int y, int w, const char *pinyin_utf8)
{
    /* Draw textbook 四线三格 rulings matching 24x46 shared-baseline glyph metrics:
       Line 1 (top solid): y + 2
       Line 2 (upper-mid dashed): y + 15
       Line 3 (baseline dashed): y + 32
       Line 4 (bottom solid): y + 45 */
    gfx_hline(x, y + 2,  w, GFX_BLACK);
    gfx_hline(x, y + 45, w, GFX_BLACK);
    for (int dx = 0; dx < w; dx += 5) {
        int seg = (dx + 3 <= w) ? 3 : (w - dx);
        gfx_hline(x + dx, y + 15, seg, GFX_BLACK);
        gfx_hline(x + dx, y + 32, seg, GFX_BLACK);
    }
    gfx_vline(x,         y + 2, 44, GFX_BLACK);
    gfx_vline(x + w - 1, y + 2, 44, GFX_BLACK);

    if (!s_hzhdr || !pinyin_utf8 || !pinyin_utf8[0]) return;
    const uint8_t *base = hanzi_study_pack_bin_start;
    const uint16_t *py_cps  = (const uint16_t *)(base + s_hzhdr->off_py_cps);
    const uint8_t  *py_advs = base + s_hzhdr->off_py_advs;
    const uint8_t  *py_bmps = base + s_hzhdr->off_py_bmps;

    int total_w = 0;
    const char *p = pinyin_utf8;
    while (*p) {
        uint32_t cp = utf8_next(&p);
        int idx = (cp <= 0xFFFF) ? bsearch_u16(py_cps, s_hzhdr->py_glyph_count, (uint16_t)cp) : -1;
        total_w += (idx >= 0) ? (py_advs[idx] + 1) : 10;
    }

    int cx = x + (w - total_w) / 2;
    p = pinyin_utf8;
    while (*p) {
        uint32_t cp = utf8_next(&p);
        int idx = (cp <= 0xFFFF) ? bsearch_u16(py_cps, s_hzhdr->py_glyph_count, (uint16_t)cp) : -1;
        if (idx >= 0) {
            blit_1bpp(cx, y, 24, 46, py_bmps + (size_t)idx * 138, 1, GFX_BLACK);
            cx += py_advs[idx] + 1;
        } else {
            cx += 10;
        }
    }
}

void gfx_draw_tianzige_hanzi(int x, int y, int box_size, uint16_t cp)
{
    gfx_fill_round_rect(x, y, box_size, box_size, 8, GFX_WHITE);
    gfx_round_rect(x, y, box_size, box_size, 8, 3, GFX_BLACK);

    int mx = x + box_size / 2;
    int my = y + box_size / 2;

    /* Center horizontal & vertical dashed crosshairs (十字田字格虚线) */
    for (int d = 4; d < box_size - 4; d += 6) {
        int seg = (d + 3 < box_size - 4) ? 3 : 2;
        gfx_hline(x + d, my, seg, GFX_BLACK);
        gfx_vline(mx, y + d, seg, GFX_BLACK);
    }
    /* Subtle 米字格 diagonal guide dots */
    for (int d = 8; d < box_size - 8; d += 8) {
        gfx_pixel(x + d, y + d, GFX_BLACK);
        gfx_pixel(x + box_size - 1 - d, y + d, GFX_BLACK);
    }

    if (s_hzhdr && cp >= 0x4E00 && cp <= 0x9FA5) {
        const uint8_t  *base = hanzi_study_pack_bin_start;
        const uint16_t *kcps = (const uint16_t *)(base + s_hzhdr->off_kaiti_cps);
        int pos = bsearch_u16(kcps, s_hzhdr->kaiti_count, cp);
        if (pos >= 0) {
            int dim = (int)s_hzhdr->kaiti_dim; /* 104 */
            size_t glyph_bytes = (size_t)((dim + 7) / 8) * (size_t)dim;
            int gx = x + (box_size - dim) / 2;
            int gy = y + (box_size - dim) / 2;
            blit_1bpp(gx, gy, dim, dim, base + s_hzhdr->off_kaiti_bmps + (size_t)pos * glyph_bytes, 1, GFX_BLACK);
            return;
        }
    }

    /* Fallback for ultra-rare characters outside the 3,769 Kaiti set: 4x scale of 24x24 CJK */
    if (s_fhdr && cp >= 0x4E00 && cp <= 0x9FA5) {
        const uint8_t *fbase = kidfont_pack_bin_start;
        int gx = x + (box_size - 96) / 2;
        int gy = y + (box_size - 96) / 2;
        blit_1bpp(gx, gy, 24, 24, fbase + s_fhdr->off_cjk24 + (size_t)(cp - 0x4E00) * 72, 4, GFX_BLACK);
    }
}
