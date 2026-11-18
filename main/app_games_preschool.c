#include "app_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

/* =========================================================================
 * GAME 1 (3-6岁): 滚轮配配乐 · 森林摩天轮 (Carousel Match Safari)
 * Pure 1D thumbwheel vertical Ferris-wheel selector, zero punishment!
 * ========================================================================= */
typedef struct {
    const char  *prompt;
    gfx_art_id_t art;
    const char  *wheel_opts[4];
    int          ans;
} safari_q_t;

static const safari_q_t k_safari[] = {
    { "红眼睛，长耳朵，小兔子最爱啃什么？", ART_RABBIT,       { "香甜胡萝卜", "硬硬大骨头", "咸咸小鱼干", "尖尖仙人掌" }, 0 },
    { "背着重重小房子，一步一步爬上山是谁？", ART_TURTLE,     { "长颈鹿", "坚持的小乌龟", "大老虎", "小燕子" }, 1 },
    { "池塘里圆脑袋小蝌蚪，长大变成谁呀？", ART_TADPOLE_FROG, { "大螃蟹", "小蜜蜂", "捉害虫的青蛙", "大白鹅" }, 2 },
    { "数一数：★★★ 再加上 ★★ 一共几颗星？", ART_TROPHY_STARS, { "4 颗星", "5 颗星", "3 颗星", "6 颗星" }, 1 },
    { "古诗配配看：春眠不觉晓，处处闻——？", ART_PARROT_MIC,   { "闻啼鸟", "闻烤肉", "闻汽车", "闻雪花" }, 0 },
    { "弯弯的月儿小小的船，夜晚挂在哪里呀？", ART_MOON_STARS, { "挂在海底", "挂在书包里", "挂在蓝蓝夜空", "挂在冰箱里" }, 2 },
    { "鼻子长长像水管，曹冲用大船称的是谁？", ART_ELEPHANT,   { "小猫咪", "大灰狼", "小松鼠", "威风的大象" }, 3 },
    { "数一数：手指头一只手有 5 根，两只手呢？", ART_FERRIS_WHEEL, { "8 根手指", "10 根手指", "6 根手指", "12 根手指" }, 1 },
};
#define SAFARI_COUNT ((int)(sizeof(k_safari) / sizeof(k_safari[0])))

static void run_game_safari_match(void)
{
    int q_idx = 0;
    int wheel = 0;
    int score = 0;
    const char *feedback = "拨动滚轮转动摩天轮，按下选出正确答案吧！";
    bool first = true;

    while (1) {
        const safari_q_t *q = &k_safari[q_idx];
        gfx_clear(GFX_WHITE);
        char title[64];
        snprintf(title, sizeof(title), "滚轮配配乐  得分:%d", score);
        gfx_draw_top_bar(title, g_prof.stars, app_eye_mins_left());

        /* Left Question & Illustration Card */
        gfx_round_rect(20, 54, 370, 320, 16, 3, GFX_BLACK);
        gfx_draw_art(205, 168, 150, q->art, GFX_BLACK);
        gfx_draw_paragraph(40, 268, 330, 96, 8, q->prompt, FONT_24, GFX_BLACK, NULL);

        /* Right Thumbwheel Carousel Cabins */
        gfx_fill_round_rect(410, 54, 370, 320, 16, GFX_GRAY_LIGHT);
        gfx_round_rect(410, 54, 370, 320, 16, 3, GFX_BLACK);
        for (int i = 0; i < 4; i++) {
            int y = 70 + i * 74;
            char item[80];
            snprintf(item, sizeof(item), "%s %s", (i == wheel) ? "▶" : "○", q->wheel_opts[i]);
            if (i == wheel) {
                gfx_fill_round_rect(430, y, 330, 60, 14, GFX_BLACK);
                gfx_draw_text(452, y + 18, item, FONT_24, GFX_WHITE);
            } else {
                gfx_fill_round_rect(430, y, 330, 60, 14, GFX_WHITE);
                gfx_round_rect(430, y, 330, 60, 14, 2, GFX_BLACK);
                gfx_draw_text(452, y + 18, item, FONT_24, GFX_BLACK);
            }
        }

        gfx_round_rect(20, 384, GFX_W - 40, 48, 10, 2, GFX_BLACK);
        gfx_draw_text_center(GFX_W / 2, 396, feedback, FONT_24, GFX_BLACK);
        gfx_draw_bottom_hints("滚轮转动转盘", "按下确认配对", "返回游乐园");

        bsp_epd_smart_present(gfx_get_fb(), first);
        first = false;

        bsp_key_t k = bsp_input_wait(30000);
        if (k == KEY_BACK) break;
        else if (k == KEY_PREV) { wheel = (wheel + 3) % 4; bsp_audio_sfx(SFX_WHEEL_TICK); }
        else if (k == KEY_NEXT) { wheel = (wheel + 1) % 4; bsp_audio_sfx(SFX_WHEEL_TICK); }
        else if (k == KEY_OK) {
            if (wheel == q->ans) {
                score += 10;
                app_update_high_score(SCORE_SAFARI_MATCH, score);
                if (score % 30 == 0) app_add_stars(1);
                bsp_audio_sfx(SFX_SUCCESS);
                feedback = "★ 太棒啦！配对成功！继续下一题 ★";
                q_idx = (q_idx + 1) % SAFARI_COUNT;
                wheel = 0;
            } else {
                bsp_audio_sfx(SFX_WRONG);
                feedback = "哎呀差一点点，再拨动滚轮试试别的吧！";
            }
        }
    }
    app_profile_save();
}

/* =========================================================================
 * GAME 2 (4-8岁): 海底小潜艇 · 接星星 (3-Lane Step Rhythm Game)
 * Designed specifically for 3-way thumbwheel + 0.3s partial refresh!
 * ========================================================================= */
#define SUB_COLS  8
#define SUB_LANES 3

