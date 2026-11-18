#!/usr/bin/env python3
import os, struct
from PIL import Image, ImageDraw, ImageFont

FONT_CANDIDATES = [
    '/System/Library/Fonts/Hiragino Sans GB.ttc',
    '/System/Library/Fonts/STHeiti Medium.ttc',
]
FONT_PATH = next(p for p in FONT_CANDIDATES if os.path.exists(p))
SYM_FONT_CANDIDATES = [
    '/System/Library/Fonts/Apple Symbols.ttf',
    '/System/Library/Fonts/Supplemental/Arial Unicode.ttf',
    '/System/Library/Fonts/STHeiti Medium.ttc',
    FONT_PATH,
]
SYM_FONT_PATH = next(p for p in SYM_FONT_CANDIDATES if os.path.exists(p))

# Extra non-ASCII, non-4E00..9FA5 symbols (Pinyin vowels, CJK punctuation, game icons)
EXTRA_CHARS = (
    "āáǎàēéěèīíǐìōóǒòūúǔùǖǘǚǜ"
    "ĀÁǍÀĒÉĚÈĪÍǏÌŌÓǑÒŪÚǓÙ"
    "，。！？、；：“”‘’《》（）【】—…·～"
    "＋－×÷＝≠＜＞％℃°"
    "★☆♥♪♫●○◆◇▲▼◀▶■□✓✗↑↓←→☾♛⚡↺✦⚙"
)

# Frequently used 32x32 headline/flashcard/UI Chinese characters (~650 chars)
TITLE32_CHARS = sorted(set(
    "墨芽童伴绘本故事屋传书工坊优盘热点扫码无线连接手机电脑拖拽完成弹出启蒙游乐园益智挑战营变声小鹦鹉星星荣誉墙家长护航站"
    "魔法扭蛋生成专属睡前联网配路由器小智验证码绑定宝贝自己主角场景主题历史匣云端离线语音伴读成功失败重试"
    "小兔子乖乖龟兔赛跑小蝌蚪找妈妈三只小猪盖房子曹冲称象司马光砸缸为什么月亮会变弯森林勇者奇遇记"
    "滚轮配配乐海底潜艇接数字摩天轮翻转记忆牌弹球打砖块密码锁猜猜看"
    "上下左右确认返回开始继续暂停重玩退出过关胜利恭喜加油真棒再来一次"
    "学龄前儿童岁幼小衔接加减乘除得分最高纪录第页章卷目录书签音量静音护眼休息设置睡眠待机时钟"
    "原声花栗鼠大狗熊录音播放停止麦克风说话电量充电温湿度舒适偏干偏湿冷热"
    "一二三四五六七八九十百千万零大小多少上下左右前后里外日月水火木金土山石田禾人口目耳手足"
    "春夏秋冬风云雨雪花鸟鱼虫牛羊马犬猫兔象熊虎狮猴蛙龟松鼠胡萝卜竹子骨头青草害虫香蕉松果"
    "红黄蓝绿白黑金银爱笑哭跑跳飞游唱听看读想问猜选打开关闭安全时长屏幕旋转反转关机测试卡"
))

def render_bitmap_1bpp(ch, w, h, font, y_shift=0, thresh=160):
    img = Image.new('L', (w, h), 255)
    drw = ImageDraw.Draw(img)
    bbox = drw.textbbox((0, 0), ch, font=font)
    gw = bbox[2] - bbox[0]
    gh = bbox[3] - bbox[1]
    x = (w - gw) // 2 - bbox[0]
    y = (h - gh) // 2 - bbox[1] + y_shift
    drw.text((x, y), ch, font=font, fill=0)
    row_bytes = (w + 7) // 8
    out = bytearray(row_bytes * h)
    px = img.load()
    for ry in range(h):
        for rx in range(w):
            # 1 = ink (black pixel), 0 = background
            if px[rx, ry] < thresh:
                out[ry * row_bytes + (rx >> 3)] |= (0x80 >> (rx & 7))
    return bytes(out)

