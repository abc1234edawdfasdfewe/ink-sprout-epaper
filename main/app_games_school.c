#include "app_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_timer.h"

/* =========================================================================
 * GAME 3 (5-10岁): 数字摩天轮 · 算术高塔 (Math Wheel Tower)
 * ========================================================================= */
static void make_math_question(bool hard, char *eq_buf, size_t eq_sz, int opts[5], int *ans_idx)
{
    int ans = 0;
    if (!hard) {
        /* 20以内加减 */
        if (rand() % 2 == 0) {
            int a = 2 + rand() % 11;
            int b = 1 + rand() % 9;
            ans = a + b;
            snprintf(eq_buf, eq_sz, "%d  ＋  %d  ＝  [ ? ]", a, b);
        } else {
            int a = 8 + rand() % 12;
            int b = 1 + rand() % (a - 1);
            ans = a - b;
            snprintf(eq_buf, eq_sz, "%d  －  %d  ＝  [ ? ]", a, b);
        }
    } else {
        /* 学龄进阶：乘除填空 & 百以内加减 */
        int mode = rand() % 3;
        if (mode == 0) {
            int a = 2 + rand() % 8;
            int b = 2 + rand() % 8;
            ans = b;
            snprintf(eq_buf, eq_sz, "%d  ×  [ ? ]  ＝  %d", a, a * b);
        } else if (mode == 1) {
            int b = 2 + rand() % 8;
            int q = 2 + rand() % 8;
            ans = q;
            snprintf(eq_buf, eq_sz, "%d  ÷  %d  ＝  [ ? ]", b * q, b);
        } else {
            int a = 18 + rand() % 45;
            int b = 12 + rand() % 35;
            ans = a + b;
            snprintf(eq_buf, eq_sz, "%d  ＋  %d  ＝  [ ? ]", a, b);
        }
    }

    *ans_idx = rand() % 5;
    for (int i = 0; i < 5; i++) {
        if (i == *ans_idx) {
            opts[i] = ans;
        } else {
            int cand;
            bool dup;
            do {
                dup = false;
                int delta = (rand() % 9) - 4;
                if (delta == 0) delta = (i + 1);
                cand = ans + delta;
                if (cand < 0) cand = ans + i + 1;
                for (int j = 0; j < i; j++) if (opts[j] == cand) dup = true;
                if (cand == ans) dup = true;
            } while (dup);
            opts[i] = cand;
        }
    }
}

static void run_game_math_tower(void)
{
    /* Choose tier first */
    bool hard = false;
    {
        int tsel = 0;
        bool f = true;
        while (1) {
            gfx_clear(GFX_WHITE);
            gfx_draw_top_bar("数字摩天轮 · 选择难度", g_prof.stars, app_eye_mins_left());
            const char *tname[2] = { "幼小衔接档 (20以内趣味加减)", "学龄进阶档 (乘除推理与百内速算)" };
            for (int i = 0; i < 2; i++) {
                int y = 135 + i * 105;
                if (i == tsel) {
                    gfx_fill_round_rect(120, y, 560, 76, 16, GFX_BLACK);
                    gfx_draw_text_center(GFX_W / 2, y + 26, tname[i], FONT_24, GFX_WHITE);
                } else {
                    gfx_round_rect(120, y, 560, 76, 16, 3, GFX_BLACK);
                    gfx_draw_text_center(GFX_W / 2, y + 26, tname[i], FONT_24, GFX_BLACK);
                }
            }
            gfx_draw_bottom_hints("滚轮选难度", "按下进入高塔", "返回挑战营");
            bsp_epd_smart_present(gfx_get_fb(), f);
            f = false;
            bsp_key_t k = bsp_input_wait(30000);
            if (k == KEY_BACK) return;
            if (k == KEY_PREV || k == KEY_NEXT) { tsel ^= 1; bsp_audio_sfx(SFX_WHEEL_TICK); }
            else if (k == KEY_OK) { hard = (tsel == 1); bsp_audio_sfx(SFX_CONFIRM); break; }
        }
    }

    char eq[80];
    int opts[5], ans_idx = 0, sel = 2;
    int floor_lv = 1, streak = 0, score = 0;
    make_math_question(hard, eq, sizeof(eq), opts, &ans_idx);
    const char *msg = "滚动拨轮旋转底部数字转盘，按下发射能量球！";
    bool first = true;

    while (1) {
        gfx_clear(GFX_WHITE);
        char hdr[96];
        snprintf(hdr, sizeof(hdr), "算术高塔 第%d层   连击:%d   得分:%d", floor_lv, streak, score);
        gfx_draw_top_bar(hdr, g_prof.stars, app_eye_mins_left());

        gfx_round_rect(40, 62, GFX_W - 80, 150, 18, 3, GFX_BLACK);
        gfx_draw_text_center(GFX_W / 2, 86, "解开魔法算式打开石门：", FONT_16, GFX_BLACK);
        gfx_draw_text_center(GFX_W / 2, 130, eq, FONT_32, GFX_BLACK);

        /* 5-Slot Number Dial Wheel */
        for (int i = 0; i < 5; i++) {
            int x = 52 + i * 144;
            int y = 242;
            char num[24];
            snprintf(num, sizeof(num), "%d", opts[i]);
            if (i == sel) {
                gfx_fill_round_rect(x, y - 12, 120, 110, 18, GFX_BLACK);
                gfx_draw_text_center(x + 60, y + 26, num, FONT_32, GFX_WHITE);
                gfx_draw_text_center(x + 60, y + 68, "▲发射", FONT_16, GFX_WHITE);
            } else {
                gfx_round_rect(x, y, 120, 88, 16, 2, GFX_BLACK);
                gfx_draw_text_center(x + 60, y + 28, num, FONT_32, GFX_BLACK);
            }
        }

        gfx_draw_text_center(GFX_W / 2, 392, msg, FONT_24, GFX_BLACK);
        gfx_draw_bottom_hints("滚轮左右旋转数字盘", "按下发射答案", "返回挑战营");
        bsp_epd_smart_present(gfx_get_fb(), first);
        first = false;

        bsp_key_t k = bsp_input_wait(30000);
        if (k == KEY_BACK) break;
        else if (k == KEY_PREV) { sel = (sel + 4) % 5; bsp_audio_sfx(SFX_WHEEL_TICK); }
        else if (k == KEY_NEXT) { sel = (sel + 1) % 5; bsp_audio_sfx(SFX_WHEEL_TICK); }
        else if (k == KEY_OK) {
            if (sel == ans_idx) {
                streak++;
                score += 10 + streak * 2;
                floor_lv++;
                app_update_high_score(SCORE_MATH_TOWER, score);
                if (floor_lv % 4 == 0) app_add_stars(1);
                bsp_audio_sfx(SFX_SUCCESS);
                msg = "★ 命中！石门开启，勇者登上更高一层！";
                make_math_question(hard, eq, sizeof(eq), opts, &ans_idx);
            } else {
                streak = 0;
                bsp_audio_sfx(SFX_WRONG);
                msg = "能量偏了一点点，重新口算转转盘再试一次！";
            }
        }
    }
    app_profile_save();
}

/* =========================================================================
 * GAME 4 (4-12岁): 翻转记忆牌 (Wheel Memory Flip — 3x4 = 12 Cards)
 * ========================================================================= */
