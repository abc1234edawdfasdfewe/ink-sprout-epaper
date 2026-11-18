#include "app_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_transport.h"
#include "esp_transport_tcp.h"
#include "esp_transport_ssl.h"
#include "esp_transport_ws.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "cJSON.h"
#include "opus.h"

static const char *TAG = "APP_AI_STORY";

#define MAX_AI_PAGES     10
#define GACHA_HERO_CNT   24
#define GACHA_SCENE_CNT  24
#define GACHA_THEME_CNT  24

typedef struct {
    char         title[96];
    gfx_art_id_t art;
    int          page_count;
    char         pages[MAX_AI_PAGES][440];
    uint16_t     page_start_permille[MAX_AI_PAGES]; /* 0..1000 audio sync cues */
    bool         has_quiz;
    char         quiz_q[192];
    char         quiz_opts[3][96];
    int          quiz_ans;
    bool         has_wav;
    char         wav_path[192];
    char         txt_path[192];
} ai_story_doc_t;

typedef struct {
    const char  *name;
    gfx_art_id_t art;
} gacha_hero_t;

static const gacha_hero_t k_heroes[GACHA_HERO_CNT] = {
    { "宝贝自己",     ART_SLEEP_BEAR    },
    { "勇敢小恐龙",   ART_DINO          },
    { "宇航员小兔",   ART_RABBIT        },
    { "侦探小猫咪",   ART_CAT           },
    { "深海小潜艇",   ART_SUBMARINE     },
    { "机智小象象",   ART_ELEPHANT      },
    { "森林小骑士",   ART_KNIGHT_FOREST },
    { "歌唱小青蛙",   ART_TADPOLE_FROG  },
    { "消防员小熊",   ART_SLEEP_BEAR    },
    { "魔法小刺猬",   ART_HEDGEHOG      },
    { "发明家小狐狸", ART_FOX           },
    { "功夫大熊猫",   ART_PANDA         },
    { "邮递员白鸽",   ART_PARROT_MIC    },
    { "厨神小松鼠",   ART_SQUIRREL      },
    { "雪地小企鹅",   ART_PENGUIN       },
    { "云端独角兽",   ART_UNICORN       },
    { "星际机器人",   ART_ROBOT         },
    { "考古小队长",   ART_LOCK_TREASURE },
    { "花仙子蝴蝶",   ART_BUTTERFLY     },
    { "乌龟老博士",   ART_TURTLE        },
    { "闪电小猎豹",   ART_CAT           },
    { "发光小水母",   ART_SUBMARINE     },
    { "守塔猫头鹰",   ART_OWL_GUARD     },
    { "齐天小美猴",   ART_MONKEY        },
};

static const char *k_scenes[GACHA_SCENE_CNT] = {
    "云朵糖果城堡",   "月球环形山基地", "海底发光森林",   "彩虹蒸汽火车",
    "恐龙博物馆夜游", "四季魔法树屋",   "蚂蚁地下王国",   "静谧星河夜空",
    "巧克力瀑布岛",   "北极极光冰屋",   "火星红色农场",   "会飞的图书馆",
    "萤火虫音乐谷",   "人体细胞小卫队", "蒲公英热气球",   "海底珊瑚邮局",
    "侏罗纪火山谷",   "微缩玩具小镇",   "敦煌飞天沙洲",   "天空浮岛花园",
    "钟表齿轮迷宫",   "迷雾竹林茶馆",   "云端彩虹滑梯",   "时间胶囊列车",
};

static const char *k_themes[GACHA_THEME_CNT] = {
    "温柔晚安快快睡", "好好刷牙护牙齿", "勇敢独立不怕黑", "学会分享与合作",
    "奇妙太空小百科", "管理情绪不哭闹", "诚实守信好品质", "爱惜粮食不挑食",
    "礼貌用语交朋友", "自己动手理玩具", "遇到困难不放弃", "保护眼睛看远方",
    "认识恐龙与化石", "海洋生物小奥秘", "奇妙天气为什么", "早睡早起不赖床",
    "输了比赛不气馁", "学会倾听尊重人", "安全过马路常识", "按时喝水爱运动",
    "感恩父母表达爱", "好奇提问爱动脑", "节约用水护地球", "奇妙昆虫小世界",
};

static void get_hero_display(int idx, char *out, size_t sz)
{
    if (idx == 0) {
        const char *cn = g_net_cfg.child_name[0] ? g_net_cfg.child_name : "宝贝";
        snprintf(out, sz, "%s自己", cn);
    } else {
        snprintf(out, sz, "%s", k_heroes[idx % GACHA_HERO_CNT].name);
    }
}

static void clean_filename_stem(const char *src, char *dst, size_t dst_sz)
{
    size_t o = 0;
    for (size_t i = 0; src[i] && o + 1 < dst_sz; i++) {
        unsigned char c = (unsigned char)src[i];
        if (c < 0x20 || c == '/' || c == '\\' || c == ':' || c == '*' ||
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|' ||
            c == '\r' || c == '\n') {
            continue;
        }
        /* Strip《》brackets in filenames for clean display */
        if ((unsigned char)src[i] == 0xE3 && (unsigned char)src[i + 1] == 0x80 &&
            ((unsigned char)src[i + 2] == 0x8A || (unsigned char)src[i + 2] == 0x8B)) {
            i += 2;
            continue;
        }
        dst[o++] = (char)c;
    }
    dst[o] = '\0';
    if (dst[0] == '\0') strncpy(dst, "魔法故事", dst_sz - 1);
}

static void write_wav_header(FILE *fp, uint32_t sample_rate, uint32_t data_bytes)
{
    uint8_t h[44];
    uint32_t riff_sz = 36 + data_bytes;
    uint32_t byte_rate = sample_rate * 2;
    memcpy(h + 0, "RIFF", 4);
    h[4] = riff_sz & 0xFF; h[5] = (riff_sz >> 8) & 0xFF; h[6] = (riff_sz >> 16) & 0xFF; h[7] = (riff_sz >> 24) & 0xFF;
    memcpy(h + 8, "WAVEfmt ", 8);
    h[16] = 16; h[17] = 0; h[18] = 0; h[19] = 0;
    h[20] = 1;  h[21] = 0; /* PCM */
    h[22] = 1;  h[23] = 0; /* Mono */
    h[24] = sample_rate & 0xFF; h[25] = (sample_rate >> 8) & 0xFF; h[26] = (sample_rate >> 16) & 0xFF; h[27] = (sample_rate >> 24) & 0xFF;
    h[28] = byte_rate & 0xFF;   h[29] = (byte_rate >> 8) & 0xFF;   h[30] = (byte_rate >> 16) & 0xFF;   h[31] = (byte_rate >> 24) & 0xFF;
    h[32] = 2;  h[33] = 0; /* BlockAlign */
    h[34] = 16; h[35] = 0; /* BitsPerSample */
    memcpy(h + 36, "data", 4);
    h[40] = data_bytes & 0xFF; h[41] = (data_bytes >> 8) & 0xFF; h[42] = (data_bytes >> 16) & 0xFF; h[43] = (data_bytes >> 24) & 0xFF;
    fseek(fp, 0, SEEK_SET);
    fwrite(h, 1, 44, fp);
}

static void draw_ai_progress(const char *stage, const char *detail, int pct, bool full_clean)
{
    gfx_clear(GFX_WHITE);
    gfx_draw_top_bar("★ AI 魔法故事工坊 · 生成中", g_prof.stars, app_eye_mins_left());

    gfx_round_rect(60, 74, GFX_W - 120, 332, 20, 3, GFX_BLACK);
    gfx_draw_art(185, 236, 165, ART_GACHA_MAGIC, GFX_BLACK);

    gfx_draw_text(305, 118, stage, FONT_24, GFX_BLACK);
    gfx_draw_paragraph(305, 168, 400, 108, 10, detail, FONT_16, GFX_BLACK, NULL);

    gfx_round_rect(305, 296, 390, 26, 12, 2, GFX_BLACK);
    if (pct > 0) {
        int pw = (382 * (pct > 100 ? 100 : pct)) / 100;
        if (pw < 12) pw = 12;
        gfx_fill_round_rect(309, 300, pw, 18, 8, GFX_BLACK);
    }
    gfx_draw_text(305, 342, "生成完毕将自动断开 Wi-Fi 零辐射护眼阅读", FONT_16, GFX_BLACK);
    gfx_draw_bottom_hints("AI 故事精灵创作中", "请稍候片刻", "按返回可中止");
    bsp_epd_smart_present(gfx_get_fb(), full_clean);
}

/* === HTTP Collector Helper === */
typedef struct {
    char  *buf;
    size_t len;
    size_t cap;
} http_mem_t;

static esp_err_t http_mem_ev_cb(esp_http_client_event_t *evt)
{
    http_mem_t *m = (http_mem_t *)evt->user_data;
    if (evt->event_id == HTTP_EVENT_ON_DATA && m && m->buf && evt->data_len > 0) {
        size_t avail = (m->len + 1 < m->cap) ? (m->cap - 1 - m->len) : 0;
        size_t cp = ((size_t)evt->data_len < avail) ? (size_t)evt->data_len : avail;
        if (cp > 0) {
            memcpy(m->buf + m->len, evt->data, cp);
            m->len += cp;
            m->buf[m->len] = '\0';
        }
    }
    return ESP_OK;
}

