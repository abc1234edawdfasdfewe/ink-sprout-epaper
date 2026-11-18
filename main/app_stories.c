#include "app_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "esp_log.h"

typedef struct {
    const char  *title;
    const char  *age_tag;
    gfx_art_id_t art;
    bsp_melody_t melody;
    int          page_count;
    const char  *pages[5];
    const char  *quiz_q;
    const char  *quiz_opts[3];
    int          quiz_ans;
} builtin_story_t;

static const builtin_story_t k_stories[] = {
    {
        .title = "小兔子乖乖",
        .age_tag = "3-6岁 · 安全儿歌",
        .art = ART_RABBIT,
        .melody = MELODY_RABBIT,
        .page_count = 4,
        .pages = {
            "森林里有一座漂亮的小木屋，里面住着兔妈妈和三只可爱的小白兔：红眼睛、长耳朵、短尾巴。",
            "一天早晨，兔妈妈挎着竹篮要去拔大萝卜。临走前，兔妈妈叮嘱道：“好孩子，把门关牢牢，陌生人敲门千万不能开哟！”",
            "不一会儿，大灰狼捏着嗓子来敲门：“小兔子乖乖，把门儿开开！”小兔子从门缝里一瞧，看到毛茸茸的大尾巴，齐声大喊：“不开不开就不开，妈妈没回来，谁来也不开！”",
            "傍晚兔妈妈回来了，三只小兔高兴地打开门，吃到了香甜的大胡萝卜！兔妈妈夸大家都是机灵又安全的好宝宝。"
        },
        .quiz_q = "故事小问答：陌生人敲门要给糖吃，小兔子该怎么做？",
        .quiz_opts = { "坚决不开门等妈妈", "马上打开门拿糖", "跟着陌生人走" },
        .quiz_ans = 0,
    },
    {
        .title = "龟兔赛跑",
        .age_tag = "3-6岁 · 坚持寓言",
        .art = ART_TURTLE,
        .melody = MELODY_ODE_JOY,
        .page_count = 4,
        .pages = {
            "骄傲的小白兔总爱笑话小乌龟爬得慢。小乌龟认真地说：“那咱们来比一比，看谁先跑到山顶的大松树下吧！”",
            "发令枪一响，小白兔像一阵风似的冲了出去。跑了一半，它回头一看，小乌龟还在老远的地方呢。",
            "“哈哈，我先睡一觉再跑也来得及！”小白兔躺在软软的草地上呼呼大睡起来。而小乌龟一步一步，擦擦汗水，一刻也不停地往上爬。",
            "等小白兔一觉醒来，小乌龟已经站在山顶大松树下冲它招手啦！小白兔红着脸明白了：只有坚持不懈，才能到达终点。"
        },
        .quiz_q = "故事小问答：小乌龟为什么能赢得比赛呢？",
        .quiz_opts = { "因为它长出了翅膀", "一步一步坚持不放弃", "小白兔让着它" },
        .quiz_ans = 1,
    },
    {
        .title = "小蝌蚪找妈妈",
        .age_tag = "3-6岁 · 自然科普",
        .art = ART_TADPOLE_FROG,
        .melody = MELODY_TWINKLE,
        .page_count = 4,
        .pages = {
            "春天来了，池塘里有一群圆脑袋、黑灰身子、甩着长尾巴的小蝌蚪，快活地游来游去。",
            "小蝌蚪看见鸭妈妈带着小鸭子，羡慕极了，决定去找自己的妈妈。鲤鱼阿姨微笑着告诉它们：“你们的妈妈四条腿，宽嘴巴！”",
            "游啊游，小蝌蚪长出了两条后腿，又长出了两条前腿。乌龟伯伯笑着说：“我不是你们妈妈，你们的妈妈披着碧绿的衣裳，露着雪白的肚皮，鼓着两只大眼睛！”",
            "荷叶上蹲着一只大青蛙！小蝌蚪尾巴慢慢变短不见了，轻轻一跳跃上荷叶：“妈妈！”从此它们跟着青蛙妈妈天天去田里捉害虫。"
        },
        .quiz_q = "科普小问答：小蝌蚪长大变青蛙时，先长出哪两条腿？",
        .quiz_opts = { "先长出两条后腿", "先长出翅膀", "四条腿一起掉下来" },
        .quiz_ans = 0,
    },
    {
        .title = "三只小猪盖房子",
        .age_tag = "3-6岁 · 勤劳启蒙",
        .art = ART_THREE_PIGS,
        .melody = MELODY_RABBIT,
        .page_count = 4,
        .pages = {
            "猪妈妈让三只小猪学本领，自己动手盖一座新房子。猪大哥想偷懒，抱来一把稻草，半天就搭好了草房子。",
            "猪二哥捡来一些细树枝，钉钉敲敲搭了一间木头房，也跑去玩耍了。只有猪小弟不怕辛苦，一块一块搬来红砖和水泥，认真砌成坚固的砖房。",
            "大灰狼来了！它深吸一口气，“呼——”吹倒了草房子，又用力撞塌了木房子。大哥二哥连忙跑进猪小弟的红砖房里锁好门。",
            "大灰狼怎么吹也吹不动红砖房，脸都憋红了只好灰溜溜逃跑！大哥二哥向猪小弟学习，从此做事再也不图省事偷懒啦。"
        },
        .quiz_q = "故事小问答：哪座房子最结实，挡住了大灰狼？",
        .quiz_opts = { "稻草盖的房子", "细树枝搭的房子", "认真砌的红砖房" },
        .quiz_ans = 2,
    },
    {
        .title = "曹冲称象",
        .age_tag = "6-12岁 · 科学思维",
        .art = ART_ELEPHANT,
        .melody = MELODY_ADVENTURE,
        .page_count = 4,
        .pages = {
            "古时候，有人送给曹操一头又高又大的大象，腿粗得像四根大柱子。官员们议论纷纷：“这么重的大象，没有那么大的秤，怎么称出重量呢？”",
            "有人说造一杆大树秤，可谁也提不动；有人说把大象切开称，那怎么行呢！这时，年仅七岁的曹冲站出来说：“我有一个好办法！”",
            "曹冲请大家把大象牵到一艘大船上，船身往下沉，他沿着水面在船舷上刻下一道记号。接着把大象牵上岸，往空船里一块一块装石头。",
            "等船再次沉到刚才刻记号的水线时，曹冲说：“现在把船上的石头分批用普通秤称一称，加起来不就是大象的重量了吗？”大家都佩服曹冲爱动脑筋的等量替换智慧！"
        },
        .quiz_q = "思维小问答：曹冲用了什么科学方法来称出大象重量？",
        .quiz_opts = { "等量替换（水线相同重量相等）", "造了一杆十米长的大秤", "靠眼睛猜出来的" },
        .quiz_ans = 0,
    },
    {
        .title = "司马光砸缸",
        .age_tag = "6-12岁 · 机智应变",
        .art = ART_SIMAGUANG,
        .melody = MELODY_ODE_JOY,
        .page_count = 3,
        .pages = {
            "北宋时期，有个叫司马光的孩子常和伙伴们在假山庭院里玩捉迷藏。假山脚下放着一口装满水的大水缸，比小朋友还要高。",
            "有个小同伴爬上假山张望，脚下一滑，“扑通”一声掉进了大水缸里！别的孩子吓得哭喊着跑去找大人，可等大人赶来就太晚了。",
            "司马光没有慌乱，他看准地上的一块大石头，双手抱起来使劲朝水缸砸去！“咔嚓”一声水缸破开大洞，缸里的水哗哗流出来，落水的小伙伴得救了！"
        },
        .quiz_q = "思维小问答：司马光的机智体现在哪种逆向思考？",
        .quiz_opts = { "自己跳进缸里捞人", "让人离开水不行，就让水离开人", "站着大哭等明天" },
        .quiz_ans = 1,
    },
    {
        .title = "为什么月亮会变弯？",
        .age_tag = "6-12岁 · 天文百科",
        .art = ART_MOON_STARS,
        .melody = MELODY_LULLABY,
        .page_count = 4,
        .pages = {
            "抬头望夜空，月亮有时像弯弯的小船，有时像圆圆的玉盘。难道月亮真的会变瘦又变胖吗？其实月球一直是一颗圆球！",
            "月球自己不会发光，我们看到的月光，其实是太阳照亮月球表面的反光。月球绕着地球不停转动，我们在地球上看到被照亮的那一面角度每天都在变化。",
            "当月球转到地球和太阳中间，亮面背对我们，就是看不见的“新月”；转到侧面时，只能看到一半亮面，就成了弯弯的“蛾眉月”和半圆的“弦月”。",
            "到了农历十五前后，地球位于太阳和月球之间，太阳把朝向我们的整张月面照得亮晶晶，这就是团圆美丽的“满月”啦！"
        },
        .quiz_q = "百科小问答：夜空中明亮的月光是从哪里来的？",
        .quiz_opts = { "月球内部自己在燃烧发光", "月球反射了太阳的光芒", "地球上的路灯照亮的" },
        .quiz_ans = 1,
    },
};
#define BUILTIN_PLAIN_COUNT  ((int)(sizeof(k_stories) / sizeof(k_stories[0])))

