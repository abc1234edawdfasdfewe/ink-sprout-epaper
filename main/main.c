#include "app_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_log.h"

static const char *TAG = "INK_SPROUT";

app_profile_t g_prof;
static int64_t s_eye_session_start_us = 0;

void app_profile_load(void)
{
    memset(&g_prof, 0, sizeof(g_prof));
    g_prof.stars = 3;
    g_prof.volume = 70;
    g_prof.max_volume = 80;
    g_prof.eye_limit_min = 20;
    g_prof.flip_180 = 0;

    nvs_handle_t h;
    if (nvs_open("inksprout", NVS_READONLY, &h) == ESP_OK) {
        size_t sz = 0;
        if (nvs_get_blob(h, "prof_v1", NULL, &sz) == ESP_OK && sz > 0 && sz <= sizeof(g_prof)) {
            nvs_get_blob(h, "prof_v1", &g_prof, &sz);
        }
        nvs_close(h);
    }
    if (g_prof.max_volume < 40 || g_prof.max_volume > 100) g_prof.max_volume = 80;
    if (g_prof.volume < 0 || g_prof.volume > g_prof.max_volume) g_prof.volume = 70;
    bsp_audio_set_max_volume(g_prof.max_volume);
    bsp_audio_set_volume(g_prof.volume);
    gfx_set_flipped(g_prof.flip_180 != 0);
    bsp_input_set_invert_wheel(g_prof.flip_180 != 0);
}