/* === Smart Story Text Parser (Tagged + Freeform Fallback) === */
static const char *skip_colon(const char *s)
{
    while (*s == ' ' || *s == '\t') s++;
    if (*s == ':') return s + 1;
    /* UTF-8 full-width colon '：' = E F B C 9 A */
    if ((unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBC && (unsigned char)s[2] == 0x9A) {
        return s + 3;
    }
    return s;
}

static void trim_inplace(char *s)
{
    size_t l = strlen(s);
    while (l > 0 && (s[l - 1] == '\r' || s[l - 1] == '\n' || s[l - 1] == ' ' || s[l - 1] == '\t')) {
        s[--l] = '\0';
    }
    char *p = s;
    while (*p == ' ' || *p == '\t' || *p == '*') p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
}

static void split_long_text_into_pages(const char *raw, ai_story_doc_t *doc)
{
    doc->page_count = 0;
    const char *p = raw;
    while (*p && doc->page_count < MAX_AI_PAGES) {
        while (*p == '\n' || *p == '\r' || *p == ' ') p++;
        if (!*p) break;

        /* Take up to ~250 bytes (~80-85 Chinese chars) breaking cleanly on sentence punctuation */
        size_t max_b = 270;
        size_t slen = strlen(p);
        size_t take = (slen <= max_b) ? slen : max_b;

        if (slen > max_b) {
            /* Walk back to UTF-8 boundary and sentence end (。！？\n) */
            size_t best = 0;
            for (size_t i = 90; i + 2 < max_b; i++) {
                if (p[i] == '\n') { best = i + 1; break; }
                unsigned char b0 = (unsigned char)p[i];
                unsigned char b1 = (unsigned char)p[i + 1];
                unsigned char b2 = (unsigned char)p[i + 2];
                /* 。(E3 80 82) ！(EF BC 81) ？(EF BC 9F) ”(E2 80 9D) */
                if ((b0 == 0xE3 && b1 == 0x80 && b2 == 0x82) ||
                    (b0 == 0xEF && b1 == 0xBC && (b2 == 0x81 || b2 == 0x9F))) {
                    best = i + 3;
                    /* Include closing quote ” if immediately following */
                    if ((unsigned char)p[best] == 0xE2 && (unsigned char)p[best + 1] == 0x80 && (unsigned char)p[best + 2] == 0x9D) {
                        best += 3;
                    }
                }
            }
            if (best > 0) {
                take = best;
            } else {
                while (take > 0 && ((unsigned char)p[take] & 0xC0) == 0x80) take--;
            }
        }

        size_t cp = (take < sizeof(doc->pages[0]) - 1) ? take : (sizeof(doc->pages[0]) - 1);
        memcpy(doc->pages[doc->page_count], p, cp);
        doc->pages[doc->page_count][cp] = '\0';
        trim_inplace(doc->pages[doc->page_count]);
        if (doc->pages[doc->page_count][0]) doc->page_count++;
        p += take;
    }
    if (doc->page_count == 0) {
        strcpy(doc->pages[0], "星光洒在窗台，故事精灵轻轻唱起晚安曲，祝宝贝做一个甜甜的好梦。");
        doc->page_count = 1;
    }
}

static void parse_llm_story_output(const char *content, const char *def_title, ai_story_doc_t *doc)
{
    strncpy(doc->title, def_title, sizeof(doc->title) - 1);
    doc->page_count = 0;
    doc->has_quiz = false;
    doc->quiz_ans = 0;

    char *work = strdup(content);
    if (!work) {
        split_long_text_into_pages(content, doc);
        return;
    }

    char freeform_accum[2600] = {0};
    char *saveptr = NULL;
    for (char *line = strtok_r(work, "\n", &saveptr); line != NULL; line = strtok_r(NULL, "\n", &saveptr)) {
        trim_inplace(line);
        if (!line[0] || strncmp(line, "```", 3) == 0) continue;

        if (strncmp(line, "标题", 6) == 0) {
            const char *v = skip_colon(line + 6);
            if (*v) strncpy(doc->title, v, sizeof(doc->title) - 1);
            trim_inplace(doc->title);
        } else if ((strncmp(line, "第一页", 9) == 0 || strncmp(line, "第1页", 7) == 0 ||
                    strncmp(line, "第二页", 9) == 0 || strncmp(line, "第2页", 7) == 0 ||
                    strncmp(line, "第三页", 9) == 0 || strncmp(line, "第3页", 7) == 0 ||
                    strncmp(line, "第四页", 9) == 0 || strncmp(line, "第4页", 7) == 0 ||
                    strncmp(line, "第五页", 9) == 0 || strncmp(line, "第5页", 7) == 0) &&
                   doc->page_count < MAX_AI_PAGES) {
            const char *colon = strstr(line, "：");
            if (!colon) colon = strchr(line, ':');
            const char *v = colon ? skip_colon(colon) : line;
            if (*v) {
                strncpy(doc->pages[doc->page_count], v, sizeof(doc->pages[0]) - 1);
                trim_inplace(doc->pages[doc->page_count]);
                if (doc->pages[doc->page_count][0]) doc->page_count++;
            }
        } else if (strncmp(line, "问题", 6) == 0) {
            const char *v = skip_colon(line + 6);
            snprintf(doc->quiz_q, sizeof(doc->quiz_q), "读后小问答：%s", v);
            trim_inplace(doc->quiz_q);
        } else if (strncmp(line, "选项A", 7) == 0 || strncmp(line, "A.", 2) == 0 || strncmp(line, "A、", 4) == 0) {
            const char *colon = strstr(line, "：");
            if (!colon) colon = strchr(line, ':');
            const char *v = colon ? skip_colon(colon) : (line + 2);
            snprintf(doc->quiz_opts[0], sizeof(doc->quiz_opts[0]), "A. %s", v);
        } else if (strncmp(line, "选项B", 7) == 0 || strncmp(line, "B.", 2) == 0 || strncmp(line, "B、", 4) == 0) {
            const char *colon = strstr(line, "：");
            if (!colon) colon = strchr(line, ':');
            const char *v = colon ? skip_colon(colon) : (line + 2);
            snprintf(doc->quiz_opts[1], sizeof(doc->quiz_opts[1]), "B. %s", v);
        } else if (strncmp(line, "选项C", 7) == 0 || strncmp(line, "C.", 2) == 0 || strncmp(line, "C、", 4) == 0) {
            const char *colon = strstr(line, "：");
            if (!colon) colon = strchr(line, ':');
            const char *v = colon ? skip_colon(colon) : (line + 2);
            snprintf(doc->quiz_opts[2], sizeof(doc->quiz_opts[2]), "C. %s", v);
        } else if (strncmp(line, "答案", 6) == 0) {
            const char *v = skip_colon(line + 6);
            while (*v == ' ') v++;
            if (*v == 'B' || *v == 'b') doc->quiz_ans = 1;
            else if (*v == 'C' || *v == 'c') doc->quiz_ans = 2;
            else doc->quiz_ans = 0;
        } else {
            if (strlen(freeform_accum) + strlen(line) + 3 < sizeof(freeform_accum)) {
                strcat(freeform_accum, line);
                strcat(freeform_accum, "\n");
            }
        }
    }
    free(work);

    if (doc->page_count == 0) {
        split_long_text_into_pages(freeform_accum[0] ? freeform_accum : content, doc);
    }
    if (doc->quiz_q[0] && doc->quiz_opts[0][0] && doc->quiz_opts[1][0] && doc->quiz_opts[2][0]) {
        doc->has_quiz = true;
    }
}

static uint32_t calc_text_speech_units(const char *s)
{
    uint32_t u = 8;
    while (s && *s) {
        unsigned char b0 = (unsigned char)s[0];
        if (b0 < 0x80) {
            if (*s == '\n') u += 18;
            else if (*s != ' ' && *s != '\r') u += 4;
            s += 1;
        } else if ((b0 & 0xF0) == 0xE0 && s[1] && s[2]) {
            unsigned char b1 = (unsigned char)s[1], b2 = (unsigned char)s[2];
            if ((b0 == 0xE3 && b1 == 0x80 && b2 == 0x82) ||
                (b0 == 0xEF && b1 == 0xBC && (b2 == 0x81 || b2 == 0x9F))) {
                u += 24;
            } else if ((b0 == 0xEF && b1 == 0xBC && b2 == 0x8C) ||
                       (b0 == 0xE3 && b1 == 0x80 && b2 == 0x81)) {
                u += 12;
            } else {
                u += 10;
            }
            s += 3;
        } else {
            s += 1;
        }
    }
    return u;
}

static void compute_weighted_page_permille(ai_story_doc_t *doc, bool title_spoken_first)
{
    if (doc->page_count <= 0) return;
    uint32_t units[MAX_AI_PAGES] = {0};
    uint32_t total = title_spoken_first ? (calc_text_speech_units(doc->title) + 24) : 0;
    for (int i = 0; i < doc->page_count; i++) {
        units[i] = calc_text_speech_units(doc->pages[i]);
        total += units[i];
    }
    uint32_t cum = title_spoken_first ? (calc_text_speech_units(doc->title) + 24) : 0;
    doc->page_start_permille[0] = 0;
    for (int i = 1; i < doc->page_count; i++) {
        cum += units[i - 1];
        doc->page_start_permille[i] = (total > 0) ? (uint16_t)(cum * 1000ULL / total) : 0;
    }
}

static void save_story_txt_to_disk(ai_story_doc_t *doc)
{
    mkdir(bsp_storage_get_stories_dir(), 0777);
    g_net_cfg.ai_story_seq++;
    app_net_cfg_save();

    char clean_t[64];
    clean_filename_stem(doc->title, clean_t, sizeof(clean_t));
    snprintf(doc->txt_path, sizeof(doc->txt_path), "%s/AI_%02lu_%s.txt",
             bsp_storage_get_stories_dir(), (unsigned long)(g_net_cfg.ai_story_seq % 100), clean_t);
    snprintf(doc->wav_path, sizeof(doc->wav_path), "%s/AI_%02lu_%s.wav",
             bsp_storage_get_stories_dir(), (unsigned long)(g_net_cfg.ai_story_seq % 100), clean_t);

    FILE *fp = fopen(doc->txt_path, "wb");
    if (!fp) return;
    fprintf(fp, "《%s》\n\n", doc->title);
    for (int i = 0; i < doc->page_count; i++) {
        fprintf(fp, "%s\n\n", doc->pages[i]);
    }
    fclose(fp);
}

/* =========================================================================
 * Cloud HTTP TTS (/v1/audio/speech -> 16kHz/24kHz WAV)
 * ========================================================================= */
static bool synthesize_cloud_tts_wav(ai_story_doc_t *doc)
{
    if (!g_net_cfg.tts_enabled || g_net_cfg.tts_url[0] == '\0') return false;
    const char *tkey = g_net_cfg.tts_key[0] ? g_net_cfg.tts_key : g_net_cfg.ai_key;

    char full_txt[2048] = {0};
    snprintf(full_txt, sizeof(full_txt), "%s。", doc->title);
    for (int i = 0; i < doc->page_count; i++) {
        if (strlen(full_txt) + strlen(doc->pages[i]) + 4 < sizeof(full_txt)) {
            strcat(full_txt, doc->pages[i]);
            strcat(full_txt, "\n");
        }
    }

    cJSON *req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "model", g_net_cfg.tts_model[0] ? g_net_cfg.tts_model : "FunAudioLLM/CosyVoice2-0.5B");
    cJSON_AddStringToObject(req, "input", full_txt);
    cJSON_AddStringToObject(req, "voice", g_net_cfg.tts_voice[0] ? g_net_cfg.tts_voice : "FunAudioLLM/CosyVoice2-0.5B:anna");
    cJSON_AddStringToObject(req, "response_format", "wav");
    cJSON_AddNumberToObject(req, "sample_rate", 16000);
    char *body = cJSON_PrintUnformatted(req);
    cJSON_Delete(req);
    if (!body) return false;

    esp_http_client_config_t hcfg = {
        .url = g_net_cfg.tts_url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 35000,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .buffer_size = 2048,
        .buffer_size_tx = 2048,
    };
    esp_http_client_handle_t cli = esp_http_client_init(&hcfg);
    if (!cli) { free(body); return false; }

    char auth[180];
    snprintf(auth, sizeof(auth), "Bearer %s", tkey);
    esp_http_client_set_header(cli, "Content-Type", "application/json");
    if (tkey[0]) esp_http_client_set_header(cli, "Authorization", auth);

    bool ok = false;
    if (esp_http_client_open(cli, (int)strlen(body)) == ESP_OK) {
        esp_http_client_write(cli, body, (int)strlen(body));
        esp_http_client_fetch_headers(cli);
        int status = esp_http_client_get_status_code(cli);
        if (status == 200) {
            FILE *fp = fopen(doc->wav_path, "wb");
            uint8_t *chunk = (uint8_t *)malloc(4096);
            if (fp && chunk) {
                uint32_t total = 0;
                bool is_riff = false;
                while (1) {
                    int r = esp_http_client_read(cli, (char *)chunk, 4096);
                    if (r <= 0) break;
                    if (total == 0) {
                        if (r >= 4 && memcmp(chunk, "RIFF", 4) == 0) {
                            is_riff = true;
                        } else {
                            /* Prepend WAV header if endpoint returned raw 16kHz PCM */
                            write_wav_header(fp, 16000, 0);
                        }
                    }
                    fwrite(chunk, 1, (size_t)r, fp);
                    total += (uint32_t)r;
                }
                if (total > 512) {
                    if (!is_riff) write_wav_header(fp, 16000, total);
                    ok = true;
                }
            }
            if (chunk) free(chunk);
            if (fp) fclose(fp);
            if (!ok) unlink(doc->wav_path);
        }
    }
    esp_http_client_cleanup(cli);
    free(body);
    doc->has_wav = ok;
    return ok;
}