static void run_game_memory_flip(void)
{
    static const char *k_pair_words[6] = { "小兔子", "小乌龟", "小恐龙", "大熊猫", "小狐狸", "小闹钟" };
    static const gfx_art_id_t k_pair_arts[6] = {
        ART_RABBIT, ART_TURTLE, ART_DINO, ART_PANDA, ART_FOX, ART_ALARM_CLOCK
    };

    int deck[12];
    bool matched[12] = {false};
    for (int i = 0; i < 12; i++) deck[i] = i / 2;
    for (int i = 11; i > 0; i--) {
        int j = rand() % (i + 1);
        int t = deck[i]; deck[i] = deck[j]; deck[j] = t;
    }

    int cursor = 0, pick1 = -1, pick2 = -1;
    int moves = 0, pairs_left = 6;
    bool first = true;

    while (1) {
        gfx_clear(GFX_WHITE);
        char hdr[96];
        snprintf(hdr, sizeof(hdr), "翻转记忆牌   步数:%d   剩余:%d对", moves, pairs_left);
        gfx_draw_top_bar(hdr, g_prof.stars, app_eye_mins_left());

        for (int i = 0; i < 12; i++) {
            int col = i % 4;
            int row = i / 4;
            int x = 44 + col * 182;
            int y = 58 + row * 122;
            bool face_up = matched[i] || (i == pick1) || (i == pick2);
            bool focus = (i == cursor);

            if (matched[i]) {
                gfx_fill_round_rect(x, y, 164, 108, 14, GFX_GRAY_LIGHT);
                gfx_round_rect(x, y, 164, 108, 14, focus ? 4 : 2, GFX_BLACK);
                gfx_draw_art(x + 42, y + 54, 60, k_pair_arts[deck[i]], GFX_BLACK);
                gfx_draw_text_center(x + 114, y + 28, k_pair_words[deck[i]], FONT_24, GFX_BLACK);
                gfx_draw_text_center(x + 114, y + 66, "✓已配对", FONT_16, GFX_BLACK);
            } else if (face_up) {
                gfx_fill_round_rect(x, y, 164, 108, 14, GFX_WHITE);
                gfx_round_rect(x, y, 164, 108, 14, 4, GFX_BLACK);
                gfx_draw_art(x + 44, y + 54, 60, k_pair_arts[deck[i]], GFX_BLACK);
                gfx_draw_text_center(x + 116, y + 42, k_pair_words[deck[i]], FONT_24, GFX_BLACK);
            } else {
                if (focus) {
                    gfx_fill_round_rect(x, y, 164, 108, 14, GFX_BLACK);
                    gfx_draw_text_center(x + 82, y + 40, "？翻开", FONT_24, GFX_WHITE);
                } else {
                    gfx_round_rect(x, y, 164, 108, 14, 2, GFX_BLACK);
                    gfx_draw_text_center(x + 82, y + 40, "★", FONT_24, GFX_BLACK);
                }
            }
        }

        if (pairs_left == 0) {
            int score = (moves <= 12) ? 100 : (160 - moves * 5);
            if (score < 20) score = 20;
            app_update_high_score(SCORE_MEMORY_FLIP, score);
            app_add_stars(2);
            app_profile_save();
            bsp_audio_sfx(SFX_FANFARE);

            gfx_fill_round_rect(170, 150, 460, 175, 18, GFX_WHITE);
            gfx_round_rect(170, 150, 460, 175, 18, 4, GFX_BLACK);
            gfx_draw_text_center(GFX_W / 2, 185, "★ 记忆大师！全部配对成功 ★", FONT_24, GFX_BLACK);
            char res[80];
            snprintf(res, sizeof(res), "共翻牌 %d 步 · 评分 %d 分 · 奖励 2 颗星", moves, score);
            gfx_draw_text_center(GFX_W / 2, 235, res, FONT_16, GFX_BLACK);
            gfx_draw_text_center(GFX_W / 2, 280, "按下滚轮返回挑战营", FONT_16, GFX_BLACK);
            bsp_epd_smart_present(gfx_get_fb(), false);
            while (bsp_input_wait(30000) != KEY_OK) {}
            break;
        }

        gfx_draw_bottom_hints("滚轮环形移光标", "按下翻开卡牌", "返回挑战营");
        bsp_epd_smart_present(gfx_get_fb(), first);
        first = false;

        /* If two non-matching cards were just shown, pause 750ms so child remembers them, then flip back */
        if (pick1 >= 0 && pick2 >= 0) {
            bsp_input_wait(750);
            pick1 = -1;
            pick2 = -1;
            continue;
        }

        bsp_key_t k = bsp_input_wait(30000);
        if (k == KEY_BACK) break;
        else if (k == KEY_PREV) { cursor = (cursor + 11) % 12; bsp_audio_sfx(SFX_WHEEL_TICK); }
        else if (k == KEY_NEXT) { cursor = (cursor + 1) % 12; bsp_audio_sfx(SFX_WHEEL_TICK); }
        else if (k == KEY_OK) {
            if (matched[cursor] || cursor == pick1) continue;
            if (pick1 < 0) {
                pick1 = cursor;
                bsp_audio_sfx(SFX_CONFIRM);
            } else {
                pick2 = cursor;
                moves++;
                if (deck[pick1] == deck[pick2]) {
                    matched[pick1] = true;
                    matched[pick2] = true;
                    pick1 = -1;
                    pick2 = -1;
                    pairs_left--;
                    bsp_audio_sfx(SFX_SUCCESS);
                } else {
                    bsp_audio_sfx(SFX_PAGE_TURN);
                }
            }
        }
    }
}

/* =========================================================================
 * GAME 5 (6-12岁): 滚轮弹球 · 打砖块 (Thumbwheel Breakout)
 * Pure 1D paddle control — born for rotary/thumbwheel!
 * ========================================================================= */
static void run_game_breakout(void)
{
    bool bricks[3][7];
    memset(bricks, true, sizeof(bricks));
    int bricks_left = 21;

    int pad_x = 330; /* Paddle width 140px inside arena x=[30..770] */
    const int pad_w = 140;
    int bx = 400, by = 370;
    int vx = 34, vy = -28;
    bool launched = false;
    int lives = 3, score = 0;
    bool first = true;

    while (lives > 0 && bricks_left > 0) {
        gfx_clear(GFX_WHITE);
        char hdr[96];
        snprintf(hdr, sizeof(hdr), "滚轮弹球打砖块   得分:%d   球数:%d", score, lives);
        gfx_draw_top_bar(hdr, g_prof.stars, app_eye_mins_left());

        gfx_round_rect(26, 52, GFX_W - 52, 376, 12, 3, GFX_BLACK);

        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 7; c++) {
                if (!bricks[r][c]) continue;
                int x = 44 + c * 102;
                int y = 70 + r * 36;
                gfx_fill_round_rect(x, y, 90, 24, 6, (r == 1) ? GFX_GRAY_MID : GFX_BLACK);
                gfx_round_rect(x, y, 90, 24, 6, 2, GFX_BLACK);
            }
        }

        /* Paddle & Ball */
        gfx_fill_round_rect(pad_x, 394, pad_w, 16, 8, GFX_BLACK);
        if (!launched) {
            bx = pad_x + pad_w / 2;
            by = 380;
        }
        gfx_fill_circle(bx, by, 10, GFX_BLACK);

        gfx_draw_bottom_hints("拨动/长拨滚轮移动挡板", launched ? "稳住接球" : "按下滚轮发射弹球", "返回挑战营");
        bsp_epd_smart_present(gfx_get_fb(), first);
        first = false;

        bsp_key_t k = bsp_input_wait(launched ? 260 : 30000);
        if (k == KEY_BACK) break;

        /* Allow both queued clicks AND held thumbwheel for extra responsive big paddle sweep */
        if (k == KEY_PREV || bsp_input_wheel_prev_held()) pad_x -= 56;
        if (k == KEY_NEXT || bsp_input_wheel_next_held()) pad_x += 56;
        if (pad_x < 34) pad_x = 34;
        if (pad_x + pad_w > GFX_W - 34) pad_x = GFX_W - 34 - pad_w;

        if (k == KEY_OK && !launched) {
            launched = true;
            bsp_audio_sfx(SFX_CONFIRM);
            continue;
        }
        if (!launched) continue;

        /* Advance ball physics */
        bx += vx;
        by += vy;
        if (bx <= 42) { bx = 42; vx = -vx; bsp_audio_sfx(SFX_BOUNCE); }
        if (bx >= GFX_W - 42) { bx = GFX_W - 42; vx = -vx; bsp_audio_sfx(SFX_BOUNCE); }
        if (by <= 66) { by = 66; vy = -vy; bsp_audio_sfx(SFX_BOUNCE); }

        /* Paddle bounce */
        if (vy > 0 && by >= 382 && by <= 412 && bx >= pad_x - 10 && bx <= pad_x + pad_w + 10) {
            by = 380;
            vy = -abs(vy);
            int offset = bx - (pad_x + pad_w / 2);
            vx = (offset / 2) + (offset >= 0 ? 18 : -18);
            bsp_audio_sfx(SFX_BOUNCE);
        }

        /* Brick collision */
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 7; c++) {
                if (!bricks[r][c]) continue;
                int x = 44 + c * 102, y = 70 + r * 36;
                if (bx >= x - 8 && bx <= x + 98 && by >= y - 8 && by <= y + 32) {
                    bricks[r][c] = false;
                    bricks_left--;
                    vy = -vy;
                    score += 15;
                    app_update_high_score(SCORE_BREAKOUT, score);
                    bsp_audio_sfx(SFX_BUBBLE);
                    r = 3; break;
                }
            }
        }

        if (by > 420) {
            lives--;
            launched = false;
            bsp_audio_sfx(SFX_WRONG);
        }
    }

    if (bricks_left == 0) {
        app_add_stars(3);
        bsp_audio_sfx(SFX_FANFARE);
    }
    app_profile_save();
}

/* =========================================================================
 * GAME 6 (7-12岁): 森林密码锁 · 逻辑破译 (Thumbwheel Combination Lock)
 * ========================================================================= */