static void run_game_submarine(void)
{
    /* 0=empty, 1=star(★ +10), 2=jellyfish(● obstacle, sonar pops into star!) */
    uint8_t grid[SUB_LANES][SUB_COLS];
    memset(grid, 0, sizeof(grid));

    int sub_lane = 1;
    int score = 0;
    int hearts = 3;
    int sonar_col = -1, sonar_lane = 0;
    bool first = true;

    while (hearts > 0) {
        /* Render clean 3-lane ocean arena */
        gfx_clear(GFX_WHITE);
        char hdr[96];
        snprintf(hdr, sizeof(hdr), "海底小潜艇   得分:%d   生命:%s",
                 score, hearts == 3 ? "♥♥♥" : (hearts == 2 ? "♥♥" : "♥"));
        gfx_draw_top_bar(hdr, g_prof.stars, app_eye_mins_left());

        for (int ln = 0; ln < SUB_LANES; ln++) {
            int ly = 60 + ln * 122;
            gfx_round_rect(24, ly, GFX_W - 48, 110, 16, 2, GFX_BLACK);
            /* Dashed lane wave guide */
            for (int wx = 44; wx < GFX_W - 50; wx += 36) {
                gfx_hline(wx, ly + 96, 18, GFX_GRAY_MID);
            }

            if (ln == sub_lane) {
                gfx_draw_art(96, ly + 54, 92, ART_SUBMARINE, GFX_BLACK);
            }

            for (int c = 1; c < SUB_COLS; c++) {
                int cx = 96 + c * 88;
                int cy = ly + 52;
                if (sonar_col == c && sonar_lane == ln) {
                    gfx_circle(cx, cy, 18, 3, GFX_BLACK);
                    gfx_circle(cx, cy, 10, 2, GFX_BLACK);
                } else if (grid[ln][c] == 1) {
                    gfx_draw_text_center(cx, cy - 16, "★", FONT_32, GFX_BLACK);
                } else if (grid[ln][c] == 2) {
                    /* Cute storybook jellyfish with eyes, scalloped bell & wavy tentacles */
                    gfx_fill_circle(cx, cy - 6, 18, GFX_BLACK);
                    gfx_fill_circle(cx, cy - 6, 15, GFX_WHITE);
                    gfx_fill_rect(cx - 20, cy + 2, 40, 20, GFX_WHITE);
                    gfx_hline(cx - 17, cy + 2, 35, GFX_BLACK);
                    gfx_hline(cx - 17, cy + 3, 35, GFX_BLACK);
                    /* Cute eyes & smile */
                    gfx_fill_circle(cx - 6, cy - 8, 2, GFX_BLACK);
                    gfx_fill_circle(cx + 6, cy - 8, 2, GFX_BLACK);
                    gfx_hline(cx - 3, cy - 3, 7, GFX_BLACK);
                    /* Scalloped dots & wavy tentacles */
                    for (int t = -12; t <= 12; t += 8) {
                        gfx_fill_circle(cx + t, cy + 4, 2, GFX_BLACK);
                        gfx_vline(cx + t,     cy + 6, 6, GFX_BLACK);
                        gfx_vline(cx + t + 2, cy + 11, 6, GFX_BLACK);
                        gfx_vline(cx + t,     cy + 16, 5, GFX_BLACK);
                    }
                }
            }
        }

        gfx_draw_bottom_hints("滚轮上浮/下潜切换水道", "按下发射声呐泡泡变星星", "退出游戏");
        bsp_epd_smart_present(gfx_get_fb(), first);
        first = false;

        /* Wait 420ms rhythm beat, responding immediately to thumbwheel */
        sonar_col = -1;
        bsp_key_t k = bsp_input_wait(420);
        if (k == KEY_BACK) break;
        if (k == KEY_PREV && sub_lane > 0) {
            sub_lane--;
            bsp_audio_sfx(SFX_WHEEL_TICK);
        } else if (k == KEY_NEXT && sub_lane < SUB_LANES - 1) {
            sub_lane++;
            bsp_audio_sfx(SFX_WHEEL_TICK);
        } else if (k == KEY_OK) {
            /* Sonar wave converts nearest jellyfish in current lane into a bonus star! */
            bsp_audio_sfx(SFX_BUBBLE);
            sonar_lane = sub_lane;
            sonar_col = 2;
            for (int c = 1; c < SUB_COLS; c++) {
                if (grid[sub_lane][c] == 2) {
                    grid[sub_lane][c] = 1;
                    sonar_col = c;
                    break;
                }
            }
        }

        /* Advance ocean objects 1 step left */
        for (int ln = 0; ln < SUB_LANES; ln++) {
            /* Check collision at submarine column (col 1) */
            if (ln == sub_lane) {
                if (grid[ln][1] == 1) {
                    score += 10;
                    app_update_high_score(SCORE_SUBMARINE, score);
                    if (score % 50 == 0) app_add_stars(1);
                    bsp_audio_sfx(SFX_SUCCESS);
                } else if (grid[ln][1] == 2) {
                    hearts--;
                    bsp_audio_sfx(SFX_WRONG);
                }
            }
            for (int c = 1; c < SUB_COLS - 1; c++) {
                grid[ln][c] = grid[ln][c + 1];
            }
            grid[ln][SUB_COLS - 1] = 0;
        }

        /* Spawn new star or single jellyfish (never block all 3 lanes!) */
        int spawn_lane = rand() % SUB_LANES;
        int roll = rand() % 100;
        if (roll < 55) grid[spawn_lane][SUB_COLS - 1] = 1;
        else if (roll < 82) grid[spawn_lane][SUB_COLS - 1] = 2;
    }

    if (hearts <= 0) {
        gfx_fill_round_rect(180, 150, 440, 180, 18, GFX_WHITE);
        gfx_round_rect(180, 150, 440, 180, 18, 4, GFX_BLACK);
        gfx_draw_text_center(GFX_W / 2, 185, "潜艇返航休息啦！", FONT_32, GFX_BLACK);
        char msg[80];
        snprintf(msg, sizeof(msg), "本次收集海星得分：%d 分", score);
        gfx_draw_text_center(GFX_W / 2, 240, msg, FONT_24, GFX_BLACK);
        gfx_draw_text_center(GFX_W / 2, 285, "按下滚轮返回启蒙乐园", FONT_16, GFX_BLACK);
        bsp_epd_smart_present(gfx_get_fb(), false);
        while (bsp_input_wait(30000) != KEY_OK) {}
    }
    app_profile_save();
}