/* =========================================================================
 * ENGINE B: OpenAI-Compatible Cloud LLM (DeepSeek / Qwen / Doubao / SiliconFlow)
 * ========================================================================= */
static bool generate_via_cloud_llm(int hero_i, int scene_i, int theme_i, ai_story_doc_t *doc, char *err_msg, size_t err_sz)
{
    if (g_net_cfg.ai_url[0] == '\0') {
        snprintf(err_msg, err_sz, "尚未填写大模型接口地址，请在配网页面设置。");
        return false;
    }

    char hero_s[64];
    get_hero_display(hero_i, hero_s, sizeof(hero_s));
    const char *cname = g_net_cfg.child_name[0] ? g_net_cfg.child_name : "宝贝";

    char sys_p[520];
    snprintf(sys_p, sizeof(sys_p),
             "你是一位温暖的儿童绘本作家。请为%s（昵称：%s）创作一篇积极阳光、零恐怖元素的睡前绘本故事，并附一道三选一互动问答。"
             "必须严格按以下纯文本标签格式输出（每项一行，不要Markdown符号）：\n"
             "标题：故事名\n"
             "第一页：约75字正文\n"
             "第二页：约75字正文\n"
             "第三页：约75字正文\n"
             "第四页：约75字温馨结尾\n"
             "问题：读后思考题\n"
             "选项A：内容\n"
             "选项B：内容\n"
             "选项C：内容\n"
             "答案：A或B或C",
             g_net_cfg.age_mode ? "6-12岁学龄儿童（词汇生动、富含科学常识与想象力）" : "3-6岁学龄前幼儿（句式短小柔和、多用可爱拟声词）",
             cname);

    char usr_p[280];
    snprintf(usr_p, sizeof(usr_p),
             "故事三要素扭蛋：主角是【%s】，奇幻场景在【%s】，成长主题是【%s】。请自然地把小朋友“%s”的名字带入故事祝福中！",
             hero_s, k_scenes[scene_i], k_themes[theme_i], cname);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "model", g_net_cfg.ai_model[0] ? g_net_cfg.ai_model : "deepseek-chat");
    cJSON_AddNumberToObject(root, "temperature", 0.8);
    cJSON_AddNumberToObject(root, "max_tokens", 900);
    cJSON_AddBoolToObject(root, "stream", false);
    cJSON *msgs = cJSON_AddArrayToObject(root, "messages");
    cJSON *m0 = cJSON_CreateObject();
    cJSON_AddStringToObject(m0, "role", "system");
    cJSON_AddStringToObject(m0, "content", sys_p);
    cJSON_AddItemToArray(msgs, m0);
    cJSON *m1 = cJSON_CreateObject();
    cJSON_AddStringToObject(m1, "role", "user");
    cJSON_AddStringToObject(m1, "content", usr_p);
    cJSON_AddItemToArray(msgs, m1);

    char *req_json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!req_json) return false;

    http_mem_t rx = {
        .cap = 24576,
        .len = 0,
        .buf = (char *)heap_caps_calloc(1, 24576, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT),
    };
    if (!rx.buf) rx.buf = (char *)calloc(1, 16384);
    if (!rx.buf) { free(req_json); return false; }

    draw_ai_progress("② 正在召唤云端大模型创作绘本...", "根据宝贝摇出的主角、场景与主题编织专属故事...", 45, false);

    esp_http_client_config_t cfg = {
        .url = g_net_cfg.ai_url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 38000,
        .event_handler = http_mem_ev_cb,
        .user_data = &rx,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .buffer_size = 2048,
        .buffer_size_tx = 2048,
    };
    esp_http_client_handle_t cli = esp_http_client_init(&cfg);
    char auth[180];
    snprintf(auth, sizeof(auth), "Bearer %s", g_net_cfg.ai_key);
    esp_http_client_set_header(cli, "Content-Type", "application/json");
    if (g_net_cfg.ai_key[0]) esp_http_client_set_header(cli, "Authorization", auth);
    esp_http_client_set_post_field(cli, req_json, (int)strlen(req_json));

    esp_err_t err = esp_http_client_perform(cli);
    int status = esp_http_client_get_status_code(cli);
    esp_http_client_cleanup(cli);
    free(req_json);

    if (err != ESP_OK || status != 200) {
        snprintf(err_msg, err_sz, "接口请求失败 (HTTP %d)，请核对 API Key 与模型名。", status);
        free(rx.buf);
        return false;
    }

    cJSON *resp = cJSON_Parse(rx.buf);
    free(rx.buf);
    if (!resp) {
        snprintf(err_msg, err_sz, "返回数据解析失败，请检查接口地址是否为兼容格式。");
        return false;
    }

    const char *content = NULL;
    cJSON *choices = cJSON_GetObjectItem(resp, "choices");
    if (cJSON_IsArray(choices) && cJSON_GetArraySize(choices) > 0) {
        cJSON *c0 = cJSON_GetArrayItem(choices, 0);
        cJSON *msg = cJSON_GetObjectItem(c0, "message");
        cJSON *cnt = msg ? cJSON_GetObjectItem(msg, "content") : NULL;
        if (cJSON_IsString(cnt)) content = cnt->valuestring;
    }
    if (!content || !content[0]) {
        cJSON_Delete(resp);
        snprintf(err_msg, err_sz, "大模型未返回故事内容，请稍后重试。");
        return false;
    }

    char def_title[96];
    snprintf(def_title, sizeof(def_title), "%s奇遇记", hero_s);
    doc->art = k_heroes[hero_i].art;
    parse_llm_story_output(content, def_title, doc);
    cJSON_Delete(resp);

    save_story_txt_to_disk(doc);
    compute_weighted_page_permille(doc, true);

    if (g_net_cfg.tts_enabled && g_net_cfg.tts_url[0]) {
        draw_ai_progress("③ 正在合成温柔伴读语音 (.wav)...", "即将保存到本机书架，下次离线也能直接听！", 80, false);
        synthesize_cloud_tts_wav(doc);
    }
    return true;
}

/* =========================================================================
 * ENGINE A: Native Xiaozhi AI (6-Digit OTA Activation + WSS Opus + Text Stream)
 * ========================================================================= */
static bool parse_ws_url(const char *url, bool *is_ssl, char *host, size_t host_sz, int *port, char *path, size_t path_sz)
{
    *is_ssl = true;
    *port = 443;
    const char *p = url;
    if (strncmp(url, "wss://", 6) == 0) {
        *is_ssl = true; *port = 443; p = url + 6;
    } else if (strncmp(url, "ws://", 5) == 0) {
        *is_ssl = false; *port = 80; p = url + 5;
    }
    const char *slash = strchr(p, '/');
    size_t hlen = slash ? (size_t)(slash - p) : strlen(p);
    char hostport[128] = {0};
    if (hlen >= sizeof(hostport)) hlen = sizeof(hostport) - 1;
    memcpy(hostport, p, hlen);

    char *colon = strchr(hostport, ':');
    if (colon) {
        *colon = '\0';
        *port = atoi(colon + 1);
    }
    strncpy(host, hostport, host_sz - 1);
    strncpy(path, slash ? slash : "/xiaozhi/v1/", path_sz - 1);
    return host[0] != '\0';
}