static void run_game_codebreaker(void)
{
    int secret[3];
    secret[0] = 1 + rand() % 6;
    do { secret[1] = 1 + rand() % 6; } while (secret[1] == secret[0]);
    do { secret[2] = 1 + rand() % 6; } while (secret[2] == secret[0] || secret[2] == secret[1]);

    int guess[3] = { 1, 2, 3 };
    int hist_g[7][3];
    int hist_a[7], hist_b[7];
    int tries = 0;
    int slot = 0;      /* 0..2 = digit dials, 3 = [开锁验证] button */
    bool editing = false;
    bool won = false;
    bool first = true;

    while (1) {
        gfx_clear(GFX_WHITE);
        gfx_draw_top_bar("森林密码锁 · 逻辑推理 (1~6不重复)", g_prof.stars, app_eye_mins_left());

        /* Left Combination Padlock Dials */
        gfx_round_rect(22, 56, 370, 366, 16, 3, GFX_BLACK);
        gfx_draw_text_center(207, 74, editing ? "转动滚轮改数字，按确认锁住" : "选转轮按确认改数字，选开锁验证", FONT_16, GFX_BLACK);

        for (int i = 0; i < 3; i++) {
            int x = 52 + i * 106;
            int y = 118;
            char d[16];
            snprintf(d, sizeof(d), "%d", guess[i]);
            if (slot == i && editing) {
                gfx_fill_round_rect(x, y, 86, 118, 14, GFX_BLACK);
                gfx_draw_text_center(x + 43, y + 10, "▲", FONT_16, GFX_WHITE);
                gfx_draw_text_center(x + 43, y + 44, d, FONT_32, GFX_WHITE);
                gfx_draw_text_center(x + 43, y + 90, "▼", FONT_16, GFX_WHITE);
            } else {
                gfx_round_rect(x, y, 86, 118, 14, (slot == i) ? 4 : 2, GFX_BLACK);
                gfx_draw_text_center(x + 43, y + 44, d, FONT_32, GFX_BLACK);
            }
        }

        if (slot == 3) {
            gfx_fill_round_rect(62, 266, 290, 58, 14, GFX_BLACK);
            gfx_draw_text_center(207, 283, "▶ 按下开锁验证！", FONT_24, GFX_WHITE);
        } else {
            gfx_round_rect(62, 266, 290, 58, 14, 2, GFX_BLACK);
            gfx_draw_text_center(207, 283, "开锁验证", FONT_24, GFX_BLACK);
        }
        gfx_draw_text_center(207, 348, "●=数字和位置全对", FONT_16, GFX_BLACK);
        gfx_draw_text_center(207, 376, "○=数字对但位置错", FONT_16, GFX_BLACK);

        /* Right Deduction History Log */
        gfx_round_rect(408, 56, 370, 366, 16, 2, GFX_BLACK);
        gfx_draw_text_center(593, 72, "推理记录本 (最多7次)", FONT_24, GFX_BLACK);
        for (int t = 0; t < tries; t++) {
            char line[80];
            snprintf(line, sizeof(line), "第%d次:  %d - %d - %d    ●%d  ○%d",
                     t + 1, hist_g[t][0], hist_g[t][1], hist_g[t][2], hist_a[t], hist_b[t]);
            gfx_draw_text(432, 112 + t * 40, line, FONT_24, GFX_BLACK);
        }

        if (won || tries >= 7) {
            char endmsg[96];
            if (won) snprintf(endmsg, sizeof(endmsg), "★ 破译成功！用时%d步，奖励3颗星！", tries);
            else     snprintf(endmsg, sizeof(endmsg), "宝藏密码是 %d-%d-%d，下次一定能破译！", secret[0], secret[1], secret[2]);
            gfx_fill_round_rect(120, 180, 560, 120, 16, GFX_WHITE);
            gfx_round_rect(120, 180, 560, 120, 16, 4, GFX_BLACK);
            gfx_draw_text_center(GFX_W / 2, 212, endmsg, FONT_24, GFX_BLACK);
            gfx_draw_text_center(GFX_W / 2, 258, "按下滚轮返回挑战营", FONT_16, GFX_BLACK);
            bsp_epd_smart_present(gfx_get_fb(), false);
            while (bsp_input_wait(30000) != KEY_OK) {}
            break;
        }

        gfx_draw_bottom_hints(editing ? "滚轮拨动数字1~6" : "滚轮切换槽位",
                              editing ? "锁定该位数字" : "编辑/提交验证", "返回挑战营");
        bsp_epd_smart_present(gfx_get_fb(), first);
        first = false;

        bsp_key_t k = bsp_input_wait(30000);
        if (k == KEY_BACK) {
            if (editing) editing = false;
            else break;
        } else if (k == KEY_PREV) {
            if (editing) guess[slot] = (guess[slot] == 1) ? 6 : (guess[slot] - 1);
            else slot = (slot + 3) % 4;
            bsp_audio_sfx(SFX_WHEEL_TICK);
        } else if (k == KEY_NEXT) {
            if (editing) guess[slot] = (guess[slot] == 6) ? 1 : (guess[slot] + 1);
            else slot = (slot + 1) % 4;
            bsp_audio_sfx(SFX_WHEEL_TICK);
        } else if (k == KEY_OK) {
            if (slot < 3) {
                editing = !editing;
                bsp_audio_sfx(SFX_CONFIRM);
            } else {
                int a = 0, b = 0;
                for (int i = 0; i < 3; i++) {
                    hist_g[tries][i] = guess[i];
                    if (guess[i] == secret[i]) a++;
                    else if (guess[i] == secret[0] || guess[i] == secret[1] || guess[i] == secret[2]) b++;
                }
                hist_a[tries] = a;
                hist_b[tries] = b;
                tries++;
                if (a == 3) {
                    won = true;
                    int sc = (8 - tries) * 20;
                    app_update_high_score(SCORE_CODEBREAKER, sc);
                    app_add_stars(3);
                    app_profile_save();
                    bsp_audio_sfx(SFX_FANFARE);
                } else {
                    bsp_audio_sfx(SFX_CONFIRM);
                }
            }
        }
    }
}

/* =========================================================================
 * GAME S5 (6-12岁): 四子连珠棋 · 智斗猫头鹰 (Connect Four — Pure 1D Column Input!)
 * ========================================================================= */
#define C4_ROWS 6
#define C4_COLS 7

static int c4_drop_row(const uint8_t bd[C4_ROWS][C4_COLS], int col)
{
    if (col < 0 || col >= C4_COLS) return -1;
    for (int r = C4_ROWS - 1; r >= 0; r--) {
        if (bd[r][col] == 0) return r;
    }
    return -1;
}

static bool c4_check_win(const uint8_t bd[C4_ROWS][C4_COLS], uint8_t who, int win_rc[4][2])
{
    static const int dr[4] = { 0, 1, 1, -1 };
    static const int dc[4] = { 1, 0, 1,  1 };
    for (int r = 0; r < C4_ROWS; r++) {
        for (int c = 0; c < C4_COLS; c++) {
            if (bd[r][c] != who) continue;
            for (int d = 0; d < 4; d++) {
                int r3 = r + 3 * dr[d], c3 = c + 3 * dc[d];
                if (r3 < 0 || r3 >= C4_ROWS || c3 < 0 || c3 >= C4_COLS) continue;
                if (bd[r + dr[d]][c + dc[d]] == who &&
                    bd[r + 2 * dr[d]][c + 2 * dc[d]] == who &&
                    bd[r3][c3] == who) {
                    if (win_rc) {
                        for (int k = 0; k < 4; k++) {
                            win_rc[k][0] = r + k * dr[d];
                            win_rc[k][1] = c + k * dc[d];
                        }
                    }
                    return true;
                }
            }
        }
    }
    return false;
}

static int c4_eval_window(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
    int ai = (a == 2) + (b == 2) + (c == 2) + (d == 2);
    int pl = (a == 1) + (b == 1) + (c == 1) + (d == 1);
    if (ai > 0 && pl > 0) return 0;
    if (ai == 3) return 45;
    if (ai == 2) return 10;
    if (pl == 3) return -50;
    if (pl == 2) return -12;
    return 0;
}

static int c4_score_board(const uint8_t bd[C4_ROWS][C4_COLS])
{
    int sc = 0;
    for (int r = 0; r < C4_ROWS; r++) {
        if (bd[r][3] == 2) sc += 6;
        for (int c = 0; c <= C4_COLS - 4; c++)
            sc += c4_eval_window(bd[r][c], bd[r][c+1], bd[r][c+2], bd[r][c+3]);
    }
    for (int c = 0; c < C4_COLS; c++)
        for (int r = 0; r <= C4_ROWS - 4; r++)
            sc += c4_eval_window(bd[r][c], bd[r+1][c], bd[r+2][c], bd[r+3][c]);
    for (int r = 0; r <= C4_ROWS - 4; r++)
        for (int c = 0; c <= C4_COLS - 4; c++) {
            sc += c4_eval_window(bd[r][c], bd[r+1][c+1], bd[r+2][c+2], bd[r+3][c+3]);
            sc += c4_eval_window(bd[r+3][c], bd[r+2][c+1], bd[r+1][c+2], bd[r][c+3]);
        }
    return sc;
}