/* =========================================================================
 * MODULE 4: 变声小鹦鹉 · 小小演说家 (Mic Recording + 3-FX Playback)
 * ========================================================================= */
void app_voice_parrot_run(void)
{
    int sel = 0;
    const char *btns[4] = {
        "● 按下开始录音 (最长8秒，再按停止)",
        "▶ 原声回放 (听听自己的声音)",
        "▶ 花栗鼠搞怪声 (快快尖尖超搞笑)",
        "▶ 大狗熊憨憨声 (慢吞吞低沉音)"
    };
    const char *status = "对着右侧麦克风讲个小故事或背首古诗吧！";
    bool first = true;

    while (1) {
        gfx_clear(GFX_WHITE);
        gfx_draw_top_bar("★ 变声小鹦鹉 · 小小演说家", g_prof.stars, app_eye_mins_left());

        gfx_round_rect(24, 56, 250, 366, 16, 3, GFX_BLACK);
        gfx_draw_art(149, 195, 165, ART_PARROT_MIC, GFX_BLACK);
        gfx_draw_paragraph(44, 305, 210, 100, 8, status, FONT_16, GFX_BLACK, NULL);

        for (int i = 0; i < 4; i++) {
            int y = 66 + i * 86;
            if (i == sel) {
                gfx_fill_round_rect(296, y, 480, 70, 14, GFX_BLACK);
                gfx_draw_text(320, y + 23, btns[i], FONT_24, GFX_WHITE);
            } else {
                gfx_round_rect(296, y, 480, 70, 14, 2, GFX_BLACK);
                gfx_draw_text(320, y + 23, btns[i], FONT_24, GFX_BLACK);
            }
        }
        gfx_draw_bottom_hints("滚轮选变声特效", "按下录音/播放", "返回主页");
        bsp_epd_smart_present(gfx_get_fb(), first);
        first = false;

        bsp_key_t k = bsp_input_wait(30000);
        if (k == KEY_BACK) { bsp_audio_stop_all(); break; }
        else if (k == KEY_PREV) { sel = (sel + 3) % 4; bsp_audio_sfx(SFX_WHEEL_TICK); }
        else if (k == KEY_NEXT) { sel = (sel + 1) % 4; bsp_audio_sfx(SFX_WHEEL_TICK); }
        else if (k == KEY_OK) {
            if (sel == 0) {
                gfx_fill_round_rect(180, 160, 440, 150, 16, GFX_BLACK);
                gfx_draw_text_center(GFX_W / 2, 195, "● 正在录音中... 请大声说话！", FONT_24, GFX_WHITE);
                gfx_draw_text_center(GFX_W / 2, 245, "(最长 8 秒，按滚轮可提前结束)", FONT_16, GFX_WHITE);
                bsp_epd_smart_present(gfx_get_fb(), false);

                if (bsp_audio_record_clip(8)) {
                    status = "录音成功！快用滚轮选花栗鼠或大狗熊听听看！";
                    app_add_stars(1);
                    app_profile_save();
                    bsp_audio_sfx(SFX_SUCCESS);
                    sel = 2; /* Jump straight to fun Chipmunk voice! */
                } else {
                    status = "未检测到麦克风录音，请重试。";
                }
                bsp_input_flush();
            } else {
                if (!bsp_audio_has_recorded_clip()) {
                    status = "还没有录音呢，请先选第 1 项录一段话哟！";
                    bsp_audio_sfx(SFX_WRONG);
                } else {
                    bsp_audio_play_recorded_clip((bsp_voice_fx_t)(sel - 1));
                    status = "正在回放变声录音...";
                }
            }
        }
    }
}

/* =========================================================================
 * GAME P3 (3-6岁): 森林八音盒 · 滚轮小琴师 (Thumbwheel 8-Key Xylophone)
 * Dialing sweeps mallet across 8 marimba bars with instant live pitch!
 * ========================================================================= */
typedef struct {
    const char *title;
    int         len;
    uint8_t     notes[16];     /* 0..7 -> Do Re Mi Fa Sol La Si Do+ */
    const char *lyrics[16];
} xylo_song_t;

static const xylo_song_t k_xylo_songs[4] = {
    {
        "一闪一闪小星星", 14,
        { 0, 0, 4, 4, 5, 5, 4,  3, 3, 2, 2, 1, 1, 0 },
        { "一","闪","一","闪","亮","晶","晶", "满","天","都","是","小","星","星" }
    },
    {
        "两只老虎跑得快", 14,
        { 0, 1, 2, 0,  0, 1, 2, 0,  2, 3, 4,  2, 3, 4 },
        { "两","只","老","虎", "两","只","老","虎", "跑","得","快", "跑","得","快" }
    },
    {
        "小兔子乖乖", 13,
        { 2, 4, 2, 4, 5, 4, 2,  1, 2, 1, 0, 1, 0 },
        { "小","兔","子","乖","乖","把","门", "开","开","快","点","开","哟" }
    },
    {
        "我是小小粉刷匠", 14,
        { 4, 2, 4, 2, 4, 2, 0,  1, 3, 2, 1, 4, 4, 0 },
        { "我","是","一","个","粉","刷","匠", "粉","刷","本","领","强","哟","嘿" }
    },
};