static bool show_xiaozhi_activation_card(const char *code6, const char *mac_str)
{
    bsp_audio_sfx(SFX_CONFIRM);
    gfx_clear(GFX_WHITE);
    gfx_draw_top_bar("★ 小智 AI 首次绑定激活 (终身免填 API Key)", g_prof.stars, app_eye_mins_left());

    /* Left QR Card -> https://xiaozhi.me */
    gfx_round_rect(22, 54, 310, 366, 16, 2, GFX_BLACK);
    gfx_draw_text_center(177, 68, "手机扫码打开小智后台", FONT_24, GFX_BLACK);
    gfx_draw_qrcode(90, 108, 5, "https://xiaozhi.me");
    gfx_draw_text_center(177, 298, "网址：https://xiaozhi.me", FONT_16, GFX_BLACK);
    char mline[48];
    snprintf(mline, sizeof(mline), "设备MAC：%s", mac_str);
    gfx_draw_text_center(177, 330, mline, FONT_16, GFX_BLACK);

    /* Right 6-Digit Activation Code Card */
    gfx_round_rect(350, 54, 428, 366, 16, 3, GFX_BLACK);
    gfx_draw_text_center(564, 72, "本机 6 位绑定验证码", FONT_24, GFX_BLACK);

    gfx_fill_round_rect(390, 114, 348, 72, 14, GFX_BLACK);
    char spaced[32] = {0};
    size_t sp = 0;
    for (size_t i = 0; code6[i] && sp + 3 < sizeof(spaced); i++) {
        spaced[sp++] = code6[i];
        if (code6[i + 1]) spaced[sp++] = ' ';
    }
    gfx_draw_text_center(564, 134, spaced, FONT_32, GFX_WHITE);

    gfx_draw_paragraph(378, 206, 372, 195, 10,
        "三步完成小智 AI 绑定：\n"
        "1. 手机打开 xiaozhi.me 注册/登录控制台\n"
        "2. 点击『添加设备』，输入上方 6 位数字\n"
        "3. 网页提示成功后，按下本机滚轮立即开始讲故事！",
        FONT_16, GFX_BLACK, NULL);

    gfx_draw_bottom_hints("在网页输入6位验证码", "绑好后按下滚轮继续", "返回扭蛋屋");
    bsp_epd_smart_present(gfx_get_fb(), true);

    while (1) {
        bsp_key_t k = bsp_input_wait(30000);
        if (k == KEY_BACK) return false;
        if (k == KEY_OK) return true;
    }
}

/* Safe WebSocket text sender: esp_transport_ws_send_raw XOR-masks payload IN-PLACE,
   so passing a .rodata literal directly causes a StoreProhibited panic! Always copy to RAM. */
static int ws_send_text_ram(esp_transport_handle_t ws, const char *txt)
{
    if (!ws || !txt) return -1;
    size_t len = strlen(txt);
    char *ram = (char *)malloc(len + 1);
    if (!ram) return -1;
    memcpy(ram, txt, len + 1);
    int r = esp_transport_ws_send_raw(ws, WS_TRANSPORT_OPCODES_TEXT | WS_TRANSPORT_OPCODES_FIN,
                                      ram, (int)len, 5000);
    free(ram);
    return r;
}

static uint32_t xz_utf8_next(const char **p)
{
    const uint8_t *s = (const uint8_t *)(*p);
    if (!*s) return 0;
    if (*s < 0x80) { *p += 1; return *s; }
    if ((*s & 0xE0) == 0xC0 && s[1]) { *p += 2; return ((uint32_t)(s[0] & 0x1F) << 6) | (s[1] & 0x3F); }
    if ((*s & 0xF0) == 0xE0 && s[1] && s[2]) {
        *p += 3;
        return ((uint32_t)(s[0] & 0x0F) << 12) | ((uint32_t)(s[1] & 0x3F) << 6) | (s[2] & 0x3F);
    }
    *p += 1;
    return 0;
}

/* Stream clean 16kHz 60ms Opus speech frames synthesized from our built-in Mandarin voice bank
   so Xiaozhi servers that require spoken audio after wake-up receive a studio-clear prompt! */
static void xz_send_spoken_opus_prompt(esp_transport_handle_t ws, const char *session_id, const char *prompt_utf8)
{
    int dec_err = 0, enc_err = 0;
    OpusDecoder *dec = opus_decoder_create(16000, 1, &dec_err);
    OpusEncoder *enc = opus_encoder_create(16000, 1, OPUS_APPLICATION_VOIP, &enc_err);
    opus_int16 *frame960 = (opus_int16 *)calloc(960, sizeof(opus_int16));
    opus_int16 *tmp320   = (opus_int16 *)malloc(320 * sizeof(opus_int16));
    char       *pkt_out  = (char *)malloc(256);

    if (!dec || !enc || !frame960 || !tmp320 || !pkt_out) {
        if (dec) opus_decoder_destroy(dec);
        if (enc) opus_encoder_destroy(enc);
        if (frame960) free(frame960);
        if (tmp320) free(tmp320);
        if (pkt_out) free(pkt_out);
        return;
    }
    opus_encoder_ctl(enc, OPUS_SET_BITRATE(16000));
    opus_encoder_ctl(enc, OPUS_SET_COMPLEXITY(2));

    char ctrl[192];
    snprintf(ctrl, sizeof(ctrl), "{\"session_id\":\"%s\",\"type\":\"listen\",\"state\":\"start\",\"mode\":\"manual\"}", session_id);
    ws_send_text_ram(ws, ctrl);

    int fill = 0;
    const char *p = prompt_utf8;
    while (*p) {
        uint32_t cp = xz_utf8_next(&p);
        if (cp == 0xFF0C /*，*/ || cp == 0x3002 /*。*/) {
            /* Insert 120ms natural comma pause */
            for (int z = 0; z < 1920; z++) {
                frame960[fill++] = 0;
                if (fill == 960) {
                    int nb = opus_encode(enc, frame960, 960, (unsigned char *)pkt_out, 250);
                    if (nb > 0) {
                        esp_transport_ws_send_raw(ws, WS_TRANSPORT_OPCODES_BINARY | WS_TRANSPORT_OPCODES_FIN, pkt_out, nb, 3000);
                        vTaskDelay(pdMS_TO_TICKS(15));
                    }
                    fill = 0;
                }
            }
            continue;
        }
        if (cp < 0x4E00 || cp > 0x9FA5) continue;

        uint16_t sid = 0xFFFF;
        const uint8_t *clip = NULL;
        uint16_t clip_len = 0;
        if (!gfx_hanzi_lookup((uint16_t)cp, NULL, 0, NULL, &sid)) continue;
        if (!gfx_hanzi_get_opus_clip(sid, &clip, &clip_len) || !clip) continue;

        opus_decoder_ctl(dec, OPUS_RESET_STATE);
        size_t cpos = 0;
        int silent_tail = 0;
        while (cpos < clip_len) {
            uint8_t plen = clip[cpos++];
            if (plen == 0 || cpos + plen > clip_len) break;
            int n = opus_decode(dec, clip + cpos, (opus_int32)plen, tmp320, 320, 0);
            cpos += plen;
            for (int i = 0; i < n; i++) {
                if (abs(tmp320[i]) < 150) silent_tail++;
                else                      silent_tail = 0;
                if (silent_tail > 320) continue; /* Trim long trailing silence for natural cadence */
                frame960[fill++] = tmp320[i];
                if (fill == 960) {
                    int nb = opus_encode(enc, frame960, 960, (unsigned char *)pkt_out, 250);
                    if (nb > 0) {
                        esp_transport_ws_send_raw(ws, WS_TRANSPORT_OPCODES_BINARY | WS_TRANSPORT_OPCODES_FIN, pkt_out, nb, 3000);
                        vTaskDelay(pdMS_TO_TICKS(15));
                    }
                    fill = 0;
                }
            }
        }
    }

    /* Flush remaining samples + 300ms trailing silence for clean ASR endpoint */
    for (int pad = 0; pad < 4800; pad++) {
        frame960[fill++] = 0;
        if (fill == 960) {
            int nb = opus_encode(enc, frame960, 960, (unsigned char *)pkt_out, 250);
            if (nb > 0) {
                esp_transport_ws_send_raw(ws, WS_TRANSPORT_OPCODES_BINARY | WS_TRANSPORT_OPCODES_FIN, pkt_out, nb, 3000);
                vTaskDelay(pdMS_TO_TICKS(15));
            }
            fill = 0;
        }
    }

    snprintf(ctrl, sizeof(ctrl), "{\"session_id\":\"%s\",\"type\":\"listen\",\"state\":\"stop\"}", session_id);
    ws_send_text_ram(ws, ctrl);

    opus_decoder_destroy(dec);
    opus_encoder_destroy(enc);
    free(frame960);
    free(tmp320);
    free(pkt_out);
}

#define MAX_XZ_SENTS  48

typedef struct {
    char     mac_str[18];
    char     ws_url[160];
    char     ws_token[256];
    char     host[96];
    char     path[96];
    char     hdrs[256];
    char     auth_hdr[300];
    char     session_id[64];
    char     hero_s[64];
    char     ask_json[640];
    char     voice_prompt[280];
    char     saved_wav_tmp[192];
    int      sent_cnt;
    uint32_t sent_pcm_off[MAX_XZ_SENTS];
    char     sent_txt[MAX_XZ_SENTS][220];
} xz_work_t;

