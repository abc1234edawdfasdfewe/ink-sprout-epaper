#include "app_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_random.h"
#include "esp_log.h"

#define MAX_NOTEBOOK_CHARS  64
#define MAX_PAGE_TOKENS     320

typedef struct {
    uint32_t magic;
    int32_t  count;
    uint16_t cps[MAX_NOTEBOOK_CHARS];
} hanzi_notebook_t;

static hanzi_notebook_t s_nb;
static bool s_nb_loaded = false;

static void nb_ensure_loaded(void)
{
    if (s_nb_loaded) return;
    memset(&s_nb, 0, sizeof(s_nb));
    nvs_handle_t h;
    if (nvs_open("inksprout", NVS_READONLY, &h) == ESP_OK) {
        size_t sz = sizeof(s_nb);
        nvs_get_blob(h, "hzbook_v1", &s_nb, &sz);
        nvs_close(h);
    }
    if (s_nb.magic != 0x485A424BUL || s_nb.count < 0 || s_nb.count > MAX_NOTEBOOK_CHARS) {
        /* Seed with 8 beloved starter characters from classic kid stories */
        static const uint16_t k_seed[8] = {
            0x5154 /*兔*/, 0x9F9F /*龟*/, 0x86D9 /*蛙*/, 0x8C61 /*象*/,
            0x68EE /*森*/, 0x6708 /*月*/, 0x661F /*星*/, 0x7231 /*爱*/
        };
        memset(&s_nb, 0, sizeof(s_nb));
        s_nb.magic = 0x485A424BUL;
        s_nb.count = 8;
        memcpy(s_nb.cps, k_seed, sizeof(k_seed));
    }
    s_nb_loaded = true;
}

