#include "app_common.h"

#include <stdio.h>
#include <string.h>

void app_trophy_wall_run(void)
{
    static const struct {
        const char    *label;
        app_score_id_t id;
    } k_rows[14] = {
        { "1. 滚轮配配乐 (启蒙)", SCORE_SAFARI_MATCH  },
        { "2. 海底小潜艇 (反应)", SCORE_SUBMARINE     },
        { "3. 森林八音盒 (乐感)", SCORE_XYLOPHONE     },
        { "4. 小刺猬接果 (手眼)", SCORE_HEDGEHOG      },
        { "5. 规律小火车 (观察)", SCORE_PATTERN_TRAIN },
        { "6. 熊猫叠蛋糕 (平衡)", SCORE_PANDA_STACK   },
        { "7. 数字摩天轮 (算术)", SCORE_MATH_TOWER    },
        { "8. 翻转记忆牌 (记忆)", SCORE_MEMORY_FLIP   },
        { "9. 滚轮弹球台 (操控)", SCORE_BREAKOUT      },
        { "10. 森林密码锁 (推理)", SCORE_CODEBREAKER  },
        { "11. 四子连珠棋 (博弈)", SCORE_CONNECT4     },
        { "12. 成语接龙轮 (国学)", SCORE_IDIOM_WHEEL  },
        { "13. 环轨防卫战 (瞄准)", SCORE_ORBIT_DEFEND },
        { "14. 数字华容道 (空间)", SCORE_SLIDE_PUZZLE },
    };

    const char *rank = "好奇小萌芽";
    if (g_prof.stars >= 80)      rank = "墨芽全能大状元";
    else if (g_prof.stars >= 45) rank = "博学智慧星";
    else if (g_prof.stars >= 15) rank = "森林探险家";

    int top_idx = 0;
    bool first = true;
    bsp_audio_sfx(SFX_CONFIRM);

    while (1) {
        gfx_clear(GFX_WHITE);
        gfx_draw_top_bar("★ 星星荣誉墙 · 成长勋章册", g_prof.stars, app_eye_mins_left());

        gfx_round_rect(24, 56, 270, 366, 16, 3, GFX_BLACK);
        gfx_draw_art(159, 175, 165, ART_TROPHY_STARS, GFX_BLACK);
        gfx_draw_text_center(159, 270, rank, FONT_24, GFX_BLACK);

        char stat1[64], stat2[64];
        snprintf(stat1, sizeof(stat1), "累计星星：★ %d 颗", (int)g_prof.stars);
        snprintf(stat2, sizeof(stat2), "读完绘本：%d 册", (int)g_prof.stories_read);
        gfx_draw_text_center(159, 322, stat1, FONT_24, GFX_BLACK);
        gfx_draw_text_center(159, 362, stat2, FONT_24, GFX_BLACK);

        gfx_round_rect(314, 56, 462, 366, 16, 2, GFX_BLACK);
        char btitle[80];
        snprintf(btitle, sizeof(btitle), "十四大滚轮游戏纪录榜 (%d~%d/14)", top_idx + 1, top_idx + 7);
        gfx_draw_text_center(545, 70, btitle, FONT_24, GFX_BLACK);

        for (int i = 0; i < 7; i++) {
            int idx = top_idx + i;
            char row[96];
            snprintf(row, sizeof(row), "%s  ——  %d 分",
                     k_rows[idx].label, app_get_high_score(k_rows[idx].id));
            gfx_draw_text(336, 112 + i * 42, row, FONT_24, GFX_BLACK);
        }

        gfx_draw_bottom_hints("滚轮上下翻看榜单", "按下返回", "返回上级");
        bsp_epd_smart_present(gfx_get_fb(), first);
        first = false;

        bsp_key_t k = bsp_input_wait(30000);
        if (k == KEY_OK || k == KEY_BACK) break;
        else if (k == KEY_PREV && top_idx > 0) { top_idx--; bsp_audio_sfx(SFX_WHEEL_TICK); }
        else if (k == KEY_NEXT && top_idx < 7) { top_idx++; bsp_audio_sfx(SFX_WHEEL_TICK); }
    }
}