static bool generate_via_xiaozhi(int hero_i, int scene_i, int theme_i, ai_story_doc_t *doc, char *err_msg, size_t err_sz)
{
    xz_work_t *w = (xz_work_t *)calloc(1, sizeof(xz_work_t));
    if (!w) return false;

    app_get_device_mac_str(w->mac_str);
    strcpy(w->ws_url, "wss://api.tenclass.net/xiaozhi/v1/");
    strcpy(w->ws_token, "test-token");
    const char *ota_url = g_net_cfg.ai_url[0] ? g_net_cfg.ai_url : "https://api.tenclass.net/xiaozhi/ota/";

    /* Step 1: Xiaozhi OTA Check & Activation Loop */
    for (int attempt = 0; attempt < 4; attempt++) {
        draw_ai_progress("② 正在连接小智 AI 服务中心...", "验证设备绑定状态并获取语音故事通道...", 32, false);

        cJSON *ota_req = cJSON_CreateObject();
        cJSON_AddNumberToObject(ota_req, "version", 2);
        cJSON_AddStringToObject(ota_req, "language", "zh-CN");
        cJSON_AddNumberToObject(ota_req, "flash_size", 16777216);
        cJSON_AddStringToObject(ota_req, "mac_address", w->mac_str);
        cJSON_AddStringToObject(ota_req, "uuid", g_net_cfg.xiaozhi_uuid);
        cJSON *app_o = cJSON_AddObjectToObject(ota_req, "application");
        cJSON_AddStringToObject(app_o, "name", "xiaozhi");
        cJSON_AddStringToObject(app_o, "version", "1.6.0");
        cJSON *brd_o = cJSON_AddObjectToObject(ota_req, "board");
        cJSON_AddStringToObject(brd_o, "type", "waveshare-s3-epaper-397");
        cJSON_AddStringToObject(brd_o, "name", "waveshare-s3-epaper-397");
        cJSON_AddStringToObject(brd_o, "ssid", g_net_cfg.wifi_ssid);
        char *ota_body = cJSON_PrintUnformatted(ota_req);
        cJSON_Delete(ota_req);

        http_mem_t rx = { .cap = 8192, .len = 0, .buf = (char *)calloc(1, 8192) };
        esp_http_client_config_t hcfg = {
            .url = ota_url,
            .method = HTTP_METHOD_POST,
            .timeout_ms = 12000,
            .event_handler = http_mem_ev_cb,
            .user_data = &rx,
            .crt_bundle_attach = esp_crt_bundle_attach,
            .buffer_size = 2048,
            .buffer_size_tx = 2048,
        };
        esp_http_client_handle_t cli = esp_http_client_init(&hcfg);
        esp_http_client_set_header(cli, "Content-Type", "application/json");
        esp_http_client_set_header(cli, "Device-Id", w->mac_str);
        esp_http_client_set_header(cli, "Client-Id", g_net_cfg.xiaozhi_uuid);
        esp_http_client_set_header(cli, "Activation-Version", "1");
        esp_http_client_set_header(cli, "Accept-Language", "zh-CN");
        esp_http_client_set_header(cli, "User-Agent", "waveshare-s3-epaper-397/1.6.0");
        esp_http_client_set_post_field(cli, ota_body, (int)strlen(ota_body));
        esp_err_t err = esp_http_client_perform(cli);
        esp_http_client_cleanup(cli);
        free(ota_body);

        if (err != ESP_OK) {
            free(rx.buf);
            free(w);
            snprintf(err_msg, err_sz, "无法连接小智服务器，请检查路由器外网连接。");
            return false;
        }

        ESP_LOGI(TAG, "Xiaozhi OTA resp: %s", rx.buf ? rx.buf : "");
        cJSON *resp = cJSON_Parse(rx.buf);
        free(rx.buf);
        if (!resp) {
            free(w);
            snprintf(err_msg, err_sz, "小智激活服务返回异常，请稍后重试。");
            return false;
        }

        cJSON *ws_o = cJSON_GetObjectItem(resp, "websocket");
        if (ws_o) {
            cJSON *u = cJSON_GetObjectItem(ws_o, "url");
            cJSON *t = cJSON_GetObjectItem(ws_o, "token");
            if (cJSON_IsString(u) && u->valuestring[0]) strncpy(w->ws_url, u->valuestring, sizeof(w->ws_url) - 1);
            if (cJSON_IsString(t) && t->valuestring[0]) strncpy(w->ws_token, t->valuestring, sizeof(w->ws_token) - 1);
        }

        cJSON *act_o = cJSON_GetObjectItem(resp, "activation");
        cJSON *code_o = act_o ? cJSON_GetObjectItem(act_o, "code") : NULL;
        if (cJSON_IsString(code_o) && code_o->valuestring[0]) {
            char code6[16] = {0};
            strncpy(code6, code_o->valuestring, sizeof(code6) - 1);
            cJSON_Delete(resp);
            if (!show_xiaozhi_activation_card(code6, w->mac_str)) {
                free(w);
                snprintf(err_msg, err_sz, "已取消小智绑定激活。");
                return false;
            }
            continue;
        }
        cJSON_Delete(resp);
        break;
    }

    /* Step 2: Open Xiaozhi WSS Stream & Decode Both Story Text + Opus Voiceover */
    bool is_ssl = true;
    int port = 443;
    if (!parse_ws_url(w->ws_url, &is_ssl, w->host, sizeof(w->host), &port, w->path, sizeof(w->path))) {
        free(w);
        snprintf(err_msg, err_sz, "小智 WebSocket 地址格式无效。");
        return false;
    }

    esp_transport_list_handle_t tlist = esp_transport_list_init();
    esp_transport_handle_t parent = is_ssl ? esp_transport_ssl_init() : esp_transport_tcp_init();
    if (is_ssl) {
        esp_transport_ssl_crt_bundle_attach(parent, esp_crt_bundle_attach);
    }
    esp_transport_list_add(tlist, parent, is_ssl ? "ssl" : "tcp");

    snprintf(w->hdrs, sizeof(w->hdrs),
             "Protocol-Version: 1\r\nDevice-Id: %s\r\nClient-Id: %s\r\n",
             w->mac_str, g_net_cfg.xiaozhi_uuid);
    snprintf(w->auth_hdr, sizeof(w->auth_hdr), "Bearer %s", w->ws_token);

    esp_transport_handle_t ws = esp_transport_ws_init(parent);
    esp_transport_ws_config_t wscfg = {
        .ws_path = w->path,
        .user_agent = "waveshare-s3-epaper-397/1.6.0",
        .headers = w->hdrs,
        .auth = w->ws_token[0] ? w->auth_hdr : NULL,
        .propagate_control_frames = false,
    };
    esp_transport_ws_set_config(ws, &wscfg);
    esp_transport_list_add(tlist, ws, "ws");

    draw_ai_progress("③ 小智姐姐正在编故事与配音...", "同步接收故事文字与真人级 Opus 语音流...", 55, false);

    if (esp_transport_connect(ws, w->host, port, 12000) < 0) {
        esp_transport_list_destroy(tlist);
        free(w);
        snprintf(err_msg, err_sz, "连接小智语音通道失败，请确认已在 xiaozhi.me 绑定设备。");
        return false;
    }

    ws_send_text_ram(ws,
        "{\"type\":\"hello\",\"version\":1,\"transport\":\"websocket\","
        "\"audio_params\":{\"format\":\"opus\",\"sample_rate\":16000,\"channels\":1,\"frame_duration\":60}}");

    get_hero_display(hero_i, w->hero_s, sizeof(w->hero_s));
    const char *cname = g_net_cfg.child_name[0] ? g_net_cfg.child_name : "宝贝";
    snprintf(doc->title, sizeof(doc->title), "%s与%s", w->hero_s, k_scenes[scene_i]);
    doc->art = k_heroes[hero_i].art;
    doc->has_quiz = true;
    snprintf(doc->quiz_q, sizeof(doc->quiz_q), "读后小问答：今晚故事里的主角去了哪里冒险呀？");
    snprintf(doc->quiz_opts[0], sizeof(doc->quiz_opts[0]), "A. %s", k_scenes[scene_i]);
    snprintf(doc->quiz_opts[1], sizeof(doc->quiz_opts[1]), "B. %s", k_scenes[(scene_i + 5) % GACHA_SCENE_CNT]);
    snprintf(doc->quiz_opts[2], sizeof(doc->quiz_opts[2]), "C. %s", k_scenes[(scene_i + 11) % GACHA_SCENE_CNT]);
    doc->quiz_ans = 0;

    mkdir(bsp_storage_get_stories_dir(), 0777);
    char clean_t[64];
    clean_filename_stem(doc->title, clean_t, sizeof(clean_t));
    snprintf(doc->wav_path, sizeof(doc->wav_path), "%s/AI_XZ_%s.wav", bsp_storage_get_stories_dir(), clean_t);
    FILE *wfp = fopen(doc->wav_path, "wb");
    if (wfp) write_wav_header(wfp, 16000, 0);

    uint32_t srv_sr = 16000;
    OpusDecoder *opus_dec = NULL;
    opus_int16 *pcm_buf = (opus_int16 *)heap_caps_malloc(2880 * sizeof(opus_int16), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!pcm_buf) pcm_buf = (opus_int16 *)malloc(2880 * sizeof(opus_int16));
    char *ws_rx = (char *)malloc(8192);
    char *story_accum = (char *)calloc(1, 6144);

    bool prompt_sent = false;
    bool voice_fallback_sent = false;
    bool got_tts_stop = false;
    uint32_t pcm_bytes = 0;
    int opus_pkts = 0;
    int idle_timeouts = 0;
    int64_t deadline_us = esp_timer_get_time() + 85000000LL;

    while (ws_rx && story_accum && esp_timer_get_time() < deadline_us && !got_tts_stop) {
        if (bsp_input_wait(0) == KEY_BACK) break;

        int rlen = esp_transport_read(ws, ws_rx, 8190, 1500);
        if (rlen == 0) {
            idle_timeouts++;
            /* If server never responded to text detect within 4.5s, send spoken Opus prompt! */
            if (prompt_sent && !voice_fallback_sent && !story_accum[0] && idle_timeouts >= 3) {
                voice_fallback_sent = true;
                idle_timeouts = 0;
                draw_ai_progress("③ 正在向小智发送语音点播故事...", w->voice_prompt, 58, false);
                xz_send_spoken_opus_prompt(ws, w->session_id, w->voice_prompt);
                continue;
            }
            if (prompt_sent && strlen(story_accum) >= 96 && opus_pkts > 20 && idle_timeouts >= 3) {
                break;
            }
            continue;
        } else if (rlen < 0) {
            break;
        }
        idle_timeouts = 0;

        ws_transport_opcodes_t op = esp_transport_ws_get_read_opcode(ws);
        if (op == WS_TRANSPORT_OPCODES_TEXT) {
            ws_rx[rlen] = '\0';
            ESP_LOGI(TAG, "XZ WS text: %s", ws_rx);
            cJSON *j = cJSON_Parse(ws_rx);
            if (!j) continue;
            cJSON *tp = cJSON_GetObjectItem(j, "type");
            const char *tstr = cJSON_IsString(tp) ? tp->valuestring : "";

            if (strcmp(tstr, "hello") == 0 && !prompt_sent) {
                cJSON *ap = cJSON_GetObjectItem(j, "audio_params");
                cJSON *sr_o = ap ? cJSON_GetObjectItem(ap, "sample_rate") : NULL;
                if (cJSON_IsNumber(sr_o) && (sr_o->valueint == 16000 || sr_o->valueint == 24000)) {
                    srv_sr = (uint32_t)sr_o->valueint;
                }
                int oerr = 0;
                opus_dec = opus_decoder_create((opus_int32)srv_sr, 1, &oerr);

                cJSON *sid_o = cJSON_GetObjectItem(j, "session_id");
                if (cJSON_IsString(sid_o)) strncpy(w->session_id, sid_o->valuestring, sizeof(w->session_id) - 1);

                snprintf(w->voice_prompt, sizeof(w->voice_prompt),
                         "请给%s讲一个两百字儿童睡前故事，主角是%s，地点在%s，学会%s，直接开始讲故事。",
                         cname, w->hero_s, k_scenes[scene_i], k_themes[theme_i]);
                snprintf(w->ask_json, sizeof(w->ask_json),
                         "{\"session_id\":\"%s\",\"type\":\"listen\",\"state\":\"detect\",\"text\":\"%s\"}",
                         w->session_id, w->voice_prompt);
                ws_send_text_ram(ws, w->ask_json);
                prompt_sent = true;
            } else if (strcmp(tstr, "tts") == 0) {
                cJSON *st = cJSON_GetObjectItem(j, "state");
                const char *sstr = cJSON_IsString(st) ? st->valuestring : "";
                if (strcmp(sstr, "sentence_start") == 0) {
                    cJSON *tx = cJSON_GetObjectItem(j, "text");
                    if (cJSON_IsString(tx) && tx->valuestring[0]) {
                        if (w->sent_cnt < MAX_XZ_SENTS) {
                            w->sent_pcm_off[w->sent_cnt] = pcm_bytes;
                            strncpy(w->sent_txt[w->sent_cnt], tx->valuestring, sizeof(w->sent_txt[0]) - 1);
                            w->sent_cnt++;
                        }
                        if (strlen(story_accum) + strlen(tx->valuestring) + 4 < 6100) {
                            strcat(story_accum, tx->valuestring);
                            strcat(story_accum, "\n");
                        }
                    }
                } else if (strcmp(sstr, "stop") == 0) {
                    /* Check if server only spoke a short wake greeting (<96 UTF-8 bytes ~30 chars);
                       if so, send full spoken Opus Push-to-Talk story request! */
                    if (strlen(story_accum) < 96 && !voice_fallback_sent) {
                        voice_fallback_sent = true;
                        story_accum[0] = '\0';
                        w->sent_cnt = 0;
                        pcm_bytes = 0;
                        opus_pkts = 0;
                        if (wfp) write_wav_header(wfp, srv_sr, 0);
                        draw_ai_progress("③ 小智已唤醒，正在语音点播专属故事...", w->voice_prompt, 62, false);
                        xz_send_spoken_opus_prompt(ws, w->session_id, w->voice_prompt);
                    } else if (story_accum[0]) {
                        got_tts_stop = true;
                    }
                }
            }
            cJSON_Delete(j);
        } else if (op == WS_TRANSPORT_OPCODES_BINARY && opus_dec && pcm_buf && wfp) {
            int max_samples = (srv_sr == 24000) ? 2880 : 1920;
            int n_samp = opus_decode(opus_dec, (const unsigned char *)ws_rx, (opus_int32)rlen, pcm_buf, max_samples, 0);
            if (n_samp > 0) {
                size_t wb = (size_t)n_samp * sizeof(opus_int16);
                fwrite(pcm_buf, 1, wb, wfp);
                pcm_bytes += (uint32_t)wb;
                opus_pkts++;
                if ((opus_pkts % 70) == 0) {
                    int pct = 64 + (opus_pkts / 12);
                    if (pct > 96) pct = 96;
                    draw_ai_progress("③ 正在接收小智真人语音伴读...", story_accum[0] ? story_accum : "故事音频流同步落盘中...", pct, false);
                }
            }
        }
    }

    if (opus_dec) opus_decoder_destroy(opus_dec);
    if (pcm_buf) free(pcm_buf);
    if (ws_rx) free(ws_rx);
    esp_transport_list_destroy(tlist);

    bool ok = (story_accum && strlen(story_accum) > 24);
    if (ok) {
        /* Pack Xiaozhi sentences into pages while preserving exact millisecond PCM byte offsets! */
        if (w->sent_cnt >= 2 && pcm_bytes > 1600) {
            doc->page_count = 0;
            uint32_t page_pcm_off[MAX_AI_PAGES] = {0};
            for (int si = 0; si < w->sent_cnt; si++) {
                const char *stxt = w->sent_txt[si];
                size_t sl = strlen(stxt);
                if (doc->page_count == 0) {
                    doc->page_count = 1;
                    doc->pages[0][0] = '\0';
                    page_pcm_off[0] = 0;
                }
                int cur_p = doc->page_count - 1;
                size_t cur_l = strlen(doc->pages[cur_p]);
                /* ~245 UTF-8 bytes (~80 CJK chars) per page fits comfortably in FONT_24 */
                if (cur_l > 0 && cur_l + sl + 2 > 245 && doc->page_count < MAX_AI_PAGES) {
                    cur_p = doc->page_count++;
                    doc->pages[cur_p][0] = '\0';
                    page_pcm_off[cur_p] = w->sent_pcm_off[si];
                    cur_l = 0;
                }
                if (cur_l + sl + 3 < sizeof(doc->pages[0])) {
                    if (cur_l > 0) strcat(doc->pages[cur_p], "\n");
                    strcat(doc->pages[cur_p], stxt);
                }
            }
            for (int pi = 0; pi < doc->page_count; pi++) {
                doc->page_start_permille[pi] = (uint16_t)((uint64_t)page_pcm_off[pi] * 1000ULL / pcm_bytes);
            }
        } else {
            split_long_text_into_pages(story_accum, doc);
            compute_weighted_page_permille(doc, false);
        }

        strncpy(w->saved_wav_tmp, doc->wav_path, sizeof(w->saved_wav_tmp) - 1);
        save_story_txt_to_disk(doc);
        if (wfp) {
            if (pcm_bytes > 1600) {
                write_wav_header(wfp, srv_sr, pcm_bytes);
                fclose(wfp);
                rename(w->saved_wav_tmp, doc->wav_path);
                doc->has_wav = true;
            } else {
                fclose(wfp);
                unlink(w->saved_wav_tmp);
                doc->has_wav = false;
            }
        }
    } else {
        if (wfp) { fclose(wfp); unlink(doc->wav_path); }
        snprintf(err_msg, err_sz, "小智暂未返回完整故事，请检查网络或后台智能体设置。");
    }
    if (story_accum) free(story_accum);
    free(w);
    return ok;
}