static void run_game_xylophone(void)
{
    static const uint16_t k_hz[8]   = { 523, 587, 659, 698, 784, 880, 988, 1047 };
    static const char    *k_num[8]  = { "1", "2", "3", "4", "5", "6", "7", "i" };
    static const char    *k_sol[8]  = { "Do", "Re", "Mi", "Fa", "Sol", "La", "Si", "Do" };

    int song_idx = 0;
    int step = 0;
    int cursor = 0; /* 0..7 = 8 xylophone bars, 8 = [切歌] button */
    int score = 0;
    bool first = true;

    while (1) {
        const xylo_song_t *sg = &k_xylo_songs[song_idx];
        int target_key = sg->notes[step];

        gfx_clear(GFX_WHITE);
        char hdr[96];
        snprintf(hdr, sizeof(hdr), "森林八音盒 · %s   得分:%d", sg->title, score);
        gfx_draw_top_bar(hdr, g_prof.stars, app_eye_mins_left());

        /* Top Guided Sheet Music Card */
        gfx_round_rect(20, 50, GFX_W - 40, 114, 16, 2, GFX_BLACK);
        gfx_draw_text(38, 58, "跟着 ★ 标记拨轮移琴槌，按下敲响儿歌（拨轮经过琴键即可自由刮奏）：", FONT_16, GFX_BLACK);

        int phrase_start = (step / 7) * 7;
        for (int i = 0; i < 7 && (phrase_start + i) < sg->len; i++) {
            int idx = phrase_start + i;
            int nx = 46 + i * 102;
            bool cur = (idx == step);
            bool done = (idx < step);
            char nstr[16];
            snprintf(nstr, sizeof(nstr), "%s %s", k_num[sg->notes[idx]], sg->lyrics[idx]);
            if (cur) {
                gfx_fill_round_rect(nx, 84, 92, 68, 12, GFX_BLACK);
                gfx_draw_text_center(nx + 46, 90, "▼跟弹", FONT_16, GFX_WHITE);
                gfx_draw_text_center(nx + 46, 116, nstr, FONT_24, GFX_WHITE);
            } else {
                gfx_fill_round_rect(nx, 92, 92, 56, 12, done ? GFX_GRAY_LIGHT : GFX_WHITE);
                gfx_round_rect(nx, 92, 92, 56, 12, 2, GFX_BLACK);
                gfx_draw_text_center(nx + 46, 108, nstr, FONT_24, GFX_BLACK);
            }
        }

        /* Wooden Xylophone Support Rails */
        gfx_fill_round_rect(24, 240, 654, 10, 4, GFX_GRAY_MID);
        gfx_fill_round_rect(24, 356, 654, 10, 4, GFX_GRAY_MID);

        /* 8 Graduated Xylophone Bars */
        for (int i = 0; i < 8; i++) {
            int bx = 26 + i * 82;
            int bw = 72;
            int bh = 228 - i * 10;
            int by = 186 + i * 5;
            bool focus = (cursor == i);
            bool is_target = (i == target_key);

            if (focus) {
                gfx_fill_circle(bx + bw / 2, by - 10, 7, GFX_BLACK);
                gfx_fill_round_rect(bx, by, bw, bh, 14, GFX_BLACK);
                gfx_fill_circle(bx + bw / 2, by + 18, 4, GFX_WHITE);
                gfx_fill_circle(bx + bw / 2, by + bh - 18, 4, GFX_WHITE);
                if (is_target) gfx_draw_text_center(bx + bw / 2, by + 32, "★", FONT_24, GFX_WHITE);
                gfx_draw_text_center(bx + bw / 2, by + bh / 2 - 16, k_num[i], FONT_32, GFX_WHITE);
                gfx_draw_text_center(bx + bw / 2, by + bh / 2 + 22, k_sol[i], FONT_16, GFX_WHITE);
            } else {
                gfx_fill_round_rect(bx, by, bw, bh, 14, (i & 1) ? GFX_GRAY_LIGHT : GFX_WHITE);
                gfx_round_rect(bx, by, bw, bh, 14, is_target ? 4 : 2, GFX_BLACK);
                gfx_circle(bx + bw / 2, by + 18, 4, 2, GFX_BLACK);
                gfx_circle(bx + bw / 2, by + bh - 18, 4, 2, GFX_BLACK);
                if (is_target) gfx_draw_text_center(bx + bw / 2, by + 32, "★", FONT_24, GFX_BLACK);
                gfx_draw_text_center(bx + bw / 2, by + bh / 2 - 16, k_num[i], FONT_32, GFX_BLACK);
                gfx_draw_text_center(bx + bw / 2, by + bh / 2 + 22, k_sol[i], FONT_16, GFX_BLACK);
            }
        }

        /* 9th Slot: Switch Nursery Song Button */
        if (cursor == 8) {
            gfx_fill_round_rect(688, 224, 92, 144, 14, GFX_BLACK);
            gfx_draw_text_center(734, 256, "♪", FONT_32, GFX_WHITE);
            gfx_draw_text_center(734, 304, "切歌", FONT_24, GFX_WHITE);
        } else {
            gfx_round_rect(688, 224, 92, 144, 14, 2, GFX_BLACK);
            gfx_draw_text_center(734, 256, "♪", FONT_32, GFX_BLACK);
            gfx_draw_text_center(734, 304, "切歌", FONT_24, GFX_BLACK);
        }

        gfx_draw_bottom_hints("拨轮滑奏琴键", cursor == 8 ? "按下切换儿歌" : "按下敲响音符", "返回游乐园");
        bsp_epd_smart_present(gfx_get_fb(), first);
        first = false;

        bsp_key_t k = bsp_input_wait(30000);
        if (k == KEY_BACK) break;
        else if (k == KEY_PREV) {
            cursor = (cursor + 8) % 9;
            if (cursor < 8) bsp_audio_play_note(k_hz[cursor], 130);
            else bsp_audio_sfx(SFX_WHEEL_TICK);
        } else if (k == KEY_NEXT) {
            cursor = (cursor + 1) % 9;
            if (cursor < 8) bsp_audio_play_note(k_hz[cursor], 130);
            else bsp_audio_sfx(SFX_WHEEL_TICK);
        } else if (k == KEY_OK) {
            if (cursor == 8) {
                song_idx = (song_idx + 1) % 4;
                step = 0;
                bsp_audio_sfx(SFX_CONFIRM);
            } else {
                bsp_audio_play_note(k_hz[cursor], 280);
                if (cursor == target_key) {
                    step++;
                    score += 10;
                    app_update_high_score(SCORE_XYLOPHONE, score);
                    if (step >= sg->len) {
                        score += 20;
                        app_update_high_score(SCORE_XYLOPHONE, score);
                        app_add_stars(1);
                        app_profile_save();
                        vTaskDelay(pdMS_TO_TICKS(240));
                        bsp_audio_sfx(SFX_FANFARE);
                        song_idx = (song_idx + 1) % 4;
                        step = 0;
                    }
                }
            }
        }
    }
    app_profile_save();
}

/* =========================================================================
 * GAME P4 (3-6岁): 小刺猬接果 · 丰收果园 (5-Tree Discrete Catcher)
 * ========================================================================= */
#define ORCH_COLS 5
#define ORCH_ROWS 5