static int c4_pick_ai_col(uint8_t bd[C4_ROWS][C4_COLS], bool master_ai)
{
    /* 1. Immediate AI win */
    for (int c = 0; c < C4_COLS; c++) {
        int r = c4_drop_row(bd, c);
        if (r < 0) continue;
        bd[r][c] = 2;
        bool w = c4_check_win(bd, 2, NULL);
        bd[r][c] = 0;
        if (w) return c;
    }
    /* 2. Block immediate Player win */
    for (int c = 0; c < C4_COLS; c++) {
        int r = c4_drop_row(bd, c);
        if (r < 0) continue;
        bd[r][c] = 1;
        bool w = c4_check_win(bd, 1, NULL);
        bd[r][c] = 0;
        if (w) return c;
    }

    int best_c = -1, best_sc = -999999;
    for (int c = 0; c < C4_COLS; c++) {
        int r = c4_drop_row(bd, c);
        if (r < 0) continue;
        bd[r][c] = 2;
        int sc = (3 - abs(c - 3)) * 4 + (rand() % (master_ai ? 4 : 18));
        if (master_ai) {
            sc += c4_score_board(bd);
            /* Never gift Player an immediate winning move right above our stone! */
            if (r > 0) {
                bd[r - 1][c] = 1;
                if (c4_check_win(bd, 1, NULL)) sc -= 800;
                bd[r - 1][c] = 0;
            }
            /* Check if Player gets any double-win fork next turn */
            int pl_wins = 0;
            for (int pc = 0; pc < C4_COLS; pc++) {
                int pr = c4_drop_row(bd, pc);
                if (pr < 0) continue;
                bd[pr][pc] = 1;
                if (c4_check_win(bd, 1, NULL)) pl_wins++;
                bd[pr][pc] = 0;
            }
            if (pl_wins >= 2) sc -= 400;
        }
        bd[r][c] = 0;
        if (sc > best_sc) { best_sc = sc; best_c = c; }
    }
    return best_c;
}

static void run_game_connect4(void)
{
    bool master = false;
    {
        int tsel = 0;
        bool f = true;
        while (1) {
            gfx_clear(GFX_WHITE);
            gfx_draw_top_bar("四子连珠棋 · 选择对手", g_prof.stars, app_eye_mins_left());
            const char *tname[2] = { "小狐狸学徒 (轻松博弈 · 适合入门)", "猫头鹰大师 (攻防预判 · 烧脑挑战)" };
            for (int i = 0; i < 2; i++) {
                int y = 135 + i * 105;
                if (i == tsel) {
                    gfx_fill_round_rect(110, y, 580, 76, 16, GFX_BLACK);
                    gfx_draw_text_center(GFX_W / 2, y + 26, tname[i], FONT_24, GFX_WHITE);
                } else {
                    gfx_round_rect(110, y, 580, 76, 16, 3, GFX_BLACK);
                    gfx_draw_text_center(GFX_W / 2, y + 26, tname[i], FONT_24, GFX_BLACK);
                }
            }
            gfx_draw_bottom_hints("滚轮选棋手难度", "按下开局", "返回挑战营");
            bsp_epd_smart_present(gfx_get_fb(), f);
            f = false;
            bsp_key_t k = bsp_input_wait(30000);
            if (k == KEY_BACK) return;
            if (k == KEY_PREV || k == KEY_NEXT) { tsel ^= 1; bsp_audio_sfx(SFX_WHEEL_TICK); }
            else if (k == KEY_OK) { master = (tsel == 1); bsp_audio_sfx(SFX_CONFIRM); break; }
        }
    }

    uint8_t bd[C4_ROWS][C4_COLS];
    memset(bd, 0, sizeof(bd));
    int win_rc[4][2] = {{-1,-1},{-1,-1},{-1,-1},{-1,-1}};
    int sel_col = 3;
    int state = 0; /* 0=playing, 1=player won, 2=AI won, 3=draw */
    int wins = 0, score = 0, stones = 0;
    const char *tip = "横竖斜任意连成 4 颗黑子即获胜！";
    bool first = true;

    while (1) {
        gfx_clear(GFX_WHITE);
        char hdr[96];
        snprintf(hdr, sizeof(hdr), "四子连珠棋   胜局:%d   得分:%d", wins, score);
        gfx_draw_top_bar(hdr, g_prof.stars, app_eye_mins_left());

        /* Left Opponent Card */
        gfx_round_rect(20, 54, 212, 372, 16, 3, GFX_BLACK);
        gfx_draw_art(126, 138, 96, master ? ART_OWL_GUARD : ART_FOX, GFX_BLACK);
        gfx_draw_text_center(126, 200, master ? "猫头鹰大师" : "小狐狸学徒", FONT_24, GFX_BLACK);
        gfx_draw_text_center(126, 238, "● 宝贝执黑先手", FONT_16, GFX_BLACK);
        gfx_draw_text_center(126, 264, "○ 对手执双环白子", FONT_16, GFX_BLACK);
        gfx_draw_paragraph(34, 304, 184, 108, 6, tip, FONT_16, GFX_BLACK, NULL);

        /* Right 7x6 Gravity Board */
        int bx0 = 252, by0 = 96;
        int cell_w = 72, cell_h = 52;

        /* Top Column Drop Indicator */
        int drop_x = bx0 + 10 + sel_col * cell_w;
        gfx_fill_round_rect(drop_x + 4, 52, 64, 38, 10, GFX_BLACK);
        gfx_draw_text_center(drop_x + 36, 61, "▼落子", FONT_16, GFX_WHITE);

        gfx_fill_round_rect(bx0, by0, 524, 330, 16, GFX_GRAY_LIGHT);
        gfx_round_rect(bx0, by0, 524, 330, 16, 3, GFX_BLACK);

        for (int r = 0; r < C4_ROWS; r++) {
            for (int c = 0; c < C4_COLS; c++) {
                int cx = bx0 + 46 + c * cell_w;
                int cy = by0 + 30 + r * cell_h;
                bool is_w = false;
                if (state == 1 || state == 2) {
                    for (int k = 0; k < 4; k++)
                        if (win_rc[k][0] == r && win_rc[k][1] == c) is_w = true;
                }
                if (bd[r][c] == 0) {
                    gfx_fill_circle(cx, cy, 20, GFX_WHITE);
                    gfx_circle(cx, cy, 20, (c == sel_col && state == 0) ? 3 : 2, GFX_BLACK);
                } else if (bd[r][c] == 1) {
                    gfx_fill_circle(cx, cy, 21, GFX_BLACK);
                    if (is_w) gfx_draw_text_center(cx, cy - 12, "★", FONT_24, GFX_WHITE);
                    else gfx_fill_circle(cx - 6, cy - 6, 4, GFX_WHITE);
                } else {
                    gfx_fill_circle(cx, cy, 21, GFX_WHITE);
                    gfx_circle(cx, cy, 21, 4, GFX_BLACK);
                    if (is_w) gfx_draw_text_center(cx, cy - 12, "★", FONT_24, GFX_BLACK);
                    else gfx_circle(cx, cy, 10, 3, GFX_BLACK);
                }
            }
        }

        if (state != 0) {
            gfx_draw_bottom_hints("本局结束", "按下再来一盘", "返回挑战营");
            bsp_epd_smart_present(gfx_get_fb(), first);
            first = false;
            bsp_key_t k = bsp_input_wait(30000);
            if (k == KEY_BACK) break;
            if (k == KEY_OK) {
                memset(bd, 0, sizeof(bd));
                stones = 0;
                state = 0;
                sel_col = 3;
                tip = "新棋局开始！选好列按下落子吧！";
                bsp_audio_sfx(SFX_CONFIRM);
            }
            continue;
        }

        gfx_draw_bottom_hints("滚轮左右选列(1~7)", "按下投入黑棋", "返回挑战营");
        bsp_epd_smart_present(gfx_get_fb(), first);
        first = false;

        bsp_key_t k = bsp_input_wait(30000);
        if (k == KEY_BACK) break;
        else if (k == KEY_PREV) { sel_col = (sel_col + 6) % C4_COLS; bsp_audio_sfx(SFX_WHEEL_TICK); }
        else if (k == KEY_NEXT) { sel_col = (sel_col + 1) % C4_COLS; bsp_audio_sfx(SFX_WHEEL_TICK); }
        else if (k == KEY_OK) {
            int r = c4_drop_row(bd, sel_col);
            if (r < 0) {
                bsp_audio_sfx(SFX_WRONG);
                tip = "这一列已经满啦，拨轮换旁边一列吧！";
                continue;
            }
            bd[r][sel_col] = 1;
            stones++;
            bsp_audio_sfx(SFX_BOUNCE);

            if (c4_check_win(bd, 1, win_rc)) {
                state = 1;
                wins++;
                score += (master ? 160 : 100) + wins * 20;
                app_update_high_score(SCORE_CONNECT4, score);
                app_add_stars(master ? 3 : 2);
                app_profile_save();
                bsp_audio_sfx(SFX_FANFARE);
                tip = "★ 四子连珠获胜！太有大将之风啦！按确认再来一局！";
                continue;
            }
            if (stones >= C4_ROWS * C4_COLS) {
                state = 3;
                score += 40;
                app_update_high_score(SCORE_CONNECT4, score);
                tip = "棋逢对手平局啦！按下滚轮再来一盘！";
                continue;
            }

            /* AI Reply */
            int ac = c4_pick_ai_col(bd, master);
            int ar = c4_drop_row(bd, ac);
            if (ar >= 0) {
                bd[ar][ac] = 2;
                stones++;
                if (c4_check_win(bd, 2, win_rc)) {
                    state = 2;
                    bsp_audio_sfx(SFX_WRONG);
                    tip = "哎呀被对手连成四颗啦！按下滚轮复仇一局！";
                } else if (stones >= C4_ROWS * C4_COLS) {
                    state = 3;
                    tip = "满盘平局！按下滚轮再来一局！";
                } else {
                    char buf[80];
                    static char s_ai_tip[80];
                    snprintf(buf, sizeof(buf), "对手落在了第 %d 列，轮到宝贝出招！", ac + 1);
                    memcpy(s_ai_tip, buf, sizeof(buf));
                    tip = s_ai_tip;
                }
            }
        }
    }
    app_profile_save();
}