/* =========================================================================
 * ENGINE C: Instant Offline Combinatorial Gacha Story Weaver (Zero-Wi-Fi Fallback)
 * ========================================================================= */
static void generate_offline_gacha_story(int hero_i, int scene_i, int theme_i, ai_story_doc_t *doc)
{
    char hero_s[64];
    get_hero_display(hero_i, hero_s, sizeof(hero_s));
    const char *cname = g_net_cfg.child_name[0] ? g_net_cfg.child_name : "宝贝";
    const char *sc = k_scenes[scene_i];
    const char *th = k_themes[theme_i];

    snprintf(doc->title, sizeof(doc->title), "%s畅游%s", hero_s, sc);
    doc->art = k_heroes[hero_i].art;
    doc->page_count = 4;
    doc->has_wav = false;

    snprintf(doc->pages[0], sizeof(doc->pages[0]),
             "星光闪烁的夜晚，%s背上亮晶晶的小行囊，轻轻推开奇妙大门，来到了梦幻般的【%s】。微风吹过，到处都飘着温柔的歌声。",
             hero_s, sc);
    snprintf(doc->pages[1], sizeof(doc->pages[1]),
             "在这里，%s遇到了一位提着星光灯笼的小精灵。小精灵微笑着问：“欢迎来到%s！你知道今晚最棒的成长魔法是什么吗？那就是——%s！”",
             hero_s, sc, th);
    snprintf(doc->pages[2], sizeof(doc->pages[2]),
             "%s认真地点点头，和%s里的小伙伴们手拉手一起练习【%s】。每做好一次，夜空中就会亮起一颗暖洋洋的小金星！",
             hero_s, sc, th);
    snprintf(doc->pages[3], sizeof(doc->pages[3]),
             "月亮升得更高啦，%s获得了闪亮的勇气勋章，还托清风把最甜的晚安祝福送给%s：“愿你今晚也带着【%s】的好习惯，睡个香香甜甜的好觉！”",
             hero_s, cname, th);

    doc->has_quiz = true;
    snprintf(doc->quiz_q, sizeof(doc->quiz_q), "故事小问答：%s在%s学到的成长小魔法是什么？", hero_s, sc);
    snprintf(doc->quiz_opts[0], sizeof(doc->quiz_opts[0]), "A. %s", th);
    snprintf(doc->quiz_opts[1], sizeof(doc->quiz_opts[1]), "B. 赖床不肯刷牙睡觉");
    snprintf(doc->quiz_opts[2], sizeof(doc->quiz_opts[2]), "C. 乱扔玩具发脾气");
    doc->quiz_ans = 0;

    save_story_txt_to_disk(doc);
}

/* =========================================================================
 * Reader & Quiz for Newly Generated AI Story
 * ========================================================================= */