void app_fun_trophy_hub_run(void)
{
    int sel = 0;
    bool first = true;
    while (1) {
        gfx_clear(GFX_WHITE);
        gfx_draw_top_bar("★ 变声小鹦鹉与星星荣誉墙", g_prof.stars, app_eye_mins_left());

        const char *titles[2] = { "变声小鹦鹉 (录音搞怪)", "星星荣誉墙 (成长勋章)" };
        const char *subs[2] = {
            "用板载数字麦克风录下宝贝讲的故事或儿歌，一键切换『原声 / 花栗鼠 / 大狗熊』三种趣味变声！",
            "查看宝贝累计收集的智慧星、阅读册数勋章头衔，以及六款滚轮益智游戏的历史最高分纪录！"
        };
        gfx_art_id_t arts[2] = { ART_PARROT_MIC, ART_TROPHY_STARS };

        for (int i = 0; i < 2; i++) {
            int x = 36 + i * 374;
            bool focus = (i == sel);
            gfx_round_rect(x, 64, 354, 336, 18, focus ? 5 : 2, GFX_BLACK);
            if (focus) {
                gfx_fill_round_rect(x + 12, 76, 330, 48, 12, GFX_BLACK);
                gfx_draw_text_center(x + 177, 88, titles[i], FONT_24, GFX_WHITE);
            } else {
                gfx_draw_text_center(x + 177, 88, titles[i], FONT_24, GFX_BLACK);
            }
            gfx_draw_art(x + 177, 200, 145, arts[i], GFX_BLACK);
            gfx_draw_paragraph(x + 24, 276, 306, 108, 8, subs[i], FONT_16, GFX_BLACK, NULL);
        }
        gfx_draw_bottom_hints("滚轮选择模块", "按下进入", "返回主页");
        bsp_epd_smart_present(gfx_get_fb(), first);
        first = false;

        bsp_key_t k = bsp_input_wait(30000);
        if (k == KEY_BACK) break;
        else if (k == KEY_PREV || k == KEY_NEXT) { sel ^= 1; bsp_audio_sfx(SFX_WHEEL_TICK); }
        else if (k == KEY_OK) {
            bsp_audio_sfx(SFX_CONFIRM);
            if (sel == 0) app_voice_parrot_run();
            else app_trophy_wall_run();
            first = true;
        }
    }
}

void app_enter_sleep_clock(void)
{
    bsp_audio_stop_all();
    bool first = true;
    int last_min = -1;

    while (1) {
        bsp_rtc_time_t tm;
        bsp_status_t st;
        bsp_rtc_get_time(&tm);
        bsp_board_read_status(&st);

        if (first || tm.minute != last_min) {
            last_min = tm.minute;
            gfx_clear(GFX_WHITE);
            gfx_round_rect(22, 22, GFX_W - 44, GFX_H - 44, 24, 3, GFX_BLACK);

            /* Left Cozy Illustration Card: Sleeping Bear */
            gfx_fill_round_rect(44, 48, 226, 384, 18, GFX_GRAY_LIGHT);
            gfx_round_rect(44, 48, 226, 384, 18, 2, GFX_BLACK);
            gfx_fill_round_rect(58, 66, 198, 228, 14, GFX_WHITE);
            gfx_round_rect(58, 66, 198, 228, 14, 2, GFX_BLACK);
            gfx_draw_art(157, 180, 165, ART_SLEEP_BEAR, GFX_BLACK);
            gfx_draw_text_center(157, 318, "★ 墨芽童伴 · 晚安画报", FONT_16, GFX_BLACK);
            gfx_draw_text_center(157, 352, "零功耗墨水待机中", FONT_16, GFX_BLACK);

            /* Right Twin-Bell Alarm Clock Icon + Giant 68px Digital Time */
            gfx_draw_art(350, 134, 92, ART_ALARM_CLOCK, GFX_BLACK);
            char clk[32];
            snprintf(clk, sizeof(clk), "%02u:%02u", tm.hour, tm.minute);
            gfx_draw_clock_digits(422, 100, clk);

            gfx_hline(302, 206, 442, GFX_BLACK);

            char date_s[64];
            snprintf(date_s, sizeof(date_s), "%04u年%02u月%02u日     电量 %d%%",
                     tm.year, tm.month, tm.day, st.battery_percent);
            gfx_draw_text(308, 232, date_s, FONT_24, GFX_BLACK);

            if (st.env_valid) {
                char env_s[80];
                snprintf(env_s, sizeof(env_s), "儿童房温湿度：%.1f℃   湿度 %.0f%%", st.temp_c, st.humi_rh);
                gfx_draw_text(308, 286, env_s, FONT_24, GFX_BLACK);
            }
            gfx_fill_round_rect(304, 348, 440, 48, 12, GFX_GRAY_LIGHT);
            gfx_round_rect(304, 348, 440, 48, 12, 2, GFX_BLACK);
            gfx_draw_text_center(524, 364, "拨动或按下顶肩滚轮即可唤醒故事机", FONT_16, GFX_BLACK);

            bsp_epd_smart_present(gfx_get_fb(), first);
            bsp_epd_sleep(); /* Drop e-Paper power rail ALDO3 to 0uA while image stays bi-stable! */
            first = false;
        }

        bsp_key_t k = bsp_input_wait(5000);
        if (k == KEY_PREV || k == KEY_NEXT || k == KEY_OK || k == KEY_BACK || k == KEY_PWR_SHORT) {
            bsp_epd_wake();
            app_eye_reset_session();
            bsp_audio_sfx(SFX_CONFIRM);
            break;
        }
    }
}