static void nb_save(void)
{
    s_nb.magic = 0x485A424BUL;
    nvs_handle_t h;
    if (nvs_open("inksprout", NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_blob(h, "hzbook_v1", &s_nb, sizeof(s_nb));
        nvs_commit(h);
        nvs_close(h);
    }
}

static bool nb_contains(uint16_t cp)
{
    nb_ensure_loaded();
    for (int i = 0; i < s_nb.count; i++) {
        if (s_nb.cps[i] == cp) return true;
    }
    return false;
}

/* Returns true if newly added, false if removed */
static bool nb_toggle(uint16_t cp, bool *out_first_add)
{
    nb_ensure_loaded();
    if (out_first_add) *out_first_add = false;
    for (int i = 0; i < s_nb.count; i++) {
        if (s_nb.cps[i] == cp) {
            for (int k = i; k + 1 < s_nb.count; k++) s_nb.cps[k] = s_nb.cps[k + 1];
            s_nb.count--;
            nb_save();
            return false;
        }
    }
    if (s_nb.count >= MAX_NOTEBOOK_CHARS) {
        memmove(&s_nb.cps[0], &s_nb.cps[1], (MAX_NOTEBOOK_CHARS - 1) * sizeof(uint16_t));
        s_nb.count = MAX_NOTEBOOK_CHARS - 1;
    }
    s_nb.cps[s_nb.count++] = cp;
    nb_save();
    if (out_first_add) *out_first_add = true;
    return true;
}

/* =========================================================================
 * Context-Sensitive Polyphone & Children's Compound Word Engine
 * ========================================================================= */
typedef struct {
    const char *word;   /* 2-char UTF-8 compound */
    const char *py0;    /* Exact accented pinyin for 1st char */
    const char *py1;    /* Exact accented pinyin for 2nd char */
} poly_word2_t;

static const poly_word2_t k_poly_words[] = {
    /* 长: cháng vs zhǎng */
    { "长大", "zhǎng", "dà"   }, { "长出", "zhǎng", "chū"  }, { "成长", "chéng", "zhǎng" },
    { "船长", "chuán", "zhǎng"}, { "家长", "jiā",   "zhǎng"}, { "长高", "zhǎng", "gāo"   },
    { "长耳", "cháng", "ěr"   }, { "长尾", "cháng", "wěi"  }, { "长颈", "cháng", "jǐng"  },
    { "长城", "cháng", "chéng"}, { "很长", "hěn",   "cháng"}, { "长长", "cháng", "cháng" },
    /* 乐: lè vs yuè */
    { "音乐", "yīn",   "yuè"  }, { "乐曲", "yuè",   "qǔ"   }, { "乐器", "yuè",   "qì"    },
    { "快乐", "kuài",  "lè"   }, { "欢乐", "huān",  "lè"   }, { "乐趣", "lè",    "qù"    },
    /* 觉: jué vs jiào */
    { "睡觉", "shuì",  "jiào" }, { "午觉", "wǔ",    "jiào" }, { "一觉", "yī",    "jiào"  },
    { "好觉", "hǎo",   "jiào" }, { "觉得", "jué",   "de"   }, { "感觉", "gǎn",   "jué"   },
    { "发觉", "fā",    "jué"  }, { "觉醒", "jué",   "xǐng" },
    /* 着: zhe vs zháo vs zhuó */
    { "睡着", "shuì",  "zháo" }, { "着火", "zháo",  "huǒ"  }, { "着急", "zháo",  "jí"    },
    { "看着", "kàn",   "zhe"  }, { "笑着", "xiào",  "zhe"  }, { "走着", "zǒu",   "zhe"   },
    { "带着", "dài",   "zhe"  }, { "披着", "pī",    "zhe"  }, { "露着", "lù",    "zhe"   },
    { "鼓着", "gǔ",    "zhe"  }, { "提着", "tí",    "zhe"  }, { "背着", "bēi",   "zhe"   },
    { "沿着", "yán",   "zhe"  }, { "躺着", "tǎng",  "zhe"  }, { "跟着", "gēn",   "zhe"   },
    /* 数: shù vs shǔ */
    { "数数", "shǔ",   "shù"  }, { "数一", "shǔ",   "yī"   }, { "无数", "wú",    "shù"   },
    { "数学", "shù",   "xué"  }, { "数字", "shù",   "zì"   },
    /* 行: xíng vs háng */
    { "银行", "yín",   "háng" }, { "一行", "yī",    "háng" }, { "同行", "tóng",  "xíng"  },
    { "旅行", "lǚ",    "xíng" }, { "飞行", "fēi",   "xíng" }, { "行囊", "xíng",  "náng"  },
    /* 好: hǎo vs hào */
    { "好奇", "hào",   "qí"   }, { "爱好", "ài",    "hào"  }, { "好学", "hào",   "xué"   },
    { "好人", "hǎo",   "rén"  }, { "好梦", "hǎo",   "mèng" }, { "美好", "měi",   "hǎo"   },
    /* 还: hái vs huán */
    { "归还", "guī",   "huán" }, { "还给", "huán",  "gěi"  }, { "还有", "hái",   "yǒu"   },
    { "还是", "hái",   "shì"  }, { "还在", "hái",   "zài"  },
    /* 都: dōu vs dū */
    { "首都", "shǒu",  "dū"   }, { "都市", "dū",    "shì"  }, { "都是", "dōu",   "shì"   },
    { "都有", "dōu",   "yǒu"  }, { "全都", "quán",  "dōu"  },
    /* 重: zhòng vs chóng */
    { "重新", "chóng", "xīn"  }, { "重复", "chóng", "fù"   }, { "重量", "zhòng", "liàng" },
    { "重要", "zhòng", "yào"  }, { "多重", "duō",   "zhòng"},
    /* 空: kōng vs kòng */
    { "天空", "tiān",  "kōng" }, { "太空", "tài",   "kōng" }, { "夜空", "yè",    "kōng"  },
    { "空气", "kōng",  "qì"   }, { "空船", "kōng",  "chuán"}, { "空地", "kòng",  "dì"    },
    /* 种: zhǒng vs zhòng */
    { "种子", "zhǒng", "zi"   }, { "各种", "gè",    "zhǒng"}, { "种花", "zhòng", "huā"   },
    { "种树", "zhòng", "shù"  },
    /* 发: fā vs fà */
    { "头发", "tóu",   "fa"   }, { "白发", "bái",   "fà"   }, { "出发", "chū",   "fā"    },
    { "发光", "fā",    "guāng"}, { "发现", "fā",    "xiàn" },
    /* 地: dì vs de */
    { "草地", "cǎo",   "dì"   }, { "大地", "dà",    "dì"   }, { "地球", "dì",    "qiú"   },
    { "地上", "dì",    "shàng"}, { "地下", "dì",    "xià"  }, { "地方", "dì",    "fang"  },
    { "轻轻", "qīng",  "qīng" },
    /* 只: zhī vs zhǐ */
    { "只有", "zhǐ",   "yǒu"  }, { "只好", "zhǐ",   "hǎo"  }, { "只能", "zhǐ",   "néng"  },
    { "三只", "sān",   "zhī"  }, { "一只", "yī",    "zhī"  }, { "两只", "liǎng", "zhī"   },
    /* 背: bēi vs bèi */
    { "背上", "bēi",   "shàng"}, { "背对", "bèi",   "duì"  }, { "后背", "hòu",   "bèi"   },
    /* 没: méi vs mò */
    { "沉没", "chén",  "mò"   }, { "淹没", "yān",   "mò"   }, { "没有", "méi",   "yǒu"   },
    /* 漂亮 / 萝卜 / 聪明 / 妈妈 / 爸爸 / 娃娃 / 星星 / 月亮 (neutral tone polish) */
    { "漂亮", "piào",  "liang"}, { "萝卜", "luó",   "bo"   }, { "妈妈", "mā",    "ma"    },
    { "爸爸", "bà",    "ba"   }, { "尾巴", "wěi",   "ba"   }, { "耳朵", "ěr",    "duo"   },
    { "嘴巴", "zuǐ",   "ba"   }, { "眼睛", "yǎn",   "jing" }, { "石头", "shí",   "tou"   },
    { "木头", "mù",    "tou"  }, { "月亮", "yuè",   "liang"}, { "星星", "xīng",  "xing"  },
    { "朋友", "péng",  "you"  }, { "孩子", "hái",   "zi"   }, { "兔子", "tù",    "zi"    },
    { "衣裳", "yī",    "shang"}, { "暖和", "nuǎn",  "huo"  },
};
#define POLY_WORD_COUNT  ((int)(sizeof(k_poly_words) / sizeof(k_poly_words[0])))

/* Starter dictionary for standalone characters in the Flashcard Notebook */
typedef struct {
    uint16_t    cp;
    const char *word;
} char_word_hint_t;

static const char_word_hint_t k_notebook_hints[] = {
    { 0x5154, "小白兔" }, { 0x9F9F, "小乌龟" }, { 0x86D9, "大青蛙" }, { 0x8C61, "称大象" },
    { 0x68EE, "大森林" }, { 0x6708, "弯月亮" }, { 0x661F, "亮星星" }, { 0x7231, "爱护你" },
    { 0x9F99, "小恐龙" }, { 0x732B, "小猫咪" }, { 0x864E, "大老虎" }, { 0x9E1F, "小飞鸟" },
    { 0x82B1, "小红花" }, { 0x8349, "绿青草" }, { 0x6811, "大松树" }, { 0x6C34, "清溪水" },
    { 0x706B, "萤火虫" }, { 0x5C71, "高山顶" }, { 0x98CE, "温柔风" }, { 0x96E8, "小春雨" },
};

static void encode_cp_utf8(uint32_t cp, char *out)
{
    if (cp < 0x80) {
        out[0] = (char)cp; out[1] = '\0';
    } else if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        out[2] = '\0';
    } else {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        out[3] = '\0';
    }
}