static void run_ai_story_quiz(const ai_story_doc_t *doc)
{
    int sel = 0;
    bool answered = false, correct = false, first = true;
    while (1) {
        gfx_clear(GFX_WHITE);
        gfx_draw_top_bar("★ AI 专属故事 · 互动问答", g_prof.stars, app_eye_mins_left());
        gfx_round_rect(24, 54, GFX_W - 48, 98, 12, 2, GFX_BLACK);
        gfx_draw_paragraph(42, 66, GFX_W - 84, 76, 8, doc->quiz_q, FONT_24, GFX_BLACK, NULL);

        for (int i = 0; i < 3; i++) {
            int y = 170 + i * 70;
            if (i == sel) {
                gfx_fill_round_rect(56, y, GFX_W - 112, 56, 12, GFX_BLACK);
                gfx_draw_text(84, y + 16, doc->quiz_opts[i], FONT_24, GFX_WHITE);
            } else {
                gfx_round_rect(56, y, GFX_W - 112, 56, 12, 2, GFX_BLACK);
                gfx_draw_text(84, y + 16, doc->quiz_opts[i], FONT_24, GFX_BLACK);
            }
        }
        if (answered) {
            gfx_fill_round_rect(120, 386, GFX_W - 240, 46, 10, GFX_GRAY_LIGHT);
            gfx_round_rect(120, 386, GFX_W - 240, 46, 10, 2, GFX_BLACK);
            gfx_draw_text_center(GFX_W / 2, 397,
                correct ? "★ 太棒啦！奖励 3 颗智慧星！按滚轮完成" : "再想一想，拨动滚轮试试另一个答案吧！",
                FONT_24, GFX_BLACK);
        } else {
            gfx_draw_bottom_hints("滚轮选答案", "按下提交", "返回扭蛋屋");
        }
        bsp_epd_smart_present(gfx_get_fb(), first);
        first = false;

        bsp_key_t k = bsp_input_wait(30000);
        if (k == KEY_BACK || (correct && k == KEY_OK)) break;
        if (k == KEY_PREV) { sel = (sel + 2) % 3; answered = false; bsp_audio_sfx(SFX_WHEEL_TICK); }
        else if (k == KEY_NEXT) { sel = (sel + 1) % 3; answered = false; bsp_audio_sfx(SFX_WHEEL_TICK); }
        else if (k == KEY_OK) {
            answered = true;
            if (sel == doc->quiz_ans) {
                correct = true;
                app_add_stars(3);
                g_prof.stories_read++;
                app_profile_save();
                bsp_audio_sfx(SFX_FANFARE);
            } else {
                correct = false;
                bsp_audio_sfx(SFX_WRONG);
            }
        }
    }
}

static void run_ai_story_reader(const ai_story_doc_t *doc)
{
    int page = 0;
    bool sound_on = true;
    bool auto_flip = doc->has_wav;
    if (doc->has_wav) {
        bsp_audio_play_wav(doc->wav_path);
    } else {
        bsp_audio_start_melody(MELODY_LULLABY, false);
    }

    bool first = true;
    bool last_wav_playing = doc->has_wav;
    while (1) {
        bool wav_playing = doc->has_wav && bsp_audio_is_playing() && !bsp_audio_is_paused();

        gfx_clear(GFX_WHITE);
        char hdr[128];
        snprintf(hdr, sizeof(hdr), "★ 《%s》", doc->title);
        gfx_draw_top_bar(hdr, g_prof.stars, app_eye_mins_left());

        gfx_fill_round_rect(20, 56, 236, 368, 16, GFX_GRAY_LIGHT);
        gfx_round_rect(20, 56, 236, 368, 16, 3, GFX_BLACK);
        gfx_fill_round_rect(34, 70, 208, 252, 12, GFX_WHITE);
        gfx_round_rect(34, 70, 208, 252, 12, 2, GFX_BLACK);
        gfx_draw_art(138, 194, 156, doc->art, GFX_BLACK);

        const char *voice_lbl = "♪ 睡前八音盒伴奏";
        if (doc->has_wav) {
            if (bsp_audio_is_paused())        voice_lbl = "♪ 伴读已暂停(摇一摇继续)";
            else if (!bsp_audio_is_playing()) voice_lbl = "♪ 朗读完毕(摇一摇重播)";
            else if (auto_flip)               voice_lbl = "♪ 语音同步自动翻页中";
            else                              voice_lbl = "♪ 伴读中(手动翻页模式)";
        }
        gfx_draw_text_center(138, 334, voice_lbl, FONT_16, GFX_BLACK);

        if (doc->has_wav) {
            int pct = bsp_audio_get_wav_progress_pct();
            gfx_round_rect(42, 358, 192, 12, 5, 2, GFX_BLACK);
            int pw = (186 * pct) / 100;
            if (pw > 0) gfx_fill_round_rect(45, 361, pw, 6, 3, GFX_BLACK);
        }

        char pg_s[64];
        snprintf(pg_s, sizeof(pg_s), "第 %d / %d 页 · 已存书架", page + 1, doc->page_count);
        gfx_draw_text_center(138, 378, pg_s, FONT_16, GFX_BLACK);
        gfx_draw_text_center(138, 401, "Wi-Fi 已关闭 · 零辐射", FONT_16, GFX_BLACK);

        gfx_round_rect(272, 56, 508, 368, 16, 2, GFX_BLACK);
        gfx_draw_paragraph(296, 80, 460, 322, 13, doc->pages[page], FONT_24, GFX_BLACK, NULL);

        gfx_draw_bottom_hints("滚轮翻页/结尾进问答", "生字拼音田字格点读", "返回(摇一摇暂停/重播)");
        bsp_epd_smart_present(gfx_get_fb(), first);
        first = false;

        /* Poll every 220ms during active voiceover so pages flip automatically on exact sentence cues */
        bsp_key_t k = bsp_input_wait((wav_playing && auto_flip) ? 220 : 2500);
        if (k == KEY_NONE) {
            bool now_playing = doc->has_wav && bsp_audio_is_playing() && !bsp_audio_is_paused();
            if (now_playing && auto_flip && page + 1 < doc->page_count) {
                int pm = bsp_audio_get_wav_progress_permille();
                /* Lead by ~6 permille (~250ms) so E-Ink 300ms partial refresh lands right on the first word! */
                if (pm + 6 >= (int)doc->page_start_permille[page + 1]) {
                    page++;
                    last_wav_playing = now_playing;
                    continue;
                }
            }
            if (now_playing != last_wav_playing) {
                last_wav_playing = now_playing;
                continue; /* Refresh once when narration finishes to update badge & 100% bar */
            }
            /* Keep E-Ink 100% static when neither page nor playback state changed */
            while (1) {
                now_playing = doc->has_wav && bsp_audio_is_playing() && !bsp_audio_is_paused();
                if (now_playing != last_wav_playing) { last_wav_playing = now_playing; break; }
                if (now_playing && auto_flip && page + 1 < doc->page_count) {
                    int pm = bsp_audio_get_wav_progress_permille();
                    if (pm + 6 >= (int)doc->page_start_permille[page + 1]) {
                        page++;
                        break;
                    }
                }
                k = bsp_input_wait((now_playing && auto_flip) ? 220 : 2500);
                if (k != KEY_NONE) break;
            }
            if (k == KEY_NONE) continue;
        }

        if (k == KEY_BACK) {
            bsp_audio_stop_all();
            break;
        } else if (k == KEY_PREV && page > 0) {
            auto_flip = false;
            page--;
            bsp_audio_sfx(SFX_PAGE_TURN);
        } else if (k == KEY_NEXT) {
            if (page < doc->page_count - 1) {
                auto_flip = false;
                page++;
                bsp_audio_sfx(SFX_PAGE_TURN);
            } else if (doc->has_quiz) {
                bsp_audio_stop_all();
                run_ai_story_quiz(doc);
                break;
            }
        } else if (k == KEY_SHAKE) {
            if (doc->has_wav) {
                if (!bsp_audio_is_playing()) {
                    page = 0;
                    auto_flip = true;
                    last_wav_playing = true;
                    bsp_audio_play_wav(doc->wav_path);
                } else {
                    bsp_audio_toggle_pause();
                    if (!bsp_audio_is_paused()) auto_flip = true;
                    last_wav_playing = !bsp_audio_is_paused();
                }
            } else {
                sound_on = !sound_on;
                if (sound_on) bsp_audio_start_melody(MELODY_LULLABY, false);
                else bsp_audio_stop_all();
            }
        } else if (k == KEY_OK) {
            auto_flip = false;
            char btitle[112];
            snprintf(btitle, sizeof(btitle), "《%s》", doc->title);
            app_word_study_page(doc->pages[page], btitle);
            if (!doc->has_wav && sound_on) bsp_audio_start_melody(MELODY_LULLABY, false);
            first = true;
        }
        app_check_eye_guard();
    }
}

/* =========================================================================
 * Prompt Dialog When Wi-Fi Not Configured Yet or Network Error
 * ========================================================================= */
static int ask_wifi_or_offline_dialog(const char *headline, const char *desc)
{
    int sel = 0; /* 0 = Scan QR to configure Wi-Fi/AI, 1 = Play instant offline gacha story */
    bool first = true;
    while (1) {
        gfx_clear(GFX_WHITE);
        gfx_draw_top_bar("★ 家庭 Wi-Fi 与 AI 故事引导", g_prof.stars, app_eye_mins_left());

        gfx_round_rect(48, 64, GFX_W - 96, 352, 18, 3, GFX_BLACK);
        gfx_draw_art(165, 225, 150, ART_OWL_GUARD, GFX_BLACK);
        gfx_draw_text(275, 96, headline, FONT_24, GFX_BLACK);
        gfx_draw_paragraph(275, 142, 435, 108, 8, desc, FONT_16, GFX_BLACK, NULL);

        const char *btns[2] = {
            "① 手机扫码配置家庭 Wi-Fi 与小智/大模型",
            "② 先用本机离线魔法引擎立即生成故事"
        };
        for (int i = 0; i < 2; i++) {
            int y = 264 + i * 64;
            if (i == sel) {
                gfx_fill_round_rect(275, y, 445, 52, 12, GFX_BLACK);
                gfx_draw_text(298, y + 14, btns[i], FONT_24, GFX_WHITE);
            } else {
                gfx_round_rect(275, y, 445, 52, 12, 2, GFX_BLACK);
                gfx_draw_text(298, y + 14, btns[i], FONT_24, GFX_BLACK);
            }
        }
        gfx_draw_bottom_hints("滚轮选择方式", "按下确认", "返回扭蛋选词");
        bsp_epd_smart_present(gfx_get_fb(), first);
        first = false;

        bsp_key_t k = bsp_input_wait(30000);
        if (k == KEY_BACK) return -1;
        if (k == KEY_PREV || k == KEY_NEXT) { sel ^= 1; bsp_audio_sfx(SFX_WHEEL_TICK); }
        else if (k == KEY_OK) return sel;
    }
}