/* =========================================================================
 * GAME S6 (6-12岁): 成语接龙轮 · 国学摩天轮 (Idiom Wheel + Mandarin Voice!)
 * ========================================================================= */
typedef struct {
    const char *c[4];
    uint8_t     blank;
    const char *wrong[5];
    const char *meaning;
} idiom_item_t;

static const idiom_item_t k_idioms[] = {
    { {"守","株","待","兔"}, 3, {"猫","虎","鹿","马","鸟"}, "妄想不劳而获，死守树桩等兔子撞上来" },
    { {"兔","飞","猛","进"}, 1, {"跑","跳","奔","走","跃"}, "谐音突飞猛进，形容进步像小兔飞奔一样快" },
    { {"画","龙","点","睛"}, 3, {"眼","尾","角","鳞","爪"}, "关键处加上神来一笔，让作品立刻活灵活现" },
    { {"胸","有","成","竹"}, 3, {"木","树","花","林","松"}, "做事之前心里早已有全面成熟的把握" },
    { {"水","滴","石","穿"}, 3, {"破","碎","开","裂","断"}, "水珠不断滴落也能穿透石头，比喻坚持不懈" },
    { {"闻","鸡","起","舞"}, 3, {"跳","唱","跑","飞","歌"}, "听到鸡叫就起床练本领，形容勤奋刻苦" },
    { {"雪","中","送","炭"}, 3, {"火","衣","柴","被","暖"}, "在下雪天给人送炭取暖，比喻急人所难" },
    { {"愚","公","移","山"}, 3, {"石","海","林","河","峰"}, "不怕困难咬定目标，挖山不止终能成功" },
    { {"狐","假","虎","威"}, 2, {"狮","狼","熊","豹","象"}, "小狐狸借着大老虎的威风吓唬百兽" },
    { {"井","底","之","蛙"}, 3, {"鱼","龟","虾","蟹","虫"}, "待在井底看天空很小，提醒我们要开阔眼界" },
    { {"亡","羊","补","牢"}, 3, {"圈","门","墙","洞","网"}, "丢了羊赶紧修补羊圈，出了错及时补救还不晚" },
    { {"马","到","成","功"}, 3, {"利","胜","名","就","果"}, "战马一到便取得胜利，祝愿事情顺利成功" },
    { {"温","故","知","新"}, 3, {"今","多","识","理","道"}, "复习学过的旧知识，从中领悟出新的体会" },
    { {"乘","风","破","浪"}, 3, {"水","海","潮","冰","云"}, "船只借着大风劈开巨浪，比喻勇往直前" },
    { {"铁","杵","成","针"}, 3, {"线","钉","刀","剑","丝"}, "粗铁棒只要天天磨也能磨成绣花针" },
    { {"聚","沙","成","塔"}, 3, {"山","城","墙","堡","楼"}, "一粒粒细沙积少成多，也能堆成高高的宝塔" },
    { {"盲","人","摸","象"}, 3, {"马","牛","虎","墙","柱"}, "只摸到一部分就下结论，看问题要全面哟" },
    { {"精","卫","填","海"}, 3, {"河","湖","山","谷","池"}, "小鸟衔木石填大海，赞美不畏艰难的毅力" },
    { {"锦","上","添","花"}, 3, {"金","彩","星","玉","光"}, "在美丽织锦上再绣花朵，比喻好上加好" },
    { {"一","举","两","得"}, 3, {"胜","全","成","获","赢"}, "做一件事同时收获两份好成果" },
};
#define IDIOM_COUNT ((int)(sizeof(k_idioms) / sizeof(k_idioms[0])))

static uint16_t utf8_single_cp(const char *s)
{
    const uint8_t *u = (const uint8_t *)s;
    if (!u || !u[0]) return 0;
    if (u[0] < 0x80) return u[0];
    if ((u[0] & 0xF0) == 0xE0 && u[1] && u[2]) {
        return (uint16_t)(((u[0] & 0x0F) << 12) | ((u[1] & 0x3F) << 6) | (u[2] & 0x3F));
    }
    return 0;
}

static void prepare_idiom_opts(const idiom_item_t *it, const char *opts[6], int *ans_slot)
{
    opts[0] = it->c[it->blank];
    for (int i = 0; i < 5; i++) opts[i + 1] = it->wrong[i];
    for (int i = 5; i > 0; i--) {
        int j = rand() % (i + 1);
        const char *t = opts[i]; opts[i] = opts[j]; opts[j] = t;
    }
    *ans_slot = 0;
    for (int i = 0; i < 6; i++) {
        if (strcmp(opts[i], it->c[it->blank]) == 0) *ans_slot = i;
    }
}