static void draw_apple_sprite(int cx, int cy)
{
    gfx_fill_circle(cx, cy + 2, 15, GFX_BLACK);
    gfx_fill_circle(cx - 5, cy - 3, 3, GFX_WHITE);
    gfx_line(cx, cy - 13, cx + 2, cy - 20, 3, GFX_BLACK);
    gfx_fill_round_rect(cx + 3, cy - 19, 10, 6, 3, GFX_BLACK);
}

static void draw_burr_sprite(int cx, int cy)
{
    gfx_circle(cx, cy, 11, 3, GFX_BLACK);
    gfx_line(cx - 17, cy, cx + 17, cy, 2, GFX_BLACK);
    gfx_line(cx, cy - 17, cx, cy + 17, 2, GFX_BLACK);
    gfx_line(cx - 12, cy - 12, cx + 12, cy + 12, 2, GFX_BLACK);
    gfx_line(cx - 12, cy + 12, cx + 12, cy - 12, 2, GFX_BLACK);
}

static void run_game_hedgehog_orchard(void)
{
    /* 0=empty, 1=apple(+10), 2=star(+20), 3=spiky burr(-1 heart, press OK to puff into star!) */
    uint8_t grid[ORCH_ROWS][ORCH_COLS];
    memset(grid, 0, sizeof(grid));

    int hog_col = 2;
    int score = 0;
    int hearts = 3;
    int puff_col = -1;
    bool first = true;
    const int64_t step_us = 650000LL;
    int64_t next_tick = esp_timer_get_time() + step_us;

    while (hearts > 0) {
        gfx_clear(GFX_WHITE);
        char hdr[96];
        snprintf(hdr, sizeof(hdr), "小刺猬接果   得分:%d   体力:%s",
                 score, hearts == 3 ? "♥♥♥" : (hearts == 2 ? "♥♥" : "♥"));
        gfx_draw_top_bar(hdr, g_prof.stars, app_eye_mins_left());

        gfx_round_rect(22, 50, GFX_W - 44, 380, 16, 2, GFX_BLACK);
        gfx_hline(24, 412, GFX_W - 48, GFX_BLACK);

        for (int c = 0; c < ORCH_COLS; c++) {
            int cx = 112 + c * 144;
            /* Leafy orchard tree canopy */
            gfx_fill_round_rect(cx - 48, 56, 96, 28, 14, GFX_GRAY_LIGHT);
            gfx_round_rect(cx - 48, 56, 96, 28, 14, 2, GFX_BLACK);

            for (int r = 0; r < ORCH_ROWS; r++) {
                int cy = 108 + r * 52;
                uint8_t cell = grid[r][c];
                if (cell == 1) draw_apple_sprite(cx, cy);
                else if (cell == 2) gfx_draw_text_center(cx, cy - 16, "★", FONT_32, GFX_BLACK);
                else if (cell == 3) draw_burr_sprite(cx, cy);
            }

            if (c == hog_col) {
                if (puff_col == c) {
                    gfx_circle(cx, 376, 40, 3, GFX_BLACK);
                }
                gfx_draw_art(cx, 380, 64, ART_HEDGEHOG, GFX_BLACK);
            } else {
                gfx_draw_text_center(cx, 386, "·", FONT_24, GFX_GRAY_MID);
            }
        }

        gfx_draw_bottom_hints("拨轮移动小刺猬接红果", "按下鼓起小刺把毛栗变星星", "返回游乐园");
        bsp_epd_smart_present(gfx_get_fb(), first);
        first = false;
        puff_col = -1;

        int64_t now = esp_timer_get_time();
        int wait_ms = (int)((next_tick - now) / 1000LL);
        if (wait_ms < 20) wait_ms = 20;

        bsp_key_t k = bsp_input_wait(wait_ms);
        if (k == KEY_BACK) break;
        if (k == KEY_PREV && hog_col > 0) {
            hog_col--;
            bsp_audio_sfx(SFX_WHEEL_TICK);
        } else if (k == KEY_NEXT && hog_col < ORCH_COLS - 1) {
            hog_col++;
            bsp_audio_sfx(SFX_WHEEL_TICK);
        } else if (k == KEY_OK) {
            puff_col = hog_col;
            bsp_audio_sfx(SFX_BUBBLE);
            for (int r = ORCH_ROWS - 1; r >= 0; r--) {
                if (grid[r][hog_col] == 3) {
                    grid[r][hog_col] = 2;
                    break;
                }
            }
        }

        if (esp_timer_get_time() >= next_tick) {
            next_tick = esp_timer_get_time() + step_us;
            /* Check fruit landing on bottom row */
            uint8_t hit = grid[ORCH_ROWS - 1][hog_col];
            if (hit == 1 || hit == 2) {
                score += (hit == 2) ? 20 : 10;
                app_update_high_score(SCORE_HEDGEHOG, score);
                if (score % 60 == 0) app_add_stars(1);
                bsp_audio_sfx(SFX_SUCCESS);
            } else if (hit == 3) {
                hearts--;
                bsp_audio_sfx(SFX_WRONG);
            }

            for (int r = ORCH_ROWS - 1; r > 0; r--) {
                for (int c = 0; c < ORCH_COLS; c++) grid[r][c] = grid[r - 1][c];
            }
            for (int c = 0; c < ORCH_COLS; c++) grid[0][c] = 0;

            int sc = rand() % ORCH_COLS;
            int roll = rand() % 100;
            if (roll < 58)      grid[0][sc] = 1;
            else if (roll < 74) grid[0][sc] = 2;
            else if (roll < 92) grid[0][sc] = 3;
        }
    }

    if (hearts <= 0) {
        gfx_fill_round_rect(180, 155, 440, 170, 18, GFX_WHITE);
        gfx_round_rect(180, 155, 440, 170, 18, 4, GFX_BLACK);
        gfx_draw_text_center(GFX_W / 2, 188, "小刺猬背满果子回家啦！", FONT_24, GFX_BLACK);
        char msg[80];
        snprintf(msg, sizeof(msg), "本次丰收得分：%d 分", score);
        gfx_draw_text_center(GFX_W / 2, 236, msg, FONT_24, GFX_BLACK);
        gfx_draw_text_center(GFX_W / 2, 282, "按下滚轮返回启蒙游乐园", FONT_16, GFX_BLACK);
        bsp_epd_smart_present(gfx_get_fb(), false);
        while (bsp_input_wait(30000) != KEY_OK) {}
    }
    app_profile_save();
}