static void trigger_story_generation(int hero_i, int scene_i, int theme_i)
{
    ai_story_doc_t *doc = (ai_story_doc_t *)calloc(1, sizeof(ai_story_doc_t));
    if (!doc) return;

    if (!app_wifi_has_credentials()) {
        int act = ask_wifi_or_offline_dialog(
            "尚未连接家庭路由器 Wi-Fi",
            "让爸爸妈妈用手机扫一扫，30秒即可配好家庭 Wi-Fi 并绑定小智 AI（免填Key带语音）或 DeepSeek/千问大模型！");
        if (act == 0) {
            app_wifi_provision_run(true);
            free(doc);
            return;
        } else if (act == 1) {
            generate_offline_gacha_story(hero_i, scene_i, theme_i, doc);
            bsp_audio_sfx(SFX_FANFARE);
            run_ai_story_reader(doc);
            free(doc);
            return;
        } else {
            free(doc);
            return;
        }
    }

    char conn_detail[128];
    snprintf(conn_detail, sizeof(conn_detail), "正在连接路由器：%s\nAI 引擎：%s",
             g_net_cfg.wifi_ssid, app_ai_provider_label(g_net_cfg.ai_provider));
    draw_ai_progress("① 正在唤醒家庭 Wi-Fi 联网...", conn_detail, 18, true);

    char ip_str[24] = {0};
    if (!app_wifi_connect_sta(12000, ip_str, sizeof(ip_str))) {
        app_wifi_shutdown();
        bsp_audio_sfx(SFX_WRONG);
        int act = ask_wifi_or_offline_dialog(
            "连接家庭 Wi-Fi 超时",
            "请确认家庭路由器开启了 2.4GHz 频段且密码正确。可重新扫码修改配网，或先生成离线扭蛋故事：");
        if (act == 0) app_wifi_provision_run(true);
        else if (act == 1) {
            generate_offline_gacha_story(hero_i, scene_i, theme_i, doc);
            bsp_audio_sfx(SFX_FANFARE);
            run_ai_story_reader(doc);
        }
        free(doc);
        return;
    }

    char err_msg[160] = {0};
    bool ok = false;
    if (g_net_cfg.ai_provider == AI_PROV_XIAOZHI) {
        ok = generate_via_xiaozhi(hero_i, scene_i, theme_i, doc, err_msg, sizeof(err_msg));
    } else {
        ok = generate_via_cloud_llm(hero_i, scene_i, theme_i, doc, err_msg, sizeof(err_msg));
    }

    /* Immediately turn off Wi-Fi radio to keep reading 100% radiation-free & low-power! */
    app_wifi_shutdown();

    if (ok) {
        bsp_audio_sfx(SFX_FANFARE);
        run_ai_story_reader(doc);
    } else {
        bsp_audio_sfx(SFX_WRONG);
        int act = ask_wifi_or_offline_dialog("云端生成遇到小插曲", err_msg[0] ? err_msg : "请检查网络或 API 配置。");
        if (act == 0) app_wifi_provision_run(true);
        else if (act == 1) {
            generate_offline_gacha_story(hero_i, scene_i, theme_i, doc);
            bsp_audio_sfx(SFX_FANFARE);
            run_ai_story_reader(doc);
        }
    }
    free(doc);
}

/* =========================================================================
 * MAIN UI: 滚轮三步扭蛋生成专属睡前故事 (3-Reel Thumbwheel Slot UI)
 * ========================================================================= */
void app_ai_story_run(void)
{
    static const int k_cnts[3] = { GACHA_HERO_CNT, GACHA_SCENE_CNT, GACHA_THEME_CNT };
    int picks[3] = {
        (int)(esp_random() % GACHA_HERO_CNT),
        (int)(esp_random() % GACHA_SCENE_CNT),
        0 /* Default theme: 温柔晚安快快睡 */
    };
    int step = 0;    /* 0=Hero, 1=Scene, 2=Theme, 3=Bottom Action Bar */
    int act_sel = 0; /* 0=Generate Story, 1=Shake Random, 2=Wi-Fi & AI Settings */
    bool first = true;

    while (1) {
        gfx_clear(GFX_WHITE);
        char top_t[120];
        snprintf(top_t, sizeof(top_t), "★ AI 扭蛋故事机 [%s · %s]",
                 app_wifi_has_credentials() ? g_net_cfg.wifi_ssid : "待配Wi-Fi",
                 app_ai_provider_label(g_net_cfg.ai_provider));
        gfx_draw_top_bar(top_t, g_prof.stars, app_eye_mins_left());

        const char *col_base[3] = { "① 主角", "② 场景", "③ 主题" };
        for (int c = 0; c < 3; c++) {
            int x = 22 + c * 258;
            int y = 52;
            bool active = (step == c);
            bool locked = (step > c);

            char chdr[48];
            snprintf(chdr, sizeof(chdr), "%s (%d/%d)", col_base[c], picks[c] + 1, k_cnts[c]);

            gfx_round_rect(x, y, 240, 294, 18, active ? 5 : 2, GFX_BLACK);
            if (active) {
                gfx_fill_round_rect(x + 10, y + 10, 220, 40, 10, GFX_BLACK);
                gfx_draw_text_center(x + 120, y + 18, chdr, FONT_24, GFX_WHITE);
            } else {
                gfx_fill_round_rect(x + 10, y + 10, 220, 40, 10, GFX_GRAY_LIGHT);
                gfx_draw_text_center(x + 120, y + 18, chdr, FONT_24, GFX_BLACK);
            }

            char prev_s[64], curr_s[64], next_s[64];
            int p_idx = (picks[c] + k_cnts[c] - 1) % k_cnts[c];
            int n_idx = (picks[c] + 1) % k_cnts[c];
            if (c == 0) {
                get_hero_display(p_idx, prev_s, sizeof(prev_s));
                get_hero_display(picks[c], curr_s, sizeof(curr_s));
                get_hero_display(n_idx, next_s, sizeof(next_s));
                gfx_draw_art(x + 120, y + 122, 95, k_heroes[picks[0]].art, GFX_BLACK);
            } else if (c == 1) {
                strcpy(prev_s, k_scenes[p_idx]);
                strcpy(curr_s, k_scenes[picks[1]]);
                strcpy(next_s, k_scenes[n_idx]);
                gfx_draw_art(x + 120, y + 122, 95, ART_MOON_STARS, GFX_BLACK);
            } else {
                strcpy(prev_s, k_themes[p_idx]);
                strcpy(curr_s, k_themes[picks[2]]);
                strcpy(next_s, k_themes[n_idx]);
                gfx_draw_art(x + 120, y + 122, 95, ART_GACHA_MAGIC, GFX_BLACK);
            }

            /* Slot machine neighboring hints + center selected capsule */
            gfx_draw_text_center(x + 120, y + 180, prev_s, FONT_16, GFX_BLACK);
            if (active) {
                gfx_fill_round_rect(x + 12, y + 204, 216, 48, 12, GFX_BLACK);
                gfx_draw_text_center(x + 120, y + 216, curr_s, FONT_24, GFX_WHITE);
            } else {
                gfx_round_rect(x + 12, y + 204, 216, 48, 12, 3, GFX_BLACK);
                gfx_draw_text_center(x + 120, y + 216, curr_s, FONT_24, GFX_BLACK);
            }
            gfx_draw_text_center(x + 120, y + 260, locked ? "✓ 已锁定 (按返回可重选)" : next_s, FONT_16, GFX_BLACK);
        }

        /* Step 3 Bottom Action Bar */
        const char *acts[3] = {
            "★ 魔法生成专属故事",
            "↺ 随机摇号换一组",
            "⚙ 配网与AI设置"
        };
        int bx[3] = { 22, 318, 566 };
        int bw[3] = { 282, 234, 212 };
        for (int b = 0; b < 3; b++) {
            bool b_focus = (step == 3 && act_sel == b);
            if (b_focus) {
                gfx_fill_round_rect(bx[b], 362, bw[b], 58, 14, GFX_BLACK);
                gfx_draw_text_center(bx[b] + bw[b] / 2, 379, acts[b], FONT_24, GFX_WHITE);
            } else {
                gfx_round_rect(bx[b], 362, bw[b], 58, 14, 2, GFX_BLACK);
                gfx_draw_text_center(bx[b] + bw[b] / 2, 379, acts[b], FONT_24, GFX_BLACK);
            }
        }

        if (step < 3) {
            gfx_draw_bottom_hints("拨滚轮/长拨快选词条", "按下锁定进入下一步", step == 0 ? "摇一摇随机·返回主页" : "退回上一列");
        } else {
            gfx_draw_bottom_hints("滚轮切换下方按钮", "按下立即执行", "退回修改第③列");
        }

        bsp_epd_smart_present(gfx_get_fb(), first);
        first = false;

        bsp_key_t k = bsp_input_wait(30000);
        if (k == KEY_SHAKE) {
            picks[0] = (int)(esp_random() % GACHA_HERO_CNT);
            picks[1] = (int)(esp_random() % GACHA_SCENE_CNT);
            picks[2] = (int)(esp_random() % GACHA_THEME_CNT);
            step = 3;
            act_sel = 0;
            bsp_audio_sfx(SFX_FANFARE);
        } else if (k == KEY_BACK) {
            if (step > 0) {
                step--;
                bsp_audio_sfx(SFX_BACK);
            } else {
                break;
            }
        } else if (k == KEY_PREV) {
            bsp_audio_sfx(SFX_WHEEL_TICK);
            if (step < 3) picks[step] = (picks[step] + k_cnts[step] - 1) % k_cnts[step];
            else act_sel = (act_sel + 2) % 3;
        } else if (k == KEY_NEXT) {
            bsp_audio_sfx(SFX_WHEEL_TICK);
            if (step < 3) picks[step] = (picks[step] + 1) % k_cnts[step];
            else act_sel = (act_sel + 1) % 3;
        } else if (k == KEY_OK) {
            if (step < 3) {
                step++;
                act_sel = 0;
                bsp_audio_sfx(SFX_BUBBLE);
            } else {
                if (act_sel == 0) {
                    bsp_audio_sfx(SFX_CONFIRM);
                    trigger_story_generation(picks[0], picks[1], picks[2]);
                    first = true;
                } else if (act_sel == 1) {
                    picks[0] = (int)(esp_random() % GACHA_HERO_CNT);
                    picks[1] = (int)(esp_random() % GACHA_SCENE_CNT);
                    picks[2] = (int)(esp_random() % GACHA_THEME_CNT);
                    bsp_audio_sfx(SFX_FANFARE);
                } else {
                    bsp_audio_sfx(SFX_CONFIRM);
                    app_wifi_provision_run(true);
                    first = true;
                }
            }
        }
        app_check_eye_guard();
    }
}