static void run_game_idiom_wheel(void)
{
    int order[IDIOM_COUNT];
    for (int i = 0; i < IDIOM_COUNT; i++) order[i] = i;
    for (int i = IDIOM_COUNT - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int t = order[i]; order[i] = order[j]; order[j] = t;
    }

    int q_pos = 0, sel = 0, ans_slot = 0;
    int score = 0, streak = 0;
    const char *opts[6];
    prepare_idiom_opts(&k_idioms[order[q_pos]], opts, &ans_slot);
    const char *feedback = "拨轮旋转底部汉字摩天轮，选出正确汉字补全成语（自带真人读音）！";
    bool first = true;

    while (1) {
        const idiom_item_t *it = &k_idioms[order[q_pos]];

        gfx_clear(GFX_WHITE);
        char hdr[96];
        snprintf(hdr, sizeof(hdr), "成语接龙轮   第%d题   连对:%d   得分:%d", q_pos + 1, streak, score);
        gfx_draw_top_bar(hdr, g_prof.stars, app_eye_mins_left());

        /* Top Scroll Card with 4 Calligraphy Tiles */
        gfx_round_rect(26, 52, GFX_W - 52, 168, 16, 3, GFX_BLACK);
        for (int i = 0; i < 4; i++) {
            int tx = 180 + i * 116;
            int ty = 68;
            if (i == it->blank) {
                gfx_fill_round_rect(tx, ty, 84, 84, 14, GFX_GRAY_LIGHT);
                gfx_round_rect(tx, ty, 84, 84, 14, 4, GFX_BLACK);
                gfx_draw_text_center(tx + 42, ty + 26, "？", FONT_32, GFX_BLACK);
            } else {
                gfx_round_rect(tx, ty, 84, 84, 14, 2, GFX_BLACK);
                gfx_draw_text_center(tx + 42, ty + 26, it->c[i], FONT_32, GFX_BLACK);
            }
        }
        char hint_line[160];
        snprintf(hint_line, sizeof(hint_line), "释义提示：%s", it->meaning);
        gfx_draw_text_center(GFX_W / 2, 174, hint_line, FONT_16, GFX_BLACK);

        /* 6-Spoke Candidate Hanzi Wheel + Live Pinyin */
        for (int i = 0; i < 6; i++) {
            int wx = 34 + i * 124;
            int wy = 244;
            uint16_t cp = utf8_single_cp(opts[i]);
            char py[24] = "";
            gfx_hanzi_lookup(cp, py, sizeof(py), NULL, NULL);

            if (i == sel) {
                gfx_fill_round_rect(wx, wy - 10, 110, 128, 16, GFX_BLACK);
                gfx_draw_text_center(wx + 55, wy + 2, py, FONT_16, GFX_WHITE);
                gfx_draw_text_center(wx + 55, wy + 36, opts[i], FONT_32, GFX_WHITE);
                gfx_draw_text_center(wx + 55, wy + 84, "▲填入", FONT_16, GFX_WHITE);
            } else {
                gfx_round_rect(wx, wy, 110, 108, 14, 2, GFX_BLACK);
                gfx_draw_text_center(wx + 55, wy + 12, py, FONT_16, GFX_BLACK);
                gfx_draw_text_center(wx + 55, wy + 46, opts[i], FONT_32, GFX_BLACK);
            }
        }

        gfx_round_rect(26, 382, GFX_W - 52, 46, 10, 2, GFX_BLACK);
        gfx_draw_text_center(GFX_W / 2, 396, feedback, FONT_16, GFX_BLACK);
        gfx_draw_bottom_hints("滚轮旋转选字(带拼音)", "按下填入并听读音", "返回挑战营");
        bsp_epd_smart_present(gfx_get_fb(), first);
        first = false;

        bsp_key_t k = bsp_input_wait(30000);
        if (k == KEY_BACK) break;
        else if (k == KEY_PREV) { sel = (sel + 5) % 6; bsp_audio_sfx(SFX_WHEEL_TICK); }
        else if (k == KEY_NEXT) { sel = (sel + 1) % 6; bsp_audio_sfx(SFX_WHEEL_TICK); }
        else if (k == KEY_OK) {
            if (sel == ans_slot) {
                uint16_t cp = utf8_single_cp(opts[sel]);
                uint16_t syl_id = 0xFFFF;
                if (gfx_hanzi_lookup(cp, NULL, 0, NULL, &syl_id) && syl_id != 0xFFFF) {
                    bsp_audio_speak_syllables(syl_id, 0xFFFF, 0xFFFF);
                } else {
                    bsp_audio_sfx(SFX_SUCCESS);
                }
                streak++;
                score += 15 + streak * 3;
                app_update_high_score(SCORE_IDIOM_WHEEL, score);
                if (streak % 4 == 0) app_add_stars(1);

                static char s_ok_msg[128];
                snprintf(s_ok_msg, sizeof(s_ok_msg), "★ 妙笔生花！【%s%s%s%s】答对啦，继续下一关！",
                         it->c[0], it->c[1], it->c[2], it->c[3]);
                feedback = s_ok_msg;

                q_pos = (q_pos + 1) % IDIOM_COUNT;
                prepare_idiom_opts(&k_idioms[order[q_pos]], opts, &ans_slot);
                sel = 0;
            } else {
                streak = 0;
                bsp_audio_sfx(SFX_WRONG);
                feedback = "这个字意思不太对哟，结合提示再转转摩天轮！";
            }
        }
    }
    app_profile_save();
}

/* =========================================================================
 * GAME S7 (6-12岁): 环轨防卫战 · 360°激光炮 (12-Sector Orbital Turret Defense)
 * ========================================================================= */
static void run_game_orbit_defender(void)
{
    /* 12 clock-face unit vectors * 100 (0 = 12 o'clock, clockwise 30 deg per click!) */
    static const int k_ux[12] = {   0,  50,  87, 100,  87,  50,   0, -50, -87, -100, -87, -50 };
    static const int k_uy[12] = { -100, -87, -50,   0,  50,  87, 100,  87,  50,    0, -50, -87 };
    static const int k_rad[5] = { 0, 68, 102, 136, 170 };

    uint8_t met[12][5]; /* met[sec][dist 1..4]: 0=none, 1=meteor */
    memset(met, 0, sizeof(met));
    met[0][4] = 1;
    met[5][4] = 1;

    int turret = 0;
    int laser_sec = -1;
    int shields = 3;
    int score = 0;
    int wave = 1;
    bool first = true;

    const int64_t beat_us = 950000LL;
    int64_t next_beat = esp_timer_get_time() + beat_us;

    while (shields > 0) {
        gfx_clear(GFX_WHITE);
        char hdr[96];
        snprintf(hdr, sizeof(hdr), "环轨防卫战   波次:%d   得分:%d   护盾:%s",
                 wave, score, shields == 3 ? "♥♥♥" : (shields == 2 ? "♥♥" : "♥"));
        gfx_draw_top_bar(hdr, g_prof.stars, app_eye_mins_left());

        /* Left & Right Tactical Radar Panels */
        gfx_round_rect(18, 56, 176, 366, 16, 2, GFX_BLACK);
        gfx_draw_art(106, 134, 96, ART_PLANET_TURRET, GFX_BLACK);
        gfx_draw_text_center(106, 204, "墨芽号空间站", FONT_24, GFX_BLACK);
        char ang_s[48];
        snprintf(ang_s, sizeof(ang_s), "炮口: %d 点钟", turret == 0 ? 12 : turret);
        gfx_draw_text_center(106, 248, ang_s, FONT_24, GFX_BLACK);
        gfx_draw_paragraph(32, 294, 148, 112, 6,
                           "顺/逆时针拨轮旋转激光炮，对准陨石轨道按下发射！",
                           FONT_16, GFX_BLACK, NULL);

        gfx_round_rect(606, 56, 176, 366, 16, 2, GFX_BLACK);
        gfx_draw_text_center(694, 80, "星际防卫勋章", FONT_24, GFX_BLACK);
        char bst[48];
        snprintf(bst, sizeof(bst), "最高:%d分", app_get_high_score(SCORE_ORBIT_DEFEND));
        gfx_draw_text_center(694, 128, bst, FONT_24, GFX_BLACK);
        int active_m = 0;
        for (int s = 0; s < 12; s++)
            for (int d = 1; d <= 4; d++)
                if (met[s][d]) active_m++;
        char mcnt[48];
        snprintf(mcnt, sizeof(mcnt), "来袭陨石: %d", active_m);
        gfx_draw_text_center(694, 180, mcnt, FONT_24, GFX_BLACK);
        gfx_draw_paragraph(620, 238, 148, 160, 8,
                           "每击毁 5 颗陨石升级一波，满 100 分奖智慧星 ★+1！",
                           FONT_16, GFX_BLACK, NULL);

        /* Center 360-Degree Radar Arena (cx=400, cy=240) */
        const int cx = 400, cy = 240;
        for (int d = 1; d <= 4; d++) {
            gfx_circle(cx, cy, k_rad[d], (d == 4) ? 2 : 1, (d == 4) ? GFX_BLACK : GFX_GRAY_MID);
        }
        /* 12 Clock Tick Marks on Outer Ring */
        for (int s = 0; s < 12; s++) {
            int tx = cx + (k_ux[s] * 176) / 100;
            int ty = cy + (k_uy[s] * 176) / 100;
            gfx_fill_circle(tx, ty, (s == turret) ? 5 : 2, GFX_BLACK);
        }

        /* Dotted Aim Line or Active Laser Beam */
        if (laser_sec >= 0) {
            int lx = cx + (k_ux[laser_sec] * 176) / 100;
            int ly = cy + (k_uy[laser_sec] * 176) / 100;
            gfx_line(cx, cy, lx, ly, 5, GFX_BLACK);
        } else {
            for (int r = 52; r <= 172; r += 18) {
                int ax = cx + (k_ux[turret] * r) / 100;
                int ay = cy + (k_uy[turret] * r) / 100;
                gfx_fill_circle(ax, ay, 2, GFX_BLACK);
            }
        }

        /* Core Planet + Turret Barrel */
        int bx = cx + (k_ux[turret] * 46) / 100;
        int by = cy + (k_uy[turret] * 46) / 100;
        gfx_line(cx, cy, bx, by, 7, GFX_BLACK);
        gfx_fill_circle(cx, cy, 28, GFX_WHITE);
        gfx_circle(cx, cy, 28, 4, GFX_BLACK);
        gfx_draw_text_center(cx, cy - 12, "★", FONT_24, GFX_BLACK);

        /* Incoming Meteors */
        for (int s = 0; s < 12; s++) {
            for (int d = 1; d <= 4; d++) {
                if (!met[s][d]) continue;
                int mx = cx + (k_ux[s] * k_rad[d]) / 100;
                int my = cy + (k_uy[s] * k_rad[d]) / 100;
                gfx_fill_circle(mx, my, 12, GFX_BLACK);
                gfx_fill_circle(mx - 3, my - 3, 3, GFX_WHITE);
            }
        }

        gfx_draw_bottom_hints("滚轮360°旋转炮台", "按下发射激光击碎陨石", "返回挑战营");
        bsp_epd_smart_present(gfx_get_fb(), first);
        first = false;
        laser_sec = -1;

        int64_t now = esp_timer_get_time();
        int wait_ms = (int)((next_beat - now) / 1000LL);
        if (wait_ms < 20) wait_ms = 20;

        bsp_key_t k = bsp_input_wait(wait_ms);
        if (k == KEY_BACK) break;
        if (k == KEY_PREV) {
            turret = (turret + 11) % 12;
            bsp_audio_sfx(SFX_WHEEL_TICK);
        } else if (k == KEY_NEXT) {
            turret = (turret + 1) % 12;
            bsp_audio_sfx(SFX_WHEEL_TICK);
        } else if (k == KEY_OK) {
            laser_sec = turret;
            bool hit = false;
            for (int d = 1; d <= 4; d++) {
                if (met[turret][d]) {
                    met[turret][d] = 0;
                    hit = true;
                    score += 15;
                    wave = 1 + score / 75;
                    app_update_high_score(SCORE_ORBIT_DEFEND, score);
                    if (score % 105 == 0) app_add_stars(1);
                    bsp_audio_sfx(SFX_BUBBLE);
                    break;
                }
            }
            if (!hit) bsp_audio_sfx(SFX_BOUNCE);
        }

        if (esp_timer_get_time() >= next_beat) {
            int64_t pace = beat_us - (int64_t)(wave - 1) * 45000LL;
            if (pace < 520000LL) pace = 520000LL;
            next_beat = esp_timer_get_time() + pace;

            /* Advance meteors 1 ring closer to planet */
            for (int s = 0; s < 12; s++) {
                if (met[s][1]) {
                    met[s][1] = 0;
                    shields--;
                    bsp_audio_sfx(SFX_WRONG);
                }
                for (int d = 1; d < 4; d++) met[s][d] = met[s][d + 1];
                met[s][4] = 0;
            }
            /* Spawn new meteor on outer ring */
            if ((rand() % 100) < (52 + wave * 4)) {
                met[rand() % 12][4] = 1;
            }
        }
    }

    if (shields <= 0) {
        gfx_fill_round_rect(175, 155, 450, 170, 18, GFX_WHITE);
        gfx_round_rect(175, 155, 450, 170, 18, 4, GFX_BLACK);
        gfx_draw_text_center(GFX_W / 2, 188, "空间站护盾耗尽，返航休整！", FONT_24, GFX_BLACK);
        char res[80];
        snprintf(res, sizeof(res), "坚守至第 %d 波 · 总得分：%d 分", wave, score);
        gfx_draw_text_center(GFX_W / 2, 236, res, FONT_24, GFX_BLACK);
        gfx_draw_text_center(GFX_W / 2, 282, "按下滚轮返回益智挑战营", FONT_16, GFX_BLACK);
        bsp_epd_smart_present(gfx_get_fb(), false);
        while (bsp_input_wait(30000) != KEY_OK) {}
    }
    app_profile_save();
}