/* =========================================================================
 * GAME P5 (3-6岁): 规律小火车 · 动脑装车厢 (Visual Pattern Train — Zero Reading!)
 * ========================================================================= */
static void make_train_puzzle(gfx_art_id_t seq[6], gfx_art_id_t opts[4], int *ans_idx)
{
    static const gfx_art_id_t k_pool[8] = {
        ART_RABBIT, ART_TURTLE, ART_PANDA, ART_FOX,
        ART_DINO, ART_TADPOLE_FROG, ART_CAT, ART_PENGUIN
    };
    int idx[8] = {0,1,2,3,4,5,6,7};
    for (int i = 7; i > 0; i--) {
        int j = rand() % (i + 1);
        int t = idx[i]; idx[i] = idx[j]; idx[j] = t;
    }
    gfx_art_id_t A = k_pool[idx[0]], B = k_pool[idx[1]], C = k_pool[idx[2]], D = k_pool[idx[3]];

    int pat = rand() % 4;
    if (pat == 0) {
        /* ABABAB */
        seq[0] = A; seq[1] = B; seq[2] = A; seq[3] = B; seq[4] = A; seq[5] = B;
    } else if (pat == 1) {
        /* AABAAB */
        seq[0] = A; seq[1] = A; seq[2] = B; seq[3] = A; seq[4] = A; seq[5] = B;
    } else if (pat == 2) {
        /* ABBABB */
        seq[0] = A; seq[1] = B; seq[2] = B; seq[3] = A; seq[4] = B; seq[5] = B;
    } else {
        /* ABCABC */
        seq[0] = A; seq[1] = B; seq[2] = C; seq[3] = A; seq[4] = B; seq[5] = C;
    }

    gfx_art_id_t cands[4] = { A, B, C, D };
    /* Shuffle the 4 option cards */
    for (int i = 3; i > 0; i--) {
        int j = rand() % (i + 1);
        gfx_art_id_t t = cands[i]; cands[i] = cands[j]; cands[j] = t;
    }
    *ans_idx = 0;
    for (int i = 0; i < 4; i++) {
        opts[i] = cands[i];
        if (opts[i] == seq[5]) *ans_idx = i;
    }
}

static void run_game_pattern_train(void)
{
    gfx_art_id_t seq[6], opts[4];
    int ans_idx = 0, sel = 0, score = 0, trains_sent = 0;
    make_train_puzzle(seq, opts, &ans_idx);
    const char *msg = "观察前 5 节车厢的排列规律，拨轮选第 6 节小动物挂上火车吧！";
    bool first = true;

    while (1) {
        gfx_clear(GFX_WHITE);
        char hdr[96];
        snprintf(hdr, sizeof(hdr), "规律小火车   已发车:%d列   得分:%d", trains_sent, score);
        gfx_draw_top_bar(hdr, g_prof.stars, app_eye_mins_left());

        /* Railway Tracks */
        gfx_hline(16, 206, GFX_W - 32, GFX_BLACK);
        gfx_hline(16, 212, GFX_W - 32, GFX_BLACK);
        for (int tx = 24; tx < GFX_W - 28; tx += 28) {
            gfx_vline(tx, 206, 10, GFX_BLACK);
        }

        /* Steam Locomotive + 6 Coupled Freight Cars */
        gfx_draw_art(56, 150, 64, ART_TRAIN, GFX_BLACK);
        for (int i = 0; i < 6; i++) {
            int cx = 104 + i * 112;
            gfx_line(cx - 12, 168, cx, 168, 4, GFX_BLACK);
            if (i < 5) {
                gfx_fill_round_rect(cx, 88, 100, 102, 12, GFX_WHITE);
                gfx_round_rect(cx, 88, 100, 102, 12, 3, GFX_BLACK);
                gfx_draw_art(cx + 50, 139, 64, seq[i], GFX_BLACK);
            } else {
                gfx_fill_round_rect(cx, 88, 100, 102, 12, GFX_GRAY_LIGHT);
                gfx_round_rect(cx, 88, 100, 102, 12, 4, GFX_BLACK);
                gfx_draw_text_center(cx + 50, 122, "？", FONT_32, GFX_BLACK);
            }
            gfx_fill_circle(cx + 24, 198, 9, GFX_BLACK);
            gfx_fill_circle(cx + 76, 198, 9, GFX_BLACK);
            gfx_fill_circle(cx + 24, 198, 3, GFX_WHITE);
            gfx_fill_circle(cx + 76, 198, 3, GFX_WHITE);
        }

        /* Bottom 4 Candidate Carriages */
        for (int i = 0; i < 4; i++) {
            int ox = 42 + i * 186;
            int oy = 236;
            bool focus = (i == sel);
            if (focus) {
                gfx_round_rect(ox - 3, oy - 3, 162, 134, 16, 5, GFX_BLACK);
                gfx_fill_round_rect(ox + 10, oy + 92, 136, 30, 8, GFX_BLACK);
                gfx_draw_art(ox + 78, oy + 46, 64, opts[i], GFX_BLACK);
                gfx_draw_text_center(ox + 78, oy + 99, "▲挂上车厢", FONT_16, GFX_WHITE);
            } else {
                gfx_round_rect(ox, oy, 156, 128, 14, 2, GFX_BLACK);
                gfx_draw_art(ox + 78, oy + 48, 64, opts[i], GFX_BLACK);
                gfx_draw_text_center(ox + 78, oy + 99, "候选车厢", FONT_16, GFX_BLACK);
            }
        }

        gfx_round_rect(20, 382, GFX_W - 40, 46, 10, 2, GFX_BLACK);
        gfx_draw_text_center(GFX_W / 2, 396, msg, FONT_16, GFX_BLACK);
        gfx_draw_bottom_hints("滚轮选动物车厢", "按下挂接发车", "返回游乐园");
        bsp_epd_smart_present(gfx_get_fb(), first);
        first = false;

        bsp_key_t k = bsp_input_wait(30000);
        if (k == KEY_BACK) break;
        else if (k == KEY_PREV) { sel = (sel + 3) % 4; bsp_audio_sfx(SFX_WHEEL_TICK); }
        else if (k == KEY_NEXT) { sel = (sel + 1) % 4; bsp_audio_sfx(SFX_WHEEL_TICK); }
        else if (k == KEY_OK) {
            if (sel == ans_idx) {
                trains_sent++;
                score += 15;
                app_update_high_score(SCORE_PATTERN_TRAIN, score);
                if (trains_sent % 3 == 0) app_add_stars(1);
                bsp_audio_sfx(SFX_SUCCESS);
                msg = "★ 呜——呜——！规律完全正确，小火车满载出发喽！";
                make_train_puzzle(seq, opts, &ans_idx);
                sel = 0;
            } else {
                bsp_audio_sfx(SFX_WRONG);
                msg = "再仔细看看前面小动物是怎么排队的，换一节试试看！";
            }
        }
    }
    app_profile_save();
}

