#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "bsp_board.h"
#include "bsp_epd.h"
#include "bsp_gfx.h"
#include "bsp_audio.h"
#include "bsp_input.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    /* Legacy 6 IDs kept at 0..5 so existing NVS prof_v1 blobs stay 100% compatible */
    SCORE_SAFARI_MATCH  = 0,
    SCORE_SUBMARINE     = 1,
    SCORE_MATH_TOWER    = 2,
    SCORE_MEMORY_FLIP   = 3,
    SCORE_BREAKOUT      = 4,
    SCORE_CODEBREAKER   = 5,
    SCORE_GAME_COUNT    = 6,

    /* 4 New Preschool Thumbwheel Games (3-6岁) */
    SCORE_XYLOPHONE     = 6,
    SCORE_HEDGEHOG      = 7,
    SCORE_PATTERN_TRAIN = 8,
    SCORE_PANDA_STACK   = 9,

    /* 4 New School-Age Thumbwheel Games (6-12岁) */
    SCORE_CONNECT4      = 10,
    SCORE_IDIOM_WHEEL   = 11,
    SCORE_ORBIT_DEFEND  = 12,
    SCORE_SLIDE_PUZZLE  = 13,

    SCORE_GAME_TOTAL    = 14
} app_score_id_t;

typedef struct {
    int32_t  stars;
    int32_t  stories_read;
    int32_t  high_scores[SCORE_GAME_COUNT];
    int32_t  volume;          /* 0..100 */
    int32_t  max_volume;      /* 40..100 parental cap */
    int32_t  eye_limit_min;   /* 0=off, 15, 20, 30 */
    uint8_t  flip_180;        /* 0 or 1 */
    uint8_t  story_bookmarks[16];
    int32_t  extra_scores[12]; /* Appended for games 6..17 without shifting legacy NVS offsets */
} app_profile_t;

typedef enum {
    AI_PROV_XIAOZHI   = 0,  /* 小智 AI (免Key·6位验证码绑定·自带Opus真人语音) */
    AI_PROV_DEEPSEEK  = 1,  /* DeepSeek 官方 (deepseek-chat) */
    AI_PROV_QWEN      = 2,  /* 阿里云百炼·通义千问 (qwen-plus) */
    AI_PROV_DOUBAO    = 3,  /* 火山引擎·豆包大模型 */
    AI_PROV_SILICON   = 4,  /* 硅基流动 SiliconFlow (Qwen + CosyVoice TTS) */
    AI_PROV_CUSTOM    = 5,  /* 自定义 OpenAI 兼容接口 / 局域网 Ollama */
} app_ai_provider_t;

typedef struct {
    uint32_t magic;
    char     wifi_ssid[33];
    char     wifi_pass[65];
    char     child_name[28];      /* e.g. "朵朵", "宝贝" */
    uint8_t  age_mode;            /* 0: 3-6岁学龄前, 1: 6-12岁学龄段 */
    uint8_t  ai_provider;         /* app_ai_provider_t */
    char     ai_url[144];         /* Chat Completions endpoint or Xiaozhi OTA URL */
    char     ai_key[144];         /* Bearer API Key */
    char     ai_model[64];        /* Model ID */
    uint8_t  tts_enabled;         /* 0 or 1 (for REST LLM modes) */
    char     tts_url[144];        /* /v1/audio/speech endpoint */
    char     tts_key[144];        /* Optional separate TTS key (blank = reuse ai_key) */
    char     tts_model[64];       /* e.g. FunAudioLLM/CosyVoice2-0.5B or tts-1 */
    char     tts_voice[64];       /* e.g. FunAudioLLM/CosyVoice2-0.5B:anna or nova */
    char     xiaozhi_uuid[40];    /* Persistent UUID v4 for Xiaozhi Client-Id */
    uint32_t ai_story_seq;        /* Auto-increment counter for saved AI stories */
} app_net_cfg_t;

extern app_profile_t g_prof;
extern app_net_cfg_t g_net_cfg;

void app_profile_load(void);
void app_profile_save(void);
void app_net_cfg_load(void);
void app_net_cfg_save(void);
void app_get_device_mac_str(char *out18);
const char *app_ai_provider_label(uint8_t prov);

/* Shared Home Router Wi-Fi STA Lifecycle (0mW when idle) */
bool app_wifi_has_credentials(void);
bool app_wifi_connect_sta(int timeout_ms, char *ip_out, int ip_out_len);
void app_wifi_shutdown(void);

void app_add_stars(int delta);
int  app_get_high_score(app_score_id_t id);
void app_update_high_score(app_score_id_t id, int score);

int  app_eye_mins_left(void);
void app_eye_reset_session(void);
void app_check_eye_guard(void);

/* Feature Modules */
void app_stories_run(int quick_random_idx);
void app_ai_story_run(void);
void app_word_study_page(const char *page_utf8, const char *book_title);
void app_hanzi_notebook_run(void);
void app_preschool_park_run(void);
void app_school_arena_run(void);
void app_voice_parrot_run(void);
void app_trophy_wall_run(void);
void app_fun_trophy_hub_run(void);
void app_parent_hub_run(void);
void app_transfer_hub_run(void);
void app_wifi_provision_run(bool focus_ai_tab);
void app_enter_sleep_clock(void);

#ifdef __cplusplus
}
#endif