/* =========================================================================
 * GAME S8 (6-12岁): 数字华容道 · 智能滑块阵 (Smart-Neighbor 3x3 / 4x4 Klotski)
 * Wheel cycles ONLY the 2~4 movable tiles adjacent to empty slot!
 * ========================================================================= */
static void shuffle_sliding_board(int n, uint8_t b[4][4], int *zr, int *zc)
{
    int v = 1;
    for (int r = 0; r < n; r++)
        for (int c = 0; c < n; c++)
            b[r][c] = (uint8_t)((r == n - 1 && c == n - 1) ? 0 : v++);
    *zr = n - 1;
    *zc = n - 1;

    static const int dr[4] = { -1, 0, 1, 0 };
    static const int dc[4] = { 0, 1, 0, -1 };
    int prev_r = -1, prev_c = -1;
    int steps = (n == 3) ? 42 : 76;
    for (int s = 0; s < steps; s++) {
        int cand[4][2], cnt = 0;
        for (int d = 0; d < 4; d++) {
            int nr = *zr + dr[d], nc = *zc + dc[d];
            if (nr >= 0 && nr < n && nc >= 0 && nc < n && !(nr == prev_r && nc == prev_c)) {
                cand[cnt][0] = nr;
                cand[cnt][1] = nc;
                cnt++;
            }
        }
        int pick = rand() % cnt;
        prev_r = *zr; prev_c = *zc;
        int nr = cand[pick][0], nc = cand[pick][1];
        b[*zr][*zc] = b[nr][nc];
        b[nr][nc] = 0;
        *zr = nr; *zc = nc;
    }
}

static bool sliding_is_solved(int n, const uint8_t b[4][4])
{
    int v = 1;
    for (int r = 0; r < n; r++) {
        for (int c = 0; c < n; c++) {
            uint8_t expect = (uint8_t)((r == n - 1 && c == n - 1) ? 0 : v++);
            if (b[r][c] != expect) return false;
        }
    }
    return true;
}