def build_pack(out_path):
    # Use SemiBold (Hiragino W6, index=2) for crisp, high-contrast 1bpp E-Ink strokes!
    # For 16px small CJK, use Heiti SC Medium (index=1) with higher threshold so strokes never break nor blob.
    hira_w6_idx = 2 if 'Hiragino' in FONT_PATH else 0
    heiti_p = '/System/Library/Fonts/STHeiti Medium.ttc'
    f16_en = ImageFont.truetype(FONT_PATH, 14, index=hira_w6_idx)
    f24_en = ImageFont.truetype(FONT_PATH, 20, index=hira_w6_idx)
    f32_en = ImageFont.truetype(FONT_PATH, 27, index=hira_w6_idx)
    if os.path.exists(heiti_p):
        f16_zh = ImageFont.truetype(heiti_p, 15, index=1)
    else:
        f16_zh = ImageFont.truetype(FONT_PATH, 15, index=hira_w6_idx)
    f24_zh = ImageFont.truetype(FONT_PATH, 22, index=hira_w6_idx)
    f32_zh = ImageFont.truetype(FONT_PATH, 29, index=hira_w6_idx)
    f16_sym = ImageFont.truetype(SYM_FONT_PATH, 16)
    f24_sym = ImageFont.truetype(SYM_FONT_PATH, 24)

    ascii16 = bytearray()
    ascii24 = bytearray()
    ascii32 = bytearray()
    for code in range(0x20, 0x7F):
        ch = chr(code)
        ascii16.extend(render_bitmap_1bpp(ch, 8, 16, f16_en, thresh=168))
        ascii24.extend(render_bitmap_1bpp(ch, 12, 24, f24_en, thresh=162))
        ascii32.extend(render_bitmap_1bpp(ch, 16, 32, f32_en, thresh=160))

    sym_cps = sorted(set(ord(c) for c in EXTRA_CHARS if not (0x20 <= ord(c) <= 0x7E) and not (0x4E00 <= ord(c) <= 0x9FA5)))
    sym_idx = struct.pack(f'<{len(sym_cps)}H', *sym_cps)
    sym16 = bytearray()
    sym24 = bytearray()
    for cp in sym_cps:
        ch = chr(cp)
        is_icon = (0x2190 <= cp <= 0x27BF)
        sym16.extend(render_bitmap_1bpp(ch, 16, 16, f16_sym if is_icon else f16_zh, thresh=172))
        sym24.extend(render_bitmap_1bpp(ch, 24, 24, f24_sym if is_icon else f24_zh, thresh=162))

    t32_cps = sorted(set(ord(c) for c in TITLE32_CHARS if 0x4E00 <= ord(c) <= 0x9FA5 or ord(c) in sym_cps))
    t32_idx = struct.pack(f'<{len(t32_cps)}H', *t32_cps)
    t32_bmp = bytearray()
    for cp in t32_cps:
        t32_bmp.extend(render_bitmap_1bpp(chr(cp), 32, 32, f32_zh, thresh=156))

    cjk16 = bytearray()
    cjk24 = bytearray()
    for cp in range(0x4E00, 0x9FA6):
        ch = chr(cp)
        cjk16.extend(render_bitmap_1bpp(ch, 16, 16, f16_zh, thresh=176))
        cjk24.extend(render_bitmap_1bpp(ch, 24, 24, f24_zh, thresh=156))

    # Header: magic(4s), sym_count(H), t32_count(H), 9 section offsets (9I) = 4 + 4 + 36 = 44 bytes (pad to 48)
    sections = [ascii16, ascii24, ascii32, sym_idx, sym16, sym24, t32_idx, t32_bmp, cjk16, cjk24]
    off = 64
    offsets = []
    for s in sections:
        offsets.append(off)
        off += (len(s) + 3) & ~3  # 4-byte aligned

    hdr = struct.pack('<4sHH10I16x', b'KIDF', len(sym_cps), len(t32_cps), *offsets)
    assert len(hdr) == 64

    with open(out_path, 'wb') as f:
        f.write(hdr)
        for s in sections:
            f.write(s)
            pad = ((len(s) + 3) & ~3) - len(s)
            if pad:
                f.write(b'\x00' * pad)
    print(f'Generated {out_path}: {os.path.getsize(out_path)} bytes, syms={len(sym_cps)}, t32={len(t32_cps)}')

if __name__ == '__main__':
    out = os.path.join(os.path.dirname(__file__), '..', 'components', 'bsp_board', 'assets', 'kidfont_pack.bin')
    build_pack(os.path.abspath(out))