/* === Story #8: Interactive Thumbwheel Branching Gamebook === */
typedef struct {
    const char  *scene;
    const char  *opt_a;
    int          next_a;
    const char  *opt_b;
    int          next_b;
    int          reward_stars; /* >0 means ending node */
} branch_node_t;

static const branch_node_t k_adventure[] = {
    /* 0 */ {
        "清晨，小勇者背上水壶走进彩虹森林寻找会唱歌的音乐水晶。前方出现岔路：左边小溪边有一只翅膀沾湿的小松鼠在发抖，右边是直通山顶的近路。",
        "A. 停下来帮小松鼠擦干取暖", 1,
        "B. 不管它，赶紧走右边近路", 2, 0
    },
    /* 1 */ {
        "小松鼠感激地送给你一颗发光橡果，并悄悄告诉你：“山洞守卫石像最喜欢听礼貌咒语！”继续往前，一座古老石门挡住了去路。",
        "A. 微笑着说：石像爷爷您好，请开门", 3,
        "B. 用木棍使劲敲打石门", 4, 0
    },
    /* 2 */ {
        "你匆匆走上陡峭近路，结果遇到浓雾迷路绕到了古老石门前。石像守卫睁开眼睛问：“小旅人，怎样才能打开智慧之门呢？”",
        "A. 静下心礼貌请教石像爷爷", 3,
        "B. 生气地大喊快给我让开", 4, 0
    },
    /* 3 */ {
        "石门轰隆隆缓缓打开！洞窟中央有两座台座：左边放着朴素却散发温暖歌声的透明水晶，右边放着金光闪闪却冰冷刺眼的假皇冠。",
        "A. 捧起温暖唱歌的透明水晶", 5,
        "B. 抢走金光闪闪的大皇冠", 6, 0
    },
    /* 4 */ {
        "石像摇摇头叹气：“缺乏耐心和礼貌可找不到真正的宝藏呀。”一阵温柔的清风把你送回了森林入口。下次试试礼貌又善良的选择吧！【结局：礼貌初体验】",
        "重新出发冒险", -1, "返回绘本书架", -2, 1
    },
    /* 5 */ {
        "透明水晶发出清脆悦耳的八音盒歌声！整片彩虹森林的花朵一起绽放，小松鼠和森林伙伴们为你戴上“善良智慧小勇者”勋章！【完美结局 ★+3】",
        "再玩一次探索不同路线", -1, "返回绘本书架", -2, 3
    },
    /* 6 */ {
        "刚碰到金皇冠，它“噗”地变成了一团蒲公英飞走啦！石像笑着说：“真正的宝贝不是虚荣的金子，而是温暖的心灵哟。”【趣味结局 ★+2】",
        "再试一次寻找真水晶", -1, "返回绘本书架", -2, 2
    },
};