void app_parent_hub_run(void)
{
    int sel = 0;
    bool editing = false;
    bool first = true;
    bsp_rtc_time_t tm;
    bsp_rtc_get_time(&tm);

    while (1) {
        gfx_clear(GFX_WHITE);
        gfx_draw_top_bar("★ 配网与家长站 (家庭Wi-Fi · AI设置 · 传书 · 护眼)", g_prof.stars, app_eye_mins_left());

        char rows[9][120];
        snprintf(rows[0], sizeof(rows[0]), "1. ★ 家庭Wi-Fi与AI设置 [%s · %s]",
                 app_wifi_has_credentials() ? g_net_cfg.wifi_ssid : "扫码配网",
                 app_ai_provider_label(g_net_cfg.ai_provider));
        snprintf(rows[1], sizeof(rows[1]), "2. ★ 传书工坊：USB电脑优盘拖拽 / 手机无线传有声书");
        snprintf(rows[2], sizeof(rows[2]), "3. 当前扬声器音量：%d%% %s",
                 (int)g_prof.volume, (sel == 2 && editing) ? "[滚轮调节中]" : "");
        snprintf(rows[3], sizeof(rows[3]), "4. 儿童听力保护上限：%d%% %s",
                 (int)g_prof.max_volume, (sel == 3 && editing) ? "[滚轮调节中]" : "");
        if (g_prof.eye_limit_min == 0)
            snprintf(rows[4], sizeof(rows[4]), "5. 护眼防沉迷番茄钟：已关闭");
        else
            snprintf(rows[4], sizeof(rows[4]), "5. 护眼防沉迷番茄钟：每 %d 分钟提醒远眺", (int)g_prof.eye_limit_min);
        snprintf(rows[5], sizeof(rows[5]), "6. 握持与滚轮方向：%s",
                 g_prof.flip_180 ? "翻转180° (滚轮在下侧)" : "标准横屏0° (滚轮在顶肩)");
        snprintf(rows[6], sizeof(rows[6]), "7. 时钟分钟微调：%02u:%02u %s",
                 tm.hour, tm.minute, (sel == 6 && editing) ? "[滚轮调时间]" : "");
        snprintf(rows[7], sizeof(rows[7]), "8. 立即进入桌面待机座钟画报 (0mW超长续航)");
        snprintf(rows[8], sizeof(rows[8]), "9. 安全关闭整机电源 (长按侧边PWR键1秒开机)");

        int start = (sel >= 5) ? (sel - 4) : 0;
        for (int r = 0; r < 5 && (start + r) < 9; r++) {
            int idx = start + r;
            int y = 58 + r * 72;
            if (idx == sel) {
                gfx_fill_round_rect(36, y, GFX_W - 72, 60, 12, GFX_BLACK);
                gfx_draw_text(60, y + 18, rows[idx], FONT_24, GFX_WHITE);
            } else {
                gfx_round_rect(36, y, GFX_W - 72, 60, 12, 2, GFX_BLACK);
                gfx_draw_text(60, y + 18, rows[idx], FONT_24, GFX_BLACK);
            }
        }

        gfx_draw_bottom_hints(editing ? "滚轮增减数值" : "滚轮选功能项",
                              editing ? "按下保存" : "按下进入/修改", "保存返回主页");
        bsp_epd_smart_present(gfx_get_fb(), first);
        first = false;

        bsp_key_t k = bsp_input_wait(30000);
        if (k == KEY_BACK) {
            if (editing) editing = false;
            else { app_profile_save(); break; }
        } else if (k == KEY_PREV) {
            if (!editing) {
                sel = (sel + 8) % 9;
                bsp_audio_sfx(SFX_WHEEL_TICK);
            } else if (sel == 2) {
                g_prof.volume = (g_prof.volume + 10 > g_prof.max_volume) ? g_prof.max_volume : (g_prof.volume + 10);
                bsp_audio_set_volume(g_prof.volume);
                bsp_audio_sfx(SFX_WHEEL_TICK);
            } else if (sel == 3) {
                g_prof.max_volume = (g_prof.max_volume + 10 > 100) ? 100 : (g_prof.max_volume + 10);
                bsp_audio_set_max_volume(g_prof.max_volume);
            } else if (sel == 6) {
                tm.minute = (tm.minute + 1) % 60;
                if (tm.minute == 0) tm.hour = (tm.hour + 1) % 24;
                bsp_rtc_set_time(&tm);
            }
        } else if (k == KEY_NEXT) {
            if (!editing) {
                sel = (sel + 1) % 9;
                bsp_audio_sfx(SFX_WHEEL_TICK);
            } else if (sel == 2) {
                g_prof.volume = (g_prof.volume - 10 < 0) ? 0 : (g_prof.volume - 10);
                bsp_audio_set_volume(g_prof.volume);
                bsp_audio_sfx(SFX_WHEEL_TICK);
            } else if (sel == 3) {
                g_prof.max_volume = (g_prof.max_volume - 10 < 40) ? 40 : (g_prof.max_volume - 10);
                bsp_audio_set_max_volume(g_prof.max_volume);
                g_prof.volume = bsp_audio_get_volume();
            } else if (sel == 6) {
                if (tm.minute == 0) { tm.minute = 59; tm.hour = (tm.hour + 23) % 24; }
                else tm.minute--;
                bsp_rtc_set_time(&tm);
            }
        } else if (k == KEY_OK) {
            if (sel == 0) {
                bsp_audio_sfx(SFX_CONFIRM);
                app_wifi_provision_run(true);
                first = true;
            } else if (sel == 1) {
                bsp_audio_sfx(SFX_CONFIRM);
                app_transfer_hub_run();
                first = true;
            } else if (sel == 2 || sel == 3 || sel == 6) {
                editing = !editing;
                bsp_audio_sfx(SFX_CONFIRM);
            } else if (sel == 4) {
                const int opts[4] = { 15, 20, 30, 0 };
                int cur = 0;
                for (int i = 0; i < 4; i++) if (opts[i] == g_prof.eye_limit_min) cur = i;
                g_prof.eye_limit_min = opts[(cur + 1) % 4];
                app_eye_reset_session();
                bsp_audio_sfx(SFX_CONFIRM);
            } else if (sel == 5) {
                g_prof.flip_180 = !g_prof.flip_180;
                gfx_set_flipped(g_prof.flip_180 != 0);
                bsp_input_set_invert_wheel(g_prof.flip_180 != 0);
                first = true;
                bsp_audio_sfx(SFX_CONFIRM);
            } else if (sel == 7) {
                app_profile_save();
                app_enter_sleep_clock();
                first = true;
            } else if (sel == 8) {
                app_profile_save();
                gfx_clear(GFX_WHITE);
                gfx_draw_art(GFX_W / 2, 195, 180, ART_SLEEP_BEAR, GFX_BLACK);
                gfx_draw_text_center(GFX_W / 2, 320, "晚安小朋友，下次再一起读绘本玩游戏哟！", FONT_24, GFX_BLACK);
                gfx_draw_text_center(GFX_W / 2, 365, "(长按侧边 PWR 键 1 秒即可重新开机)", FONT_16, GFX_BLACK);
                bsp_epd_present(gfx_get_fb(), EPD_REFRESH_FAST);
                bsp_board_power_off();
            }
        }
    }
}