/* =========================================================================
 * GAME P6 (3-6岁): 熊猫叠蛋糕 · 甜蜜高塔 (Thumbwheel Crane Cake Stacker)
 * ========================================================================= */
static void run_game_panda_stack(void)
{
    int layer_cx[8];
    int layer_w[8];
    int layers = 0;
    int cur_w = 200;
    int top_cx = 512;
    int crane_slot = 4; /* 0..8 -> cx = 320 + slot * 48 (slot 4 = 512) */
    int breeze_dir = 1;
    int score = 0;
    int misses_left = 3;
    const char *tip = "拨轮对准下方托盘中心，按下滚轮放下美味蛋糕！";
    bool first = true;

    const int64_t sway_us = 750000LL;
    int64_t next_sway = esp_timer_get_time() + sway_us;

    while (misses_left > 0) {
        gfx_clear(GFX_WHITE);
        char hdr[96];
        snprintf(hdr, sizeof(hdr), "熊猫叠蛋糕   高度:%d/8层   得分:%d", layers, score);
        gfx_draw_top_bar(hdr, g_prof.stars, app_eye_mins_left());

        /* Left Chef Panda Card */
        gfx_round_rect(20, 52, 212, 372, 16, 3, GFX_BLACK);
        gfx_draw_art(126, 146, 96, ART_PANDA, GFX_BLACK);
        gfx_draw_text_center(126, 212, "熊猫甜点师", FONT_24, GFX_BLACK);
        char st1[48], st2[48];
        snprintf(st1, sizeof(st1), "机会：%s", misses_left == 3 ? "♥♥♥" : (misses_left == 2 ? "♥♥" : "♥"));
        snprintf(st2, sizeof(st2), "目标：叠满 8 层");
        gfx_draw_text_center(126, 256, st1, FONT_24, GFX_BLACK);
        gfx_draw_text_center(126, 294, st2, FONT_16, GFX_BLACK);
        gfx_draw_paragraph(34, 330, 184, 82, 6, tip, FONT_16, GFX_BLACK, NULL);

        /* Right Cake Tower Arena (center = 512) */
        gfx_round_rect(246, 52, 534, 372, 16, 2, GFX_BLACK);

        /* Top Crane & Held Cake Layer */
        int drop_cx = 320 + crane_slot * 48;
        gfx_hline(266, 68, 494, GFX_BLACK);
        gfx_vline(drop_cx, 68, 22, GFX_BLACK);
        gfx_vline(drop_cx + 1, 68, 22, GFX_BLACK);
        gfx_fill_round_rect(drop_cx - cur_w / 2, 90, cur_w, 28, 8, GFX_GRAY_LIGHT);
        gfx_round_rect(drop_cx - cur_w / 2, 90, cur_w, 28, 8, 3, GFX_BLACK);
        gfx_hline(drop_cx - cur_w / 2 + 8, 98, cur_w - 16, GFX_BLACK);

        /* Guide Center Dotted Line */
        for (int gy = 124; gy < 392; gy += 18) {
            gfx_vline(top_cx, gy, 8, GFX_GRAY_MID);
        }

        /* Serving Plate at Bottom */
        gfx_fill_round_rect(352, 396, 320, 14, 7, GFX_BLACK);

        /* Stacked Cake Layers */
        for (int i = 0; i < layers; i++) {
            int ly = 396 - (i + 1) * 30;
            int lx = layer_cx[i] - layer_w[i] / 2;
            gfx_fill_round_rect(lx, ly, layer_w[i], 30, 8, (i & 1) ? GFX_WHITE : GFX_GRAY_LIGHT);
            gfx_round_rect(lx, ly, layer_w[i], 30, 8, 3, GFX_BLACK);
            /* Cream frosting scallops */
            for (int sx = lx + 16; sx < lx + layer_w[i] - 14; sx += 24) {
                gfx_fill_circle(sx, ly + 8, 4, GFX_BLACK);
            }
        }

        gfx_draw_bottom_hints("拨轮左右对齐蛋糕塔", "按下放下蛋糕", "返回游乐园");
        bsp_epd_smart_present(gfx_get_fb(), first);
        first = false;

        int64_t now = esp_timer_get_time();
        int wait_ms = (int)((next_sway - now) / 1000LL);
        if (wait_ms < 25) wait_ms = 25;

        bsp_key_t k = bsp_input_wait(wait_ms);
        if (k == KEY_BACK) break;
        if (k == KEY_PREV && crane_slot > 0) {
            crane_slot--;
            bsp_audio_sfx(SFX_WHEEL_TICK);
        } else if (k == KEY_NEXT && crane_slot < 8) {
            crane_slot++;
            bsp_audio_sfx(SFX_WHEEL_TICK);
        } else if (k == KEY_OK) {
            int diff = abs(drop_cx - top_cx);
            if (diff <= 24) {
                layer_cx[layers] = drop_cx;
                layer_w[layers] = cur_w;
                top_cx = drop_cx;
                layers++;
                score += 25;
                app_update_high_score(SCORE_PANDA_STACK, score);
                bsp_audio_sfx(SFX_SUCCESS);
                tip = "★ 完美居中对齐！奶油香喷喷 +25分！";
            } else if (diff <= cur_w / 2 + 24) {
                cur_w -= diff / 2;
                if (cur_w < 96) cur_w = 96;
                top_cx = (top_cx + drop_cx) / 2;
                layer_cx[layers] = top_cx;
                layer_w[layers] = cur_w;
                layers++;
                score += 15;
                app_update_high_score(SCORE_PANDA_STACK, score);
                bsp_audio_sfx(SFX_CONFIRM);
                tip = "稳稳接住啦 +15分！继续瞄准中心往上叠！";
            } else {
                misses_left--;
                bsp_audio_sfx(SFX_WRONG);
                tip = "哎呀偏出托盘啦，再瞄准虚线试一次！";
            }

            if (layers >= 8) {
                score += 50;
                app_update_high_score(SCORE_PANDA_STACK, score);
                app_add_stars(2);
                app_profile_save();
                bsp_audio_sfx(SFX_FANFARE);

                /* Draw 3 lit birthday candles on top & celebrate! */
                int top_y = 396 - 8 * 30;
                for (int c = -1; c <= 1; c++) {
                    gfx_fill_rect(top_cx + c * 26 - 3, top_y - 22, 6, 22, GFX_BLACK);
                    gfx_fill_circle(top_cx + c * 26, top_y - 28, 5, GFX_BLACK);
                }
                gfx_fill_round_rect(290, 170, 444, 140, 18, GFX_WHITE);
                gfx_round_rect(290, 170, 444, 140, 18, 4, GFX_BLACK);
                gfx_draw_text_center(512, 198, "★ 八层生日大蛋糕完成！奖励2颗星 ★", FONT_24, GFX_BLACK);
                gfx_draw_text_center(512, 254, "按下滚轮继续挑战下一个蛋糕", FONT_16, GFX_BLACK);
                bsp_epd_smart_present(gfx_get_fb(), false);
                while (bsp_input_wait(30000) != KEY_OK) {}

                layers = 0;
                cur_w = 200;
                top_cx = 512;
                tip = "新蛋糕开始喽，看看这次能拿多少分！";
            }
            next_sway = esp_timer_get_time() + sway_us;
            continue;
        }

        if (esp_timer_get_time() >= next_sway) {
            next_sway = esp_timer_get_time() + sway_us;
            crane_slot += breeze_dir;
            if (crane_slot >= 7) breeze_dir = -1;
            else if (crane_slot <= 1) breeze_dir = 1;
        }
    }
    app_profile_save();
}