static uint32_t next_cp(const char **p)
{
    const uint8_t *s = (const uint8_t *)(*p);
    if (!*s) return 0;
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

static bool is_cjk(uint32_t cp)
{
    return (cp >= 0x4E00 && cp <= 0x9FA5);
}

static bool is_particle_char(uint16_t cp)
{
    /* Grammatical particles avoided when picking automatic 2-char neighbors unless in k_poly_words */
    static const uint16_t k_part[] = {
        0x7684 /*的*/, 0x4E86 /*了*/, 0x5417 /*吗*/, 0x5462 /*呢*/, 0x5427 /*吧*/,
        0x5440 /*呀*/, 0x554A /*啊*/, 0x54DF /*哟*/, 0x7740 /*着*/, 0x8FC7 /*过*/
    };
    for (size_t i = 0; i < sizeof(k_part) / sizeof(k_part[0]); i++) {
        if (k_part[i] == cp) return true;
    }
    return false;
}

static const char *tone_badge_str(uint8_t tone)
{
    switch (tone) {
    case 1:  return "第一声 · 阴平 (ˉ)";
    case 2:  return "第二声 · 阳平 (ˊ)";
    case 3:  return "第三声 · 上声 (ˇ)";
    case 4:  return "第四声 · 去声 (ˋ)";
    default: return "轻声音节";
    }
}

typedef struct {
    uint32_t cp;
    int16_t  hanzi_ord;  /* -1 if punctuation/space, 0..N-1 for CJK chars */
    int16_t  line_idx;
    int16_t  x;
} page_tok_t;

typedef struct {
    uint16_t cp;
    char     py[16];
    uint8_t  tone;
    uint16_t syl_id;
    /* Context 2-char compound word */
    bool     has_word;
    uint16_t w_cp0, w_cp1;
    char     w_py0[16], w_py1[16];
    uint16_t w_syl0, w_syl1;
    int      partner_tok_idx; /* Token index of companion char on page for subtle underline */
} char_study_info_t;

static bool match_poly_pair(uint16_t c0, uint16_t c1, const char **out_py0, const char **out_py1)
{
    char pair[8] = {0};
    char b0[5], b1[5];
    encode_cp_utf8(c0, b0);
    encode_cp_utf8(c1, b1);
    snprintf(pair, sizeof(pair), "%s%s", b0, b1);
    for (int i = 0; i < POLY_WORD_COUNT; i++) {
        if (strcmp(k_poly_words[i].word, pair) == 0) {
            *out_py0 = k_poly_words[i].py0;
            *out_py1 = k_poly_words[i].py1;
            return true;
        }
    }
    return false;
}

static void analyze_char_in_context(const page_tok_t *toks, int n_toks, int focus_tok, char_study_info_t *out)
{
    memset(out, 0, sizeof(*out));
    out->partner_tok_idx = -1;
    out->w_syl0 = out->w_syl1 = 0xFFFF;

    uint16_t cp = (uint16_t)toks[focus_tok].cp;
    out->cp = cp;
    gfx_hanzi_lookup(cp, out->py, sizeof(out->py), &out->tone, &out->syl_id);

    /* Adverbial '地' before verb (e.g. 高兴地、轻轻地、认真地) -> 'de' */
    if (cp == 0x5730 /*地*/ && focus_tok > 0 && is_cjk(toks[focus_tok - 1].cp) &&
        focus_tok + 1 < n_toks && is_cjk(toks[focus_tok + 1].cp)) {
        const char *p0 = NULL, *p1 = NULL;
        if (!match_poly_pair((uint16_t)toks[focus_tok - 1].cp, cp, &p0, &p1) &&
            !match_poly_pair(cp, (uint16_t)toks[focus_tok + 1].cp, &p0, &p1)) {
            uint8_t t = 5;
            uint16_t sid = gfx_hanzi_find_syl_id("de", &t);
            if (sid != 0xFFFF) {
                strcpy(out->py, "de");
                out->tone = 5;
                out->syl_id = sid;
            }
        }
    }

    bool has_prev = (focus_tok > 0 && is_cjk(toks[focus_tok - 1].cp));
    bool has_next = (focus_tok + 1 < n_toks && is_cjk(toks[focus_tok + 1].cp));

    const char *ov0 = NULL, *ov1 = NULL;
    /* Priority 1: Exact match in polyphone / common kid compound table with next or prev char */
    if (has_next && match_poly_pair(cp, (uint16_t)toks[focus_tok + 1].cp, &ov0, &ov1)) {
        out->has_word = true;
        out->partner_tok_idx = focus_tok + 1;
        out->w_cp0 = cp;
        out->w_cp1 = (uint16_t)toks[focus_tok + 1].cp;
        strncpy(out->py, ov0, sizeof(out->py) - 1);
        out->syl_id = gfx_hanzi_find_syl_id(ov0, &out->tone);
        strncpy(out->w_py0, ov0, sizeof(out->w_py0) - 1);
        strncpy(out->w_py1, ov1, sizeof(out->w_py1) - 1);
        uint8_t dm = 0;
        out->w_syl0 = out->syl_id;
        out->w_syl1 = gfx_hanzi_find_syl_id(ov1, &dm);
        return;
    }
    if (has_prev && match_poly_pair((uint16_t)toks[focus_tok - 1].cp, cp, &ov0, &ov1)) {
        out->has_word = true;
        out->partner_tok_idx = focus_tok - 1;
        out->w_cp0 = (uint16_t)toks[focus_tok - 1].cp;
        out->w_cp1 = cp;
        strncpy(out->py, ov1, sizeof(out->py) - 1);
        out->syl_id = gfx_hanzi_find_syl_id(ov1, &out->tone);
        strncpy(out->w_py0, ov0, sizeof(out->w_py0) - 1);
        strncpy(out->w_py1, ov1, sizeof(out->w_py1) - 1);
        uint8_t dm = 0;
        out->w_syl0 = gfx_hanzi_find_syl_id(ov0, &dm);
        out->w_syl1 = out->syl_id;
        return;
    }

    /* Priority 2: Natural adjacent 2-char phrase in story clause (prefer non-particle neighbor) */
    if (has_next && !is_particle_char((uint16_t)toks[focus_tok + 1].cp)) {
        out->has_word = true;
        out->partner_tok_idx = focus_tok + 1;
        out->w_cp0 = cp;
        out->w_cp1 = (uint16_t)toks[focus_tok + 1].cp;
    } else if (has_prev && !is_particle_char((uint16_t)toks[focus_tok - 1].cp)) {
        out->has_word = true;
        out->partner_tok_idx = focus_tok - 1;
        out->w_cp0 = (uint16_t)toks[focus_tok - 1].cp;
        out->w_cp1 = cp;
    } else if (has_next) {
        out->has_word = true;
        out->partner_tok_idx = focus_tok + 1;
        out->w_cp0 = cp;
        out->w_cp1 = (uint16_t)toks[focus_tok + 1].cp;
    } else if (has_prev) {
        out->has_word = true;
        out->partner_tok_idx = focus_tok - 1;
        out->w_cp0 = (uint16_t)toks[focus_tok - 1].cp;
        out->w_cp1 = cp;
    }

    if (out->has_word) {
        uint8_t t0 = 0, t1 = 0;
        gfx_hanzi_lookup(out->w_cp0, out->w_py0, sizeof(out->w_py0), &t0, &out->w_syl0);
        gfx_hanzi_lookup(out->w_cp1, out->w_py1, sizeof(out->w_py1), &t1, &out->w_syl1);
    }
}

static void draw_right_flashcard(const char_study_info_t *inf, bool in_notebook)
{
    const int rx = 464, ry = 50, rw = 320, rh = 382;
    const int rcx = rx + rw / 2; /* 624 */

    gfx_fill_round_rect(rx, ry, rw, rh, 16, GFX_WHITE);
    gfx_round_rect(rx, ry, rw, rh, 16, 3, GFX_BLACK);

    /* 1. Tone & Notebook Status Header */
    gfx_draw_text_center(rcx, 58, tone_badge_str(inf->tone), FONT_16, GFX_BLACK);

    /* 2. Textbook 4-Line 3-Grid Pinyin Rulings (y=80..128) */
    gfx_draw_pinyin_4line(rx + 36, 80, rw - 72, inf->py);

    /* 3. Giant 148x148 Standard Tianzige + 104x104 Native Kaiti SC Character */
    gfx_draw_tianzige_hanzi(rcx - 74, 136, 148, inf->cp);

    /* 4. Context 2-Char Word Compound Box (y=294..368) */
    gfx_fill_round_rect(rx + 18, 294, rw - 36, 72, 12, GFX_GRAY_LIGHT);
    gfx_round_rect(rx + 18, 294, rw - 36, 72, 12, 2, GFX_BLACK);
    if (inf->has_word) {
        char s0[5], s1[5], wline[64], pyline[64];
        encode_cp_utf8(inf->w_cp0, s0);
        encode_cp_utf8(inf->w_cp1, s1);
        snprintf(wline, sizeof(wline), "文中词组：【 %s %s 】", s0, s1);
        snprintf(pyline, sizeof(pyline), "%s   %s", inf->w_py0, inf->w_py1);
        gfx_draw_text_center(rcx, 304, wline, FONT_24, GFX_BLACK);
        gfx_draw_text_center(rcx, 338, pyline, FONT_16, GFX_BLACK);
    } else {
        char s0[5], wline[64];
        encode_cp_utf8(inf->cp, s0);
        snprintf(wline, sizeof(wline), "单字识读：【 %s 】", s0);
        gfx_draw_text_center(rcx, 318, wline, FONT_24, GFX_BLACK);
    }

    /* 5. Notebook Collection Pill (y=378..418) */
    if (in_notebook) {
        gfx_fill_round_rect(rx + 18, 378, rw - 36, 40, 10, GFX_BLACK);
        gfx_draw_text_center(rcx, 390, "★ 已收入生字本 · 按滚轮连读词组", FONT_16, GFX_WHITE);
    } else {
        gfx_round_rect(rx + 18, 378, rw - 36, 40, 10, 2, GFX_BLACK);
        gfx_draw_text_center(rcx, 390, "● 按滚轮连读词组 + 收入生字本(★+1)", FONT_16, GFX_BLACK);
    }
}

/* =========================================================================
 * PUBLIC ENTRY 1: Story Reading Page Thumbwheel Spot-Reader Modal
 * ========================================================================= */
void app_word_study_page(const char *page_utf8, const char *book_title)
{
    if (!page_utf8 || !page_utf8[0]) return;
    bsp_audio_stop_all();
    nb_ensure_loaded();

    page_tok_t *toks = (page_tok_t *)calloc(MAX_PAGE_TOKENS, sizeof(page_tok_t));
    int16_t *hanzi_to_tok = (int16_t *)calloc(MAX_PAGE_TOKENS, sizeof(int16_t));
    if (!toks || !hanzi_to_tok) {
        if (toks) free(toks);
        if (hanzi_to_tok) free(hanzi_to_tok);
        return;
    }

    /* Layout tokens across Left Panel (x=30..434, max_w=404, FONT_24) */
    const int lx_start = 30;
    const int max_w = 404;
    int cx = lx_start;
    int line_idx = 0;
    int n_toks = 0;
    int n_hanzi = 0;

    const char *p = page_utf8;
    if ((uint8_t)p[0] == 0xEF && (uint8_t)p[1] == 0xBB && (uint8_t)p[2] == 0xBF) p += 3;

    while (*p && n_toks < MAX_PAGE_TOKENS) {
        uint32_t cp = next_cp(&p);
        if (cp == '\r') continue;
        if (cp == '\n') {
            if (cx > lx_start) { cx = lx_start; line_idx++; }
            continue;
        }
        int adv = (cp >= 0x20 && cp <= 0x7E) ? 12 : 24;
        if (cx + adv > lx_start + max_w && cx > lx_start) {
            cx = lx_start;
            line_idx++;
        }
        toks[n_toks].cp = cp;
        toks[n_toks].line_idx = (int16_t)line_idx;
        toks[n_toks].x = (int16_t)cx;
        if (is_cjk(cp)) {
            toks[n_toks].hanzi_ord = (int16_t)n_hanzi;
            hanzi_to_tok[n_hanzi] = (int16_t)n_toks;
            n_hanzi++;
        } else {
            toks[n_toks].hanzi_ord = -1;
        }
        n_toks++;
        cx += adv;
    }

    if (n_hanzi == 0) {
        free(toks);
        free(hanzi_to_tok);
        return;
    }

    int cur_hz = 0;
    int last_spoken_hz = -1;
    bool first = true;

    while (1) {
        int focus_tok = hanzi_to_tok[cur_hz];
        char_study_info_t inf;
        analyze_char_in_context(toks, n_toks, focus_tok, &inf);
        bool saved = nb_contains(inf.cp);

        if (cur_hz != last_spoken_hz) {
            bsp_audio_speak_syllables(inf.syl_id, 0xFFFF, 0xFFFF);
            last_spoken_hz = cur_hz;
        }

        gfx_clear(GFX_WHITE);
        gfx_draw_top_bar("★ 绘本滚轮点读 · 四线拼音与楷体田字格", g_prof.stars, app_eye_mins_left());

        /* Left Panel: Highlighted Story Text */
        gfx_round_rect(16, 50, 434, 382, 16, 2, GFX_BLACK);
        char sub_h[128];
        snprintf(sub_h, sizeof(sub_h), "%s  (第 %d / %d 字)",
                 (book_title && book_title[0]) ? book_title : "当前绘本页", cur_hz + 1, n_hanzi);
        gfx_draw_text(30, 59, sub_h, FONT_16, GFX_BLACK);
        gfx_hline(24, 82, 418, GFX_BLACK);

        /* Viewport auto-scrolls smoothly if page exceeds 8 lines */
        const int vis_lines = 8;
        const int line_h = 40;
        int focus_line = toks[focus_tok].line_idx;
        int start_line = 0;
        if (focus_line >= vis_lines) {
            start_line = focus_line - (vis_lines - 2);
        }

        for (int i = 0; i < n_toks; i++) {
            int rel_line = toks[i].line_idx - start_line;
            if (rel_line < 0 || rel_line >= vis_lines) continue;
            int tx = toks[i].x;
            int ty = 94 + rel_line * line_h;
            char ch_s[5];
            encode_cp_utf8(toks[i].cp, ch_s);

            if (i == focus_tok) {
                gfx_fill_round_rect(tx - 3, ty - 4, 30, 32, 7, GFX_BLACK);
                gfx_draw_text(tx, ty, ch_s, FONT_24, GFX_WHITE);
                /* Tiny pointer triangle below cursor */
                gfx_hline(tx + 9,  ty + 30, 6, GFX_BLACK);
                gfx_hline(tx + 11, ty + 31, 2, GFX_BLACK);
            } else {
                gfx_draw_text(tx, ty, ch_s, FONT_24, GFX_BLACK);
                if (i == inf.partner_tok_idx) {
                    /* Dashed underline under partner character of 2-char phrase */
                    for (int ux = tx + 1; ux < tx + 23; ux += 4) {
                        gfx_hline(ux, ty + 27, 2, GFX_BLACK);
                        gfx_hline(ux, ty + 28, 2, GFX_BLACK);
                    }
                }
            }
        }

        /* Right Panel: Textbook 4-Line Pinyin + 104px Kaiti Tianzige Card */
        draw_right_flashcard(&inf, saved);

        gfx_draw_bottom_hints("拨动滚轮逐字点读", "连读词组/收生字", "返回继续读绘本");
        bsp_epd_smart_present(gfx_get_fb(), first);
        first = false;

        bsp_key_t k = bsp_input_wait(30000);
        if (k == KEY_BACK) {
            bsp_audio_stop_all();
            break;
        } else if (k == KEY_PREV) {
            cur_hz = (cur_hz + n_hanzi - 1) % n_hanzi;
        } else if (k == KEY_NEXT) {
            cur_hz = (cur_hz + 1) % n_hanzi;
        } else if (k == KEY_SHAKE) {
            /* Replay character + compound word without toggling notebook */
            if (inf.has_word) bsp_audio_speak_syllables(inf.syl_id, inf.w_syl0, inf.w_syl1);
            else              bsp_audio_speak_syllables(inf.syl_id, 0xFFFF, 0xFFFF);
        } else if (k == KEY_OK) {
            /* Speak single character + pause + 2-char context compound, and collect into Notebook! */
            if (!saved) {
                bool added = false;
                nb_toggle(inf.cp, &added);
                if (added) {
                    app_add_stars(1);
                    app_profile_save();
                }
            }
            if (inf.has_word) {
                bsp_audio_speak_syllables(inf.syl_id, inf.w_syl0, inf.w_syl1);
            } else {
                bsp_audio_speak_syllables(inf.syl_id, 0xFFFF, 0xFFFF);
            }
        }
    }

    free(toks);
    free(hanzi_to_tok);
}

/* =========================================================================
 * PUBLIC ENTRY 2: 「★ 我的生字卡片盒」+ 听音找字小闯关
 * ========================================================================= */
static void run_listen_find_hanzi_quiz(void)
{
    nb_ensure_loaded();
    if (s_nb.count < 4) return;

    int round = 0;
    while (round < 5) {
        int ans_slot = (int)(esp_random() % 4);
        uint16_t opts[4] = {0};
        opts[ans_slot] = s_nb.cps[esp_random() % s_nb.count];

        /* Fill 3 distinct distractors with different pronunciations */
        char ans_py[16] = {0};
        uint8_t ans_tone = 0;
        uint16_t ans_syl = 0xFFFF;
        gfx_hanzi_lookup(opts[ans_slot], ans_py, sizeof(ans_py), &ans_tone, &ans_syl);

        for (int i = 0; i < 4; i++) {
            if (i == ans_slot) continue;
            for (int tries = 0; tries < 40; tries++) {
                uint16_t cand = s_nb.cps[esp_random() % s_nb.count];
                if (tries > 15) {
                    static const uint16_t k_pool[] = {
                        0x5154,0x9F9F,0x86D9,0x8C61,0x68EE,0x6708,0x661F,0x7231,
                        0x5C71,0x6C34,0x706B,0x98CE,0x82B1,0x9E1F,0x9C7C,0x6811
                    };
                    cand = k_pool[esp_random() % 16];
                }
                char cpy[16]; uint8_t ct; uint16_t csyl = 0xFFFF;
                gfx_hanzi_lookup(cand, cpy, sizeof(cpy), &ct, &csyl);
                bool dup = (csyl == ans_syl);
                for (int j = 0; j < 4; j++) if (opts[j] == cand) dup = true;
                if (!dup) { opts[i] = cand; break; }
            }
            if (opts[i] == 0) opts[i] = (uint16_t)(0x4E00 + i * 17);
        }

        int sel = 0;
        bool solved = false;
        bool wrong_hint = false;
        bool first = true;
        bsp_audio_speak_syllables(ans_syl, 0xFFFF, 0xFFFF);

        while (1) {
            gfx_clear(GFX_WHITE);
            char hdr[96];
            snprintf(hdr, sizeof(hdr), "★ 听音找字小闯关 (第 %d / 5 题)", round + 1);
            gfx_draw_top_bar(hdr, g_prof.stars, app_eye_mins_left());

            gfx_draw_pinyin_4line(276, 54, 248, ans_py);
            gfx_draw_text_center(GFX_W / 2, 108, "听发音看拼音，拨动滚轮找出正确的汉字吧！(摇一摇重听发音)", FONT_16, GFX_BLACK);

            for (int i = 0; i < 4; i++) {
                int x = 34 + i * 188;
                int y = 144;
                if (i == sel) {
                    gfx_round_rect(x - 6, y - 6, 168, 214, 14, 5, GFX_BLACK);
                } else {
                    gfx_round_rect(x - 2, y - 2, 160, 206, 12, 1, GFX_BLACK);
                }
                gfx_draw_tianzige_hanzi(x + 6, y + 6, 144, opts[i]);
                gfx_fill_round_rect(x + 28, y + 158, 100, 34, 8, (i == sel) ? GFX_BLACK : GFX_GRAY_LIGHT);
                char lbl[24];
                snprintf(lbl, sizeof(lbl), "选项 %d", i + 1);
                gfx_draw_text_center(x + 78, y + 167, lbl, FONT_16, (i == sel) ? GFX_WHITE : GFX_BLACK);
            }

            if (solved) {
                gfx_fill_round_rect(150, 376, GFX_W - 300, 50, 12, GFX_BLACK);
                gfx_draw_text_center(GFX_W / 2, 390, "★ 找对啦！奖励 +1 颗智慧星！按滚轮下一题", FONT_24, GFX_WHITE);
            } else if (wrong_hint) {
                gfx_round_rect(160, 376, GFX_W - 320, 50, 12, 2, GFX_BLACK);
                gfx_draw_text_center(GFX_W / 2, 390, "再仔细听听读音，换一张田字格试试哟！", FONT_24, GFX_BLACK);
            }

            gfx_draw_bottom_hints("滚轮选田字格", solved ? "进入下一题" : "按下确认答案", "退出闯关");
            bsp_epd_smart_present(gfx_get_fb(), first);
            first = false;

            bsp_key_t k = bsp_input_wait(30000);
            if (k == KEY_BACK) return;
            if (solved && k == KEY_OK) { round++; break; }
            if (k == KEY_PREV) { sel = (sel + 3) % 4; wrong_hint = false; }
            else if (k == KEY_NEXT) { sel = (sel + 1) % 4; wrong_hint = false; }
            else if (k == KEY_SHAKE) { bsp_audio_speak_syllables(ans_syl, 0xFFFF, 0xFFFF); }
            else if (k == KEY_OK) {
                if (sel == ans_slot) {
                    solved = true;
                    app_add_stars(1);
                    app_profile_save();
                    bsp_audio_sfx(SFX_SUCCESS);
                } else {
                    wrong_hint = true;
                    bsp_audio_speak_syllables(ans_syl, 0xFFFF, 0xFFFF);
                }
            }
        }
    }
}

void app_hanzi_notebook_run(void)
{
    nb_ensure_loaded();
    int sel = 0;
    int last_spoken = -1;
    bool first = true;

    while (1) {
        if (sel >= s_nb.count) sel = s_nb.count - 1;
        if (sel < 0) sel = 0;

        uint16_t cp = s_nb.cps[sel];
        char_study_info_t inf = {0};
        inf.cp = cp;
        gfx_hanzi_lookup(cp, inf.py, sizeof(inf.py), &inf.tone, &inf.syl_id);

        /* Attach starter word compound if available */
        for (size_t i = 0; i < sizeof(k_notebook_hints) / sizeof(k_notebook_hints[0]); i++) {
            if (k_notebook_hints[i].cp == cp) {
                const char *wp = k_notebook_hints[i].word;
                uint32_t c0 = next_cp(&wp);
                uint32_t c1 = next_cp(&wp);
                uint32_t c2 = next_cp(&wp);
                inf.has_word = true;
                inf.w_cp0 = (uint16_t)(c2 ? c1 : c0);
                inf.w_cp1 = (uint16_t)(c2 ? c2 : c1);
                uint8_t dm = 0;
                gfx_hanzi_lookup(inf.w_cp0, inf.w_py0, sizeof(inf.w_py0), &dm, &inf.w_syl0);
                gfx_hanzi_lookup(inf.w_cp1, inf.w_py1, sizeof(inf.w_py1), &dm, &inf.w_syl1);
                break;
            }
        }

        if (sel != last_spoken) {
            bsp_audio_speak_syllables(inf.syl_id, 0xFFFF, 0xFFFF);
            last_spoken = sel;
        }

        gfx_clear(GFX_WHITE);
        char top[96];
        snprintf(top, sizeof(top), "★ 我的生字卡片盒 (共收藏 %d 字)", (int)s_nb.count);
        gfx_draw_top_bar(top, g_prof.stars, app_eye_mins_left());

        /* Left Grid of Collected Characters (4 cols x 4 rows = 16 per page) */
        gfx_round_rect(16, 50, 434, 382, 16, 2, GFX_BLACK);
        gfx_draw_text(32, 60, "阅读绘本时按滚轮收藏的生字  (摇一摇开启听音找字闯关！)", FONT_16, GFX_BLACK);
        gfx_hline(24, 84, 418, GFX_BLACK);

        int pg_base = (sel / 16) * 16;
        for (int i = 0; i < 16 && (pg_base + i) < s_nb.count; i++) {
            int idx = pg_base + i;
            int col = i % 4, row = i / 4;
            int bx = 34 + col * 100;
            int by = 96 + row * 70;
            char ch_s[5];
            encode_cp_utf8(s_nb.cps[idx], ch_s);
            if (idx == sel) {
                gfx_fill_round_rect(bx, by, 84, 58, 12, GFX_BLACK);
                gfx_draw_text_center(bx + 42, by + 17, ch_s, FONT_24, GFX_WHITE);
            } else {
                gfx_round_rect(bx, by, 84, 58, 12, 2, GFX_BLACK);
                gfx_draw_text_center(bx + 42, by + 17, ch_s, FONT_24, GFX_BLACK);
            }
        }

        gfx_fill_round_rect(34, 382, 398, 38, 10, GFX_GRAY_LIGHT);
        gfx_round_rect(34, 382, 398, 38, 10, 2, GFX_BLACK);
        gfx_draw_text_center(233, 393, "✦ 轻摇机身：开启『听音找字』田字格闯关 (★+1)", FONT_16, GFX_BLACK);

        /* Right Textbook Tianzige Flashcard */
        draw_right_flashcard(&inf, true);

        gfx_draw_bottom_hints("滚轮翻生字卡", "重播标准读音", "返回绘本馆");
        bsp_epd_smart_present(gfx_get_fb(), first);
        first = false;

        bsp_key_t k = bsp_input_wait(30000);
        if (k == KEY_BACK) {
            bsp_audio_stop_all();
            break;
        } else if (k == KEY_PREV) {
            sel = (sel + s_nb.count - 1) % s_nb.count;
        } else if (k == KEY_NEXT) {
            sel = (sel + 1) % s_nb.count;
        } else if (k == KEY_OK) {
            if (inf.has_word) bsp_audio_speak_syllables(inf.syl_id, inf.w_syl0, inf.w_syl1);
            else              bsp_audio_speak_syllables(inf.syl_id, 0xFFFF, 0xFFFF);
        } else if (k == KEY_SHAKE && s_nb.count >= 4) {
            bsp_audio_sfx(SFX_CONFIRM);
            run_listen_find_hanzi_quiz();
            last_spoken = -1;
            first = true;
        }
    }
}