void app_profile_save(void)
{
    nvs_handle_t h;
    if (nvs_open("inksprout", NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_blob(h, "prof_v1", &g_prof, sizeof(g_prof));
        nvs_commit(h);
        nvs_close(h);
    }
}

void app_add_stars(int delta)
{
    g_prof.stars += delta;
    if (g_prof.stars < 0) g_prof.stars = 0;
}

int app_get_high_score(app_score_id_t id)
{
    if (id >= 0 && id < SCORE_GAME_COUNT) return (int)g_prof.high_scores[id];
    int ext = (int)id - SCORE_GAME_COUNT;
    if (ext >= 0 && ext < 12) return (int)g_prof.extra_scores[ext];
    return 0;
}

void app_update_high_score(app_score_id_t id, int score)
{
    if (id >= 0 && id < SCORE_GAME_COUNT) {
        if (score > g_prof.high_scores[id]) g_prof.high_scores[id] = score;
    } else {
        int ext = (int)id - SCORE_GAME_COUNT;
        if (ext >= 0 && ext < 12 && score > g_prof.extra_scores[ext]) {
            g_prof.extra_scores[ext] = score;
        }
    }
}

void app_eye_reset_session(void)
{
    s_eye_session_start_us = esp_timer_get_time();
}

int app_eye_mins_left(void)
{
    if (g_prof.eye_limit_min <= 0) return 0;
    int elapsed_min = (int)((esp_timer_get_time() - s_eye_session_start_us) / 60000000LL);
    int left = g_prof.eye_limit_min - elapsed_min;
    return (left > 0) ? left : 1;
}

void app_check_eye_guard(void)
{
    if (g_prof.eye_limit_min <= 0) return;
    int elapsed_min = (int)((esp_timer_get_time() - s_eye_session_start_us) / 60000000LL);
    if (elapsed_min < g_prof.eye_limit_min) return;

    bsp_audio_stop_all();
    bsp_audio_start_melody(MELODY_LULLABY, false);

    gfx_clear(GFX_WHITE);
    gfx_round_rect(40, 40, GFX_W - 80, GFX_H - 80, 24, 4, GFX_BLACK);
    gfx_draw_art(185, 240, 176, ART_OWL_GUARD, GFX_BLACK);
    gfx_draw_art(358, 122, 64, ART_ALARM_CLOCK, GFX_BLACK);
    gfx_draw_text(404, 108, "护眼小闹钟提醒：", FONT_32, GFX_BLACK);
    char eye_msg[192];
    snprintf(eye_msg, sizeof(eye_msg),
             "已经连续专注玩了 %d 分钟啦！请走到窗边看看远处的绿树，眨眨眼睛休息 20 秒吧！",
             (int)g_prof.eye_limit_min);
    gfx_draw_paragraph(330, 185, 396, 140, 12, eye_msg, FONT_24, GFX_BLACK, NULL);
    gfx_draw_text(330, 350, "休息好后按下滚轮继续 (奖励 ★+1)", FONT_16, GFX_BLACK);
    bsp_epd_present(gfx_get_fb(), EPD_REFRESH_FAST);

    /* Minimum 6-second calm pause before accepting dismiss */
    vTaskDelay(pdMS_TO_TICKS(6000));
    bsp_input_flush();
    while (bsp_input_wait(30000) != KEY_OK) {}
    app_add_stars(1);
    app_profile_save();
    app_eye_reset_session();
}

static void draw_home_screen(int sel)
{
    static const struct {
        const char  *title;
        const char  *sub;
        gfx_art_id_t art;
    } k_cards[6] = {
        { "绘本故事屋", "经典8册+有声书架",   ART_RABBIT       },
        { "AI 扭蛋故事", "滚轮定制·小智/大模型", ART_GACHA_MAGIC },
        { "启蒙游乐园", "3-6岁 · 6款滚轮游戏", ART_SUBMARINE    },
        { "益智挑战营", "6-12岁 · 8款脑力挑战", ART_FERRIS_WHEEL },
        { "变声与荣誉", "鹦鹉变声·星星荣誉墙", ART_PARROT_MIC   },
        { "配网与家长", "Wi-Fi·传书·护眼设置", ART_OWL_GUARD    },
    };

    gfx_clear(GFX_WHITE);
    gfx_draw_top_bar("★ 墨芽童伴", g_prof.stars, app_eye_mins_left());

    for (int i = 0; i < 6; i++) {
        int col = i % 3;
        int row = i / 3;
        int x = 22 + col * 258;
        int y = 54 + row * 190;
        bool focus = (i == sel);

        if (focus) {
            gfx_round_rect(x - 2, y - 2, 244, 180, 18, 5, GFX_BLACK);
            gfx_fill_round_rect(x + 8, y + 114, 224, 34, 10, GFX_BLACK);
            gfx_draw_art(x + 120, y + 57, 88, k_cards[i].art, GFX_BLACK);
            gfx_draw_text_center(x + 120, y + 119, k_cards[i].title, FONT_24, GFX_WHITE);
            gfx_draw_text_center(x + 120, y + 153, k_cards[i].sub, FONT_16, GFX_BLACK);
        } else {
            gfx_round_rect(x, y, 240, 176, 16, 2, GFX_BLACK);
            gfx_draw_art(x + 120, y + 57, 85, k_cards[i].art, GFX_BLACK);
            gfx_draw_text_center(x + 120, y + 119, k_cards[i].title, FONT_24, GFX_BLACK);
            gfx_draw_text_center(x + 120, y + 153, k_cards[i].sub, FONT_16, GFX_BLACK);
        }
    }

    gfx_draw_bottom_hints("拨动滚轮选乐园", "按下进入", "摇一摇随机听故事");
}

void app_main(void)
{
    ESP_LOGI(TAG, "Booting InkSprout Kids Storyteller & Thumbwheel Console...");
    srand((unsigned)esp_random());

    bsp_board_init();
    gfx_init();
    app_profile_load();
    app_net_cfg_load();
    app_eye_reset_session();

    /* Welcoming startup chime */
    bsp_audio_sfx(SFX_CONFIRM);

    int home_sel = 0;
    int idle_ticks = 0;
    bool need_clean = true;

    while (1) {
        draw_home_screen(home_sel);
        bsp_epd_smart_present(gfx_get_fb(), need_clean);
        need_clean = false;

        bsp_key_t k = bsp_input_wait(15000);
        if (k == KEY_NONE) {
            idle_ticks++;
            /* Enter ultra-low-power bedside clock after ~3 minutes of inactivity */
            if (idle_ticks >= 12) {
                app_enter_sleep_clock();
                idle_ticks = 0;
                need_clean = true;
            }
            continue;
        }
        idle_ticks = 0;

        if (k == KEY_PREV) {
            home_sel = (home_sel + 5) % 6;
            bsp_audio_sfx(SFX_WHEEL_TICK);
        } else if (k == KEY_NEXT) {
            home_sel = (home_sel + 1) % 6;
            bsp_audio_sfx(SFX_WHEEL_TICK);
        } else if (k == KEY_SHAKE) {
            bsp_audio_sfx(SFX_FANFARE);
            app_stories_run((int)(esp_random() & 0x7FFF));
            need_clean = true;
        } else if (k == KEY_PWR_SHORT) {
            /* Side PWR button short press enters cozy sleep clock immediately */
            app_enter_sleep_clock();
            need_clean = true;
        } else if (k == KEY_OK) {
            bsp_audio_sfx(SFX_CONFIRM);
            switch (home_sel) {
            case 0: app_stories_run(-1);        break;
            case 1: app_ai_story_run();         break;
            case 2: app_preschool_park_run();   break;
            case 3: app_school_arena_run();     break;
            case 4: app_fun_trophy_hub_run();   break;
            case 5: app_parent_hub_run();       break;
            }
            need_clean = true;
        }
        app_check_eye_guard();
    }
}