static void run_game_sliding_puzzle(void)
{
    int n = 3;
    {
        int tsel = 0;
        bool f = true;
        while (1) {
            gfx_clear(GFX_WHITE);
            gfx_draw_top_bar("数字华容道 · 选择棋盘规模", g_prof.stars, app_eye_mins_left());
            const char *tname[2] = { "3×3 经典八数码 (1~8归位 · 推荐入门)", "4×4 大师十五数码 (1~15归位 · 空间挑战)" };
            for (int i = 0; i < 2; i++) {
                int y = 135 + i * 105;
                if (i == tsel) {
                    gfx_fill_round_rect(100, y, 600, 76, 16, GFX_BLACK);
                    gfx_draw_text_center(GFX_W / 2, y + 26, tname[i], FONT_24, GFX_WHITE);
                } else {
                    gfx_round_rect(100, y, 600, 76, 16, 3, GFX_BLACK);
                    gfx_draw_text_center(GFX_W / 2, y + 26, tname[i], FONT_24, GFX_BLACK);
                }
            }
            gfx_draw_bottom_hints("滚轮选盘面大小", "按下开始解谜", "返回挑战营");
            bsp_epd_smart_present(gfx_get_fb(), f);
            f = false;
            bsp_key_t k = bsp_input_wait(30000);
            if (k == KEY_BACK) return;
            if (k == KEY_PREV || k == KEY_NEXT) { tsel ^= 1; bsp_audio_sfx(SFX_WHEEL_TICK); }
            else if (k == KEY_OK) { n = (tsel == 0) ? 3 : 4; bsp_audio_sfx(SFX_CONFIRM); break; }
        }
    }

    uint8_t b[4][4];
    int zr = 0, zc = 0;
    shuffle_sliding_board(n, b, &zr, &zc);
    int moves = 0;
    int sel_idx = 0;
    bool first = true;

    static const int dr[4] = { -1, 0, 1, 0 };
    static const int dc[4] = { 0, 1, 0, -1 };
    static const char *k_dir_arrow[4] = { "↓", "←", "↑", "→" }; /* Arrow from neighbor into empty cell! */

    while (1) {
        int nb_r[4], nb_c[4];
        const char *nb_arr[4];
        int nb_cnt = 0;
        for (int d = 0; d < 4; d++) {
            int nr = zr + dr[d], nc = zc + dc[d];
            if (nr >= 0 && nr < n && nc >= 0 && nc < n) {
                nb_r[nb_cnt] = nr;
                nb_c[nb_cnt] = nc;
                nb_arr[nb_cnt] = k_dir_arrow[d];
                nb_cnt++;
            }
        }
        int total_slots = nb_cnt + 1; /* +1 for [重新洗牌] button */
        if (sel_idx >= total_slots) sel_idx = 0;

        gfx_clear(GFX_WHITE);
        char hdr[96];
        snprintf(hdr, sizeof(hdr), "数字华容道 (%d×%d)   步数:%d   最高:%d分",
                 n, n, moves, app_get_high_score(SCORE_SLIDE_PUZZLE));
        gfx_draw_top_bar(hdr, g_prof.stars, app_eye_mins_left());

        /* Left Puzzle Frame */
        int board_px = 352;
        int bx0 = 56, by0 = 62;
        gfx_fill_round_rect(bx0, by0, board_px, board_px, 18, GFX_GRAY_LIGHT);
        gfx_round_rect(bx0, by0, board_px, board_px, 18, 4, GFX_BLACK);

        int gap = 10;
        int tile = (board_px - gap * (n + 1)) / n;

        for (int r = 0; r < n; r++) {
            for (int c = 0; c < n; c++) {
                int tx = bx0 + gap + c * (tile + gap);
                int ty = by0 + gap + r * (tile + gap);
                uint8_t val = b[r][c];
                if (val == 0) {
                    gfx_round_rect(tx + 4, ty + 4, tile - 8, tile - 8, 12, 2, GFX_GRAY_MID);
                    continue;
                }
                int nb_match = -1;
                for (int k = 0; k < nb_cnt; k++)
                    if (nb_r[k] == r && nb_c[k] == c) nb_match = k;

                char ns[24];
                if (nb_match >= 0 && sel_idx == nb_match) {
                    snprintf(ns, sizeof(ns), "%u%s", (unsigned)val, nb_arr[nb_match]);
                    gfx_fill_round_rect(tx, ty, tile, tile, 14, GFX_BLACK);
                    gfx_draw_text_center(tx + tile / 2, ty + tile / 2 - 16, ns, FONT_32, GFX_WHITE);
                } else {
                    snprintf(ns, sizeof(ns), "%u", (unsigned)val);
                    gfx_fill_round_rect(tx, ty, tile, tile, 14, GFX_WHITE);
                    gfx_round_rect(tx, ty, tile, tile, 14, (nb_match >= 0) ? 4 : 2, GFX_BLACK);
                    gfx_draw_text_center(tx + tile / 2, ty + tile / 2 - 16, ns, FONT_32, GFX_BLACK);
                }
            }
        }

        /* Right Guide & Reshuffle Panel */
        gfx_round_rect(438, 62, 316, 352, 18, 2, GFX_BLACK);
        gfx_draw_art(596, 134, 96, ART_SLIDE_PUZZLE, GFX_BLACK);
        gfx_draw_text_center(596, 198, "智能滚轮滑块操控", FONT_24, GFX_BLACK);
        gfx_draw_paragraph(460, 236, 272, 92, 6,
                           n == 3 ? "按从左到右、从上到下排成 1~8（右下角留空）即可通关！"
                                  : "按顺序排成 1~15（右下角留空）即可通关！滚轮只会在可滑入空格的数字间切换。",
                           FONT_16, GFX_BLACK, NULL);

        if (sel_idx == nb_cnt) {
            gfx_fill_round_rect(476, 342, 240, 52, 12, GFX_BLACK);
            gfx_draw_text_center(596, 356, "↺ 按下重新洗牌", FONT_24, GFX_WHITE);
        } else {
            gfx_round_rect(476, 342, 240, 52, 12, 2, GFX_BLACK);
            gfx_draw_text_center(596, 356, "↺ 重新洗牌", FONT_24, GFX_BLACK);
        }

        if (sliding_is_solved(n, b)) {
            int base_sc = (n == 3) ? 140 : 260;
            int sc = base_sc - moves;
            if (sc < 50) sc = 50;
            app_update_high_score(SCORE_SLIDE_PUZZLE, sc);
            app_add_stars(n == 3 ? 2 : 4);
            app_profile_save();
            bsp_audio_sfx(SFX_FANFARE);

            gfx_fill_round_rect(160, 155, 480, 175, 18, GFX_WHITE);
            gfx_round_rect(160, 155, 480, 175, 18, 4, GFX_BLACK);
            gfx_draw_text_center(GFX_W / 2, 188, "★ 华容道归位成功！最强大脑 ★", FONT_24, GFX_BLACK);
            char res[96];
            snprintf(res, sizeof(res), "共移动 %d 步 · 评分 %d 分 · 奖励 %d 颗星",
                     moves, sc, n == 3 ? 2 : 4);
            gfx_draw_text_center(GFX_W / 2, 238, res, FONT_16, GFX_BLACK);
            gfx_draw_text_center(GFX_W / 2, 282, "按下滚轮返回益智挑战营", FONT_16, GFX_BLACK);
            bsp_epd_smart_present(gfx_get_fb(), false);
            while (bsp_input_wait(30000) != KEY_OK) {}
            break;
        }

        gfx_draw_bottom_hints("滚轮切换可移动滑块", sel_idx == nb_cnt ? "重新打乱" : "按下滑入空格", "返回挑战营");
        bsp_epd_smart_present(gfx_get_fb(), first);
        first = false;

        bsp_key_t k = bsp_input_wait(30000);
        if (k == KEY_BACK) break;
        else if (k == KEY_PREV) { sel_idx = (sel_idx + total_slots - 1) % total_slots; bsp_audio_sfx(SFX_WHEEL_TICK); }
        else if (k == KEY_NEXT) { sel_idx = (sel_idx + 1) % total_slots; bsp_audio_sfx(SFX_WHEEL_TICK); }
        else if (k == KEY_OK) {
            if (sel_idx == nb_cnt) {
                shuffle_sliding_board(n, b, &zr, &zc);
                moves = 0;
                sel_idx = 0;
                bsp_audio_sfx(SFX_CONFIRM);
            } else {
                int nr = nb_r[sel_idx], nc = nb_c[sel_idx];
                b[zr][zc] = b[nr][nc];
                b[nr][nc] = 0;
                zr = nr; zc = nc;
                moves++;
                sel_idx = 0;
                bsp_audio_sfx(SFX_PAGE_TURN);
            }
        }
    }
}

/* =========================================================================
 * SCHOOL CHALLENGE ARENA HUB (6-12岁 · 8 Thumbwheel Games in 4x2 Grid)
 * ========================================================================= */
void app_school_arena_run(void)
{
    static const struct {
        const char    *name;
        const char    *tag;
        gfx_art_id_t   art;
        app_score_id_t sid;
    } k_games[8] = {
        { "数字摩天轮", "口算转盘闯关", ART_FERRIS_WHEEL,  SCORE_MATH_TOWER   },
        { "翻转记忆牌", "瞬时空间记忆", ART_CARDS_MAGIC,   SCORE_MEMORY_FLIP  },
        { "滚轮弹球台", "旋钮挡板弹球", ART_BREAKOUT,      SCORE_BREAKOUT     },
        { "森林密码锁", "侦探数字推理", ART_LOCK_TREASURE, SCORE_CODEBREAKER  },
        { "四子连珠棋", "重力四子博弈", ART_CONNECT4,      SCORE_CONNECT4     },
        { "成语接龙轮", "国学点读接龙", ART_IDIOM_SCROLL,  SCORE_IDIOM_WHEEL  },
        { "环轨防卫战", "圆周激光守卫", ART_PLANET_TURRET, SCORE_ORBIT_DEFEND },
        { "数字华容道", "智能滑块解谜", ART_SLIDE_PUZZLE,  SCORE_SLIDE_PUZZLE },
    };

    int sel = 0;
    bool first = true;

    while (1) {
        gfx_clear(GFX_WHITE);
        gfx_draw_top_bar("★ 益智挑战营 (6-12岁 · 8款滚轮策略与脑力挑战)", g_prof.stars, app_eye_mins_left());

        for (int i = 0; i < 8; i++) {
            int col = i % 4;
            int row = i / 4;
            int x = 18 + col * 194;
            int y = 52 + row * 190;
            bool focus = (i == sel);

            if (focus) {
                gfx_round_rect(x - 2, y - 2, 186, 182, 16, 5, GFX_BLACK);
                gfx_fill_round_rect(x + 8, y + 84, 166, 34, 10, GFX_BLACK);
                gfx_draw_art(x + 91, y + 44, 64, k_games[i].art, GFX_BLACK);
                gfx_draw_text_center(x + 91, y + 89, k_games[i].name, FONT_24, GFX_WHITE);
            } else {
                gfx_round_rect(x, y, 182, 178, 14, 2, GFX_BLACK);
                gfx_draw_art(x + 91, y + 44, 64, k_games[i].art, GFX_BLACK);
                gfx_draw_text_center(x + 91, y + 89, k_games[i].name, FONT_24, GFX_BLACK);
            }
            gfx_draw_text_center(x + 91, y + 126, k_games[i].tag, FONT_16, GFX_BLACK);
            char best[40];
            snprintf(best, sizeof(best), "纪录:%d分", app_get_high_score(k_games[i].sid));
            gfx_draw_text_center(x + 91, y + 150, best, FONT_16, GFX_BLACK);
        }

        gfx_draw_bottom_hints("滚轮挑挑战项目(1~8)", "按下开始挑战", "返回主页");
        bsp_epd_smart_present(gfx_get_fb(), first);
        first = false;

        bsp_key_t k = bsp_input_wait(30000);
        if (k == KEY_BACK) break;
        else if (k == KEY_PREV) { sel = (sel + 7) % 8; bsp_audio_sfx(SFX_WHEEL_TICK); }
        else if (k == KEY_NEXT) { sel = (sel + 1) % 8; bsp_audio_sfx(SFX_WHEEL_TICK); }
        else if (k == KEY_OK) {
            bsp_audio_sfx(SFX_CONFIRM);
            switch (sel) {
            case 0: run_game_math_tower();      break;
            case 1: run_game_memory_flip();     break;
            case 2: run_game_breakout();        break;
            case 3: run_game_codebreaker();     break;
            case 4: run_game_connect4();        break;
            case 5: run_game_idiom_wheel();     break;
            case 6: run_game_orbit_defender();  break;
            case 7: run_game_sliding_puzzle();  break;
            }
            first = true;
        }
        app_check_eye_guard();
    }
}