/* =========================================================================
 * PRESCHOOL PARK HUB (3-6岁 · 6 Thumbwheel-Native Games in 3x2 Grid)
 * ========================================================================= */
void app_preschool_park_run(void)
{
    static const struct {
        const char    *title;
        const char    *sub;
        gfx_art_id_t   art;
        app_score_id_t sid;
    } k_games[6] = {
        { "滚轮配配乐", "常识古诗摩天轮", ART_FERRIS_WHEEL, SCORE_SAFARI_MATCH  },
        { "海底小潜艇", "三水道声呐摘星", ART_SUBMARINE,    SCORE_SUBMARINE     },
        { "森林八音盒", "滚轮刮奏小儿歌", ART_XYLOPHONE,    SCORE_XYLOPHONE     },
        { "小刺猬接果", "五棵果树接红果", ART_HEDGEHOG,     SCORE_HEDGEHOG      },
        { "规律小火车", "纯图形找规律车", ART_TRAIN,        SCORE_PATTERN_TRAIN },
        { "熊猫叠蛋糕", "对齐吊臂叠高塔", ART_CAKE_TOWER,   SCORE_PANDA_STACK   },
    };

    int sel = 0;
    bool first = true;
    while (1) {
        gfx_clear(GFX_WHITE);
        gfx_draw_top_bar("★ 启蒙游乐园 (3-6岁 · 6款滚轮启蒙游戏)", g_prof.stars, app_eye_mins_left());

        for (int i = 0; i < 6; i++) {
            int col = i % 3;
            int row = i / 3;
            int x = 22 + col * 258;
            int y = 52 + row * 190;
            bool focus = (i == sel);

            if (focus) {
                gfx_round_rect(x - 2, y - 2, 244, 182, 18, 5, GFX_BLACK);
                gfx_fill_round_rect(x + 10, y + 96, 220, 34, 10, GFX_BLACK);
                gfx_draw_art(x + 120, y + 48, 88, k_games[i].art, GFX_BLACK);
                gfx_draw_text_center(x + 120, y + 101, k_games[i].title, FONT_24, GFX_WHITE);
            } else {
                gfx_round_rect(x, y, 240, 178, 16, 2, GFX_BLACK);
                gfx_draw_art(x + 120, y + 48, 85, k_games[i].art, GFX_BLACK);
                gfx_draw_text_center(x + 120, y + 101, k_games[i].title, FONT_24, GFX_BLACK);
            }
            gfx_draw_text_center(x + 120, y + 135, k_games[i].sub, FONT_16, GFX_BLACK);
            char sc[48];
            snprintf(sc, sizeof(sc), "最高纪录: %d 分", app_get_high_score(k_games[i].sid));
            gfx_draw_text_center(x + 120, y + 156, sc, FONT_16, GFX_BLACK);
        }

        gfx_draw_bottom_hints("滚轮选启蒙游戏", "按下开始玩耍", "返回主页");
        bsp_epd_smart_present(gfx_get_fb(), first);
        first = false;

        bsp_key_t k = bsp_input_wait(30000);
        if (k == KEY_BACK) break;
        else if (k == KEY_PREV) { sel = (sel + 5) % 6; bsp_audio_sfx(SFX_WHEEL_TICK); }
        else if (k == KEY_NEXT) { sel = (sel + 1) % 6; bsp_audio_sfx(SFX_WHEEL_TICK); }
        else if (k == KEY_OK) {
            bsp_audio_sfx(SFX_CONFIRM);
            switch (sel) {
            case 0: run_game_safari_match();     break;
            case 1: run_game_submarine();        break;
            case 2: run_game_xylophone();        break;
            case 3: run_game_hedgehog_orchard(); break;
            case 4: run_game_pattern_train();    break;
            case 5: run_game_panda_stack();      break;
            }
            first = true;
        }
        app_check_eye_guard();
    }
}