static void run_story_quiz(const builtin_story_t *st)
{
    int sel = 0;
    bool answered = false;
    bool correct = false;
    bool first = true;

    while (1) {
        gfx_clear(GFX_WHITE);
        gfx_draw_top_bar("★ 读后智慧问答", g_prof.stars, app_eye_mins_left());
        gfx_round_rect(24, 56, GFX_W - 48, 96, 12, 2, GFX_BLACK);
        gfx_draw_paragraph(44, 70, GFX_W - 88, 72, 8, st->quiz_q, FONT_24, GFX_BLACK, NULL);

        for (int i = 0; i < 3; i++) {
            int y = 172 + i * 72;
            bool focus = (i == sel);
            if (focus) {
                gfx_fill_round_rect(60, y, GFX_W - 120, 56, 12, GFX_BLACK);
                gfx_draw_text(92, y + 16, st->quiz_opts[i], FONT_24, GFX_WHITE);
            } else {
                gfx_round_rect(60, y, GFX_W - 120, 56, 12, 2, GFX_BLACK);
                gfx_draw_text(92, y + 16, st->quiz_opts[i], FONT_24, GFX_BLACK);
            }
        }

        if (answered) {
            gfx_fill_round_rect(140, 386, GFX_W - 280, 46, 10, GFX_GRAY_LIGHT);
            gfx_round_rect(140, 386, GFX_W - 280, 46, 10, 2, GFX_BLACK);
            gfx_draw_text_center(GFX_W / 2, 397,
                correct ? "★ 答对啦！奖励 2 颗智慧星！按滚轮完成" : "再想一想，拨动滚轮试试别的选项吧！",
                FONT_24, GFX_BLACK);
        } else {
            gfx_draw_bottom_hints("滚轮选答案", "按下提交", "返回书架");
        }

        bsp_epd_smart_present(gfx_get_fb(), first);
        first = false;

        bsp_key_t k = bsp_input_wait(30000);
        if (k == KEY_BACK) break;
        if (correct && k == KEY_OK) break;
        if (k == KEY_PREV) { sel = (sel + 2) % 3; answered = false; bsp_audio_sfx(SFX_WHEEL_TICK); }
        else if (k == KEY_NEXT) { sel = (sel + 1) % 3; answered = false; bsp_audio_sfx(SFX_WHEEL_TICK); }
        else if (k == KEY_OK) {
            answered = true;
            if (sel == st->quiz_ans) {
                correct = true;
                app_add_stars(2);
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

static void run_builtin_story(int idx)
{
    const builtin_story_t *st = &k_stories[idx];
    int page = g_prof.story_bookmarks[idx];
    if (page < 0 || page >= st->page_count) page = 0;

    bool music_on = true;
    if (st->melody != MELODY_NONE) {
        bsp_audio_start_melody(st->melody, false);
    }

    bool first = true;
    while (1) {
        gfx_clear(GFX_WHITE);
        char hdr[96];
        snprintf(hdr, sizeof(hdr), "《%s》", st->title);
        gfx_draw_top_bar(hdr, g_prof.stars, app_eye_mins_left());

        /* Left Illustration Card (240px wide) */
        gfx_fill_round_rect(20, 56, 236, 368, 16, GFX_GRAY_LIGHT);
        gfx_round_rect(20, 56, 236, 368, 16, 3, GFX_BLACK);
        gfx_fill_round_rect(34, 70, 208, 290, 12, GFX_WHITE);
        gfx_round_rect(34, 70, 208, 290, 12, 2, GFX_BLACK);
        gfx_draw_art(138, 212, 165, st->art, GFX_BLACK);

        gfx_draw_text_center(138, 372, st->age_tag, FONT_16, GFX_BLACK);
        char pg_str[48];
        snprintf(pg_str, sizeof(pg_str), "第 %d / %d 页  %s",
                 page + 1, st->page_count, music_on ? "♪伴奏开" : "♪静音");
        gfx_draw_text_center(138, 396, pg_str, FONT_16, GFX_BLACK);

        /* Right Large-Print Story Page */
        gfx_round_rect(272, 56, 508, 368, 16, 2, GFX_BLACK);
        gfx_draw_paragraph(296, 82, 460, 316, 14, st->pages[page], FONT_24, GFX_BLACK, NULL);

        gfx_draw_bottom_hints("滚轮翻页/结尾进问答", "生字拼音田字格点读", "返回(摇一摇开关伴奏)");
        bsp_epd_smart_present(gfx_get_fb(), first);
        first = false;

        g_prof.story_bookmarks[idx] = (uint8_t)page;

        bsp_key_t k = bsp_input_wait(30000);
        if (k == KEY_BACK) {
            bsp_audio_stop_all();
            app_profile_save();
            break;
        } else if (k == KEY_PREV) {
            if (page > 0) { page--; bsp_audio_sfx(SFX_PAGE_TURN); }
        } else if (k == KEY_NEXT) {
            if (page < st->page_count - 1) {
                page++;
                bsp_audio_sfx(SFX_PAGE_TURN);
            } else {
                bsp_audio_stop_all();
                g_prof.story_bookmarks[idx] = 0;
                run_story_quiz(st);
                break;
            }
        } else if (k == KEY_SHAKE) {
            music_on = !music_on;
            if (music_on && st->melody != MELODY_NONE) bsp_audio_start_melody(st->melody, false);
            else bsp_audio_stop_all();
        } else if (k == KEY_OK) {
            /* Launch Thumbwheel Hanzi Pinyin + Kaiti Tianzige Spot-Reader on current page! */
            char btitle[80];
            snprintf(btitle, sizeof(btitle), "《%s》", st->title);
            app_word_study_page(st->pages[page], btitle);
            if (music_on && st->melody != MELODY_NONE) bsp_audio_start_melody(st->melody, false);
            first = true;
        }
        app_check_eye_guard();
    }
}

static void run_branching_gamebook(void)
{
    int node = 0;
    int sel = 0;
    bool first = true;
    bsp_audio_start_melody(MELODY_ADVENTURE, false);

    while (1) {
        const branch_node_t *nd = &k_adventure[node];
        gfx_clear(GFX_WHITE);
        gfx_draw_top_bar("《森林勇者奇遇记》互动故事书", g_prof.stars, app_eye_mins_left());

        gfx_round_rect(20, 54, 210, 372, 14, 2, GFX_BLACK);
        gfx_draw_art(125, 200, 155, ART_KNIGHT_FOREST, GFX_BLACK);
        gfx_draw_text_center(125, 345, "滚轮做选择", FONT_24, GFX_BLACK);
        gfx_draw_text_center(125, 380, "决定故事结局！", FONT_16, GFX_BLACK);

        gfx_round_rect(246, 54, 534, 226, 14, 2, GFX_BLACK);
        gfx_draw_paragraph(268, 72, 490, 192, 10, nd->scene, FONT_24, GFX_BLACK, NULL);

        const char *opts[2] = { nd->opt_a, nd->opt_b };
        for (int i = 0; i < 2; i++) {
            int y = 294 + i * 66;
            if (i == sel) {
                gfx_fill_round_rect(246, y, 534, 54, 12, GFX_BLACK);
                gfx_draw_text(270, y + 15, opts[i], FONT_24, GFX_WHITE);
            } else {
                gfx_round_rect(246, y, 534, 54, 12, 2, GFX_BLACK);
                gfx_draw_text(270, y + 15, opts[i], FONT_24, GFX_BLACK);
            }
        }
        gfx_draw_bottom_hints("滚轮切换行动路线", "按下决定命运", "退出(摇一摇生字点读)");
        bsp_epd_smart_present(gfx_get_fb(), first);
        first = false;

        bsp_key_t k = bsp_input_wait(30000);
        if (k == KEY_BACK) { bsp_audio_stop_all(); break; }
        if (k == KEY_SHAKE) {
            app_word_study_page(nd->scene, "《森林勇者奇遇记》");
            bsp_audio_start_melody(MELODY_ADVENTURE, false);
            first = true;
            continue;
        }
        if (k == KEY_PREV || k == KEY_NEXT) {
            sel ^= 1;
            bsp_audio_sfx(SFX_WHEEL_TICK);
        } else if (k == KEY_OK) {
            int nxt = (sel == 0) ? nd->next_a : nd->next_b;
            if (nxt == -1) {
                node = 0; sel = 0; first = true;
                bsp_audio_sfx(SFX_CONFIRM);
            } else if (nxt == -2) {
                bsp_audio_stop_all();
                break;
            } else {
                node = nxt;
                sel = 0;
                if (k_adventure[node].reward_stars > 0) {
                    app_add_stars(k_adventure[node].reward_stars);
                    g_prof.stories_read++;
                    app_profile_save();
                    bsp_audio_sfx(SFX_FANFARE);
                } else {
                    bsp_audio_sfx(SFX_PAGE_TURN);
                }
            }
        }
    }
}

/* === MicroSD /sdcard/stories Reader & WAV Story Player === */
typedef struct {
    char name[96];
    char fullpath[160];
    bool is_wav;
} sd_story_item_t;

static void run_sd_wav_player(const sd_story_item_t *item)
{
    bsp_audio_play_wav(item->fullpath);
    bool first = true;
    while (1) {
        gfx_clear(GFX_WHITE);
        gfx_draw_top_bar("♪ 护眼有声故事屋", g_prof.stars, app_eye_mins_left());

        gfx_round_rect(40, 62, GFX_W - 80, 356, 18, 3, GFX_BLACK);
        gfx_draw_art(200, 235, 175, ART_MOON_STARS, GFX_BLACK);

        gfx_draw_text(350, 105, "正在播放：", FONT_16, GFX_BLACK);
        gfx_draw_paragraph(350, 135, 380, 64, 8, item->name, FONT_24, GFX_BLACK, NULL);

        int pct = bsp_audio_get_wav_progress_pct();
        gfx_round_rect(350, 225, 370, 24, 10, 2, GFX_BLACK);
        int pw = (362 * pct) / 100;
        if (pw > 0) gfx_fill_round_rect(354, 229, pw, 16, 6, GFX_BLACK);

        char vstr[96];
        snprintf(vstr, sizeof(vstr), "状态：%s     音量：%d%% (上限%d%%)",
                 bsp_audio_is_paused() ? "已暂停" : (bsp_audio_is_playing() ? "播放中" : "播放完毕"),
                 g_prof.volume, g_prof.max_volume);
        gfx_draw_text(350, 272, vstr, FONT_24, GFX_BLACK);
        gfx_draw_text(350, 322, "提示：墨水屏静止零耗电，闭上眼睛安心听故事吧！", FONT_16, GFX_BLACK);

        gfx_draw_bottom_hints("滚轮调节音量大小", "暂停/继续播放", "停止并返回");
        bsp_epd_smart_present(gfx_get_fb(), first);
        first = false;

        bsp_key_t k = bsp_input_wait(2500);
        if (k == KEY_BACK) {
            bsp_audio_stop_all();
            break;
        } else if (k == KEY_PREV) {
            g_prof.volume += 5;
            if (g_prof.volume > g_prof.max_volume) g_prof.volume = g_prof.max_volume;
            bsp_audio_set_volume(g_prof.volume);
        } else if (k == KEY_NEXT) {
            g_prof.volume -= 5;
            if (g_prof.volume < 0) g_prof.volume = 0;
            bsp_audio_set_volume(g_prof.volume);
        } else if (k == KEY_OK) {
            if (!bsp_audio_is_playing()) bsp_audio_play_wav(item->fullpath);
            else bsp_audio_toggle_pause();
        }
    }
    app_profile_save();
}

static void run_sd_txt_reader(const sd_story_item_t *item)
{
    FILE *fp = fopen(item->fullpath, "rb");
    if (!fp) return;
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (sz <= 0) { fclose(fp); return; }
    if (sz > 128 * 1024) sz = 128 * 1024;
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(fp); return; }
    size_t rd = fread(buf, 1, (size_t)sz, fp);
    buf[rd] = '\0';
    fclose(fp);

    /* Companion WAV auto-narration if same stem exists! */
    char wav_companion[170];
    strncpy(wav_companion, item->fullpath, sizeof(wav_companion) - 6);
    char *dot = strrchr(wav_companion, '.');
    if (dot) strcpy(dot, ".wav");
    FILE *wchk = fopen(wav_companion, "rb");
    bool has_companion = (wchk != NULL);
    if (wchk) {
        fclose(wchk);
        bsp_audio_play_wav(wav_companion);
    }

    const char *page_offsets[128];
    int total_pages = 0;
    const char *cur = buf;
    while (cur && *cur && total_pages < 128) {
        page_offsets[total_pages++] = cur;
        const char *nxt = NULL;
        gfx_draw_paragraph(44, 72, GFX_W - 88, 336, 12, cur, FONT_24, GFX_BLACK, &nxt);
        cur = nxt;
    }
    if (total_pages == 0) total_pages = 1;

    /* Compute acoustic duration weights per page so companion WAV auto-turns pages smoothly! */
    uint16_t page_permille[128] = {0};
    uint32_t page_units[128] = {0};
    uint32_t total_units = 0;
    for (int i = 0; i < total_pages; i++) {
        const char *s = page_offsets[i];
        const char *e = (i + 1 < total_pages) ? page_offsets[i + 1] : (s + strlen(s));
        uint32_t u = 12;
        while (s < e && *s) {
            unsigned char b0 = (unsigned char)s[0];
            if (b0 < 0x80) {
                if (*s == '\n') u += 18;
                else if (*s != ' ' && *s != '\r') u += 4;
                s += 1;
            } else if ((b0 & 0xF0) == 0xE0 && s + 2 < e) {
                unsigned char b1 = (unsigned char)s[1], b2 = (unsigned char)s[2];
                if ((b0 == 0xE3 && b1 == 0x80 && b2 == 0x82) ||
                    (b0 == 0xEF && b1 == 0xBC && (b2 == 0x81 || b2 == 0x9F))) {
                    u += 24; /* 。！？ sentence breath pause */
                } else if ((b0 == 0xEF && b1 == 0xBC && b2 == 0x8C) ||
                           (b0 == 0xE3 && b1 == 0x80 && b2 == 0x81)) {
                    u += 12; /* ，、 clause pause */
                } else {
                    u += 10; /* spoken CJK character */
                }
                s += 3;
            } else {
                s += 1;
            }
        }
        page_units[i] = u;
        total_units += u;
    }
    uint32_t cum_u = 0;
    for (int i = 0; i < total_pages; i++) {
        page_permille[i] = (total_units > 0) ? (uint16_t)(cum_u * 1000ULL / total_units) : 0;
        cum_u += page_units[i];
    }

    int pg = 0;
    bool auto_flip = has_companion;
    bool first = true;
    while (1) {
        gfx_clear(GFX_WHITE);
        gfx_draw_top_bar(item->name, g_prof.stars, app_eye_mins_left());
        gfx_round_rect(20, 52, GFX_W - 40, 374, 14, 2, GFX_BLACK);
        gfx_draw_paragraph(44, 72, GFX_W - 88, 336, 12, page_offsets[pg], FONT_24, GFX_BLACK, NULL);

        char info[80];
        snprintf(info, sizeof(info), "%s  第 %d/%d 页",
                 has_companion ? (auto_flip ? "♪伴读自动翻页" : "♪伴读(手动翻页)") : "外置绘本",
                 pg + 1, total_pages);
        gfx_draw_text(GFX_W - 268, 398, info, FONT_16, GFX_BLACK);
        gfx_draw_bottom_hints("滚轮上下翻页", "生字拼音田字格点读", "返回(摇一摇暂停伴读)");
        bsp_epd_smart_present(gfx_get_fb(), first);
        first = false;

        bool wav_active = has_companion && bsp_audio_is_playing() && !bsp_audio_is_paused();
        bsp_key_t k = bsp_input_wait(wav_active && auto_flip ? 250 : 30000);
        if (k == KEY_NONE) {
            if (wav_active && auto_flip && pg + 1 < total_pages) {
                int pm = bsp_audio_get_wav_progress_permille();
                if (pm + 8 >= (int)page_permille[pg + 1]) {
                    pg++;
                } else {
                    continue; /* Keep E-Ink static until next page cue! */
                }
            } else {
                continue;
            }
        } else if (k == KEY_BACK) break;
        else if (k == KEY_PREV && pg > 0) { auto_flip = false; pg--; bsp_audio_sfx(SFX_PAGE_TURN); }
        else if (k == KEY_NEXT && pg < total_pages - 1) { auto_flip = false; pg++; bsp_audio_sfx(SFX_PAGE_TURN); }
        else if (k == KEY_SHAKE && has_companion) {
            if (!bsp_audio_is_playing()) {
                pg = 0;
                auto_flip = true;
                bsp_audio_play_wav(wav_companion);
            } else {
                bsp_audio_toggle_pause();
                if (!bsp_audio_is_paused()) auto_flip = true;
            }
        } else if (k == KEY_OK) {
            auto_flip = false;
            size_t slice_len = (pg + 1 < total_pages)
                             ? (size_t)(page_offsets[pg + 1] - page_offsets[pg])
                             : strlen(page_offsets[pg]);
            if (slice_len > 2048) slice_len = 2048;
            char *page_slice = (char *)calloc(1, slice_len + 1);
            if (page_slice) {
                memcpy(page_slice, page_offsets[pg], slice_len);
                app_word_study_page(page_slice, item->name);
                free(page_slice);
                first = true;
            }
        }
    }
    bsp_audio_stop_all();
    free(buf);
}

static void run_sdcard_shelf(void)
{
    bsp_sdcard_try_mount();
    sd_story_item_t *items = (sd_story_item_t *)calloc(32, sizeof(sd_story_item_t));
    int count = 0;

    if (bsp_sdcard_is_mounted()) {
        const char *dirs[2] = { bsp_storage_get_stories_dir(), bsp_storage_get_root() };
        for (int d = 0; d < 2 && count == 0; d++) {
            DIR *dir = opendir(dirs[d]);
            if (!dir) continue;
            struct dirent *ent;
            while ((ent = readdir(dir)) != NULL && count < 32) {
                if (ent->d_name[0] == '.') continue;
                size_t len = strlen(ent->d_name);
                bool is_txt = (len > 4 && strcasecmp(ent->d_name + len - 4, ".txt") == 0);
                bool is_wav = (len > 4 && strcasecmp(ent->d_name + len - 4, ".wav") == 0);
                if (!is_txt && !is_wav) continue;
                strncpy(items[count].name, ent->d_name, sizeof(items[count].name) - 1);
                snprintf(items[count].fullpath, sizeof(items[count].fullpath), "%s/%s", dirs[d], ent->d_name);
                items[count].is_wav = is_wav;
                count++;
            }
            closedir(dir);
        }
    }

    int sel = 0;
    bool first = true;
    while (1) {
        gfx_clear(GFX_WHITE);
        char shelf_hdr[96];
        snprintf(shelf_hdr, sizeof(shelf_hdr), "外置故事书架 (%s)", bsp_storage_get_medium_label());
        gfx_draw_top_bar(shelf_hdr, g_prof.stars, app_eye_mins_left());

        if (count == 0) {
            gfx_round_rect(60, 84, GFX_W - 120, 310, 16, 2, GFX_BLACK);
            gfx_draw_art(180, 236, 150, ART_RABBIT, GFX_BLACK);
            gfx_draw_text(300, 130, "当前还没有导入外置故事文件哟！", FONT_24, GFX_BLACK);
            gfx_draw_paragraph(300, 180, 400, 180, 10,
                "按下滚轮即可直接打开【传书工坊】：支持 Type-C 连接电脑像优盘一样拖入故事，或开启设备热点用手机扫码无线传故事！",
                FONT_16, GFX_BLACK, NULL);
            gfx_draw_bottom_hints("一键导入", "按下打开传书工坊", "返回内置绘本馆");
        } else {
            int page_start = (sel / 5) * 5;
            for (int i = 0; i < 5 && (page_start + i) < count; i++) {
                int idx = page_start + i;
                int y = 62 + i * 70;
                char row[140];
                snprintf(row, sizeof(row), "%s  %s",
                         items[idx].is_wav ? "[♪有声WAV]" : "[绘本TXT]", items[idx].name);
                if (idx == sel) {
                    gfx_fill_round_rect(40, y, GFX_W - 80, 58, 12, GFX_BLACK);
                    gfx_draw_text(64, y + 17, row, FONT_24, GFX_WHITE);
                } else {
                    gfx_round_rect(40, y, GFX_W - 80, 58, 12, 2, GFX_BLACK);
                    gfx_draw_text(64, y + 17, row, FONT_24, GFX_BLACK);
                }
            }
            gfx_draw_bottom_hints("滚轮选故事", "打开阅读/收听", "返回内置绘本馆");
        }

        bsp_epd_smart_present(gfx_get_fb(), first);
        first = false;

        bsp_key_t k = bsp_input_wait(30000);
        if (k == KEY_BACK) break;
        if (count == 0) {
            if (k == KEY_OK) {
                app_transfer_hub_run();
                free(items);
                run_sdcard_shelf();
                return;
            }
        } else {
            if (k == KEY_PREV) { sel = (sel + count - 1) % count; bsp_audio_sfx(SFX_WHEEL_TICK); }
            else if (k == KEY_NEXT) { sel = (sel + 1) % count; bsp_audio_sfx(SFX_WHEEL_TICK); }
            else if (k == KEY_OK) {
                bsp_audio_sfx(SFX_CONFIRM);
                if (items[sel].is_wav) run_sd_wav_player(&items[sel]);
                else run_sd_txt_reader(&items[sel]);
                first = true;
            }
        }
    }
    free(items);
}

void app_stories_run(int quick_random_idx)
{
    if (quick_random_idx >= 0) {
        int r = quick_random_idx % (BUILTIN_PLAIN_COUNT + 1);
        if (r == BUILTIN_PLAIN_COUNT) run_branching_gamebook();
        else run_builtin_story(r);
        return;
    }

    /* Total shelf entries: 1 AI Story Gacha + 1 Hanzi Notebook + 7 classic stories + 1 branching gamebook + 1 Imported Shelf + 1 Transfer Studio = 12 */
    const int total = BUILTIN_PLAIN_COUNT + 5;
    int sel = 0;
    bool first = true;

    while (1) {
        gfx_clear(GFX_WHITE);
        gfx_draw_top_bar("★ 双龄护眼绘本故事屋 (阅读页按滚轮可点读生字)", g_prof.stars, app_eye_mins_left());

        /* Left preview card */
        gfx_fill_round_rect(20, 54, 250, 372, 16, GFX_GRAY_LIGHT);
        gfx_round_rect(20, 54, 250, 372, 16, 3, GFX_BLACK);
        gfx_fill_round_rect(36, 70, 218, 250, 12, GFX_WHITE);
        gfx_round_rect(36, 70, 218, 250, 12, 2, GFX_BLACK);

        gfx_art_id_t preview_art = ART_KNIGHT_FOREST;
        const char *preview_tag = "全年龄 · 互动分支";
        if (sel == 0) {
            preview_art = ART_GACHA_MAGIC;
            preview_tag = "家庭Wi-Fi · 小智/大模型";
        } else if (sel == 1) {
            preview_art = ART_CARDS_MAGIC;
            preview_tag = "四线拼音 · 楷体田字格点读";
        } else if (sel - 2 < BUILTIN_PLAIN_COUNT) {
            preview_art = k_stories[sel - 2].art;
            preview_tag = k_stories[sel - 2].age_tag;
        } else if (sel == BUILTIN_PLAIN_COUNT + 3) {
            preview_art = ART_MOON_STARS;
            preview_tag = bsp_storage_get_medium_label();
        } else if (sel == BUILTIN_PLAIN_COUNT + 4) {
            preview_art = ART_PARROT_MIC;
            preview_tag = "USB优盘 + 无线配网传书";
        }
        gfx_draw_art(145, 192, 160, preview_art, GFX_BLACK);
        gfx_draw_text_center(145, 342, preview_tag, FONT_16, GFX_BLACK);
        gfx_draw_text_center(145, 374, "✦ 阅读中按滚轮点读生字", FONT_16, GFX_BLACK);

        /* Right scrolling bookshelf (5 visible rows) */
        int win_start = (sel >= 5) ? (sel - 4) : 0;
        for (int row = 0; row < 5 && (win_start + row) < total; row++) {
            int idx = win_start + row;
            int y = 56 + row * 74;
            char label[128];
            if (idx == 0) {
                snprintf(label, sizeof(label), "★ [AI 扭蛋编故事] 滚轮三步生成专属睡前绘本");
            } else if (idx == 1) {
                snprintf(label, sizeof(label), "★ [我的生字卡片盒] 楷体田字格复习与听音找字");
            } else if (idx - 2 < BUILTIN_PLAIN_COUNT) {
                snprintf(label, sizeof(label), "%d. 《%s》 [%s]",
                         idx - 1, k_stories[idx - 2].title, k_stories[idx - 2].age_tag);
            } else if (idx == BUILTIN_PLAIN_COUNT + 2) {
                snprintf(label, sizeof(label), "8. 《森林勇者奇遇记》 [滚轮分支互动书]");
            } else if (idx == BUILTIN_PLAIN_COUNT + 3) {
                snprintf(label, sizeof(label), "9. [外置与AI书架] 已存 AI有声绘本与导入故事");
            } else {
                snprintf(label, sizeof(label), "10. [配网与传书] 家庭Wi-Fi·AI设置·优盘/扫码传书");
            }
            if (idx == sel) {
                gfx_fill_round_rect(286, y, 494, 62, 12, GFX_BLACK);
                gfx_draw_text(306, y + 19, label, FONT_24, GFX_WHITE);
            } else {
                gfx_round_rect(286, y, 494, 62, 12, 2, GFX_BLACK);
                gfx_draw_text(306, y + 19, label, FONT_24, GFX_BLACK);
            }
        }

        gfx_draw_bottom_hints("滚轮挑绘本/生字盒", "按下进入", "返回主页");
        bsp_epd_smart_present(gfx_get_fb(), first);
        first = false;

        bsp_key_t k = bsp_input_wait(30000);
        if (k == KEY_BACK) break;
        else if (k == KEY_PREV) { sel = (sel + total - 1) % total; bsp_audio_sfx(SFX_WHEEL_TICK); }
        else if (k == KEY_NEXT) { sel = (sel + 1) % total; bsp_audio_sfx(SFX_WHEEL_TICK); }
        else if (k == KEY_SHAKE) {
            bsp_audio_sfx(SFX_CONFIRM);
            run_builtin_story(rand() % BUILTIN_PLAIN_COUNT);
            first = true;
        } else if (k == KEY_OK) {
            bsp_audio_sfx(SFX_CONFIRM);
            if (sel == 0) app_ai_story_run();
            else if (sel == 1) app_hanzi_notebook_run();
            else if (sel - 2 < BUILTIN_PLAIN_COUNT) run_builtin_story(sel - 2);
            else if (sel == BUILTIN_PLAIN_COUNT + 2) run_branching_gamebook();
            else if (sel == BUILTIN_PLAIN_COUNT + 3) run_sdcard_shelf();
            else app_transfer_hub_run();
            first = true;
        }
        app_check_eye_guard();
    }
}
