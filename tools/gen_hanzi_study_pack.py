#!/usr/bin/env python3
"""
Generates components/bsp_board/assets/hanzi_study_pack.bin containing:
1. Full 20,902 Unicode CJK (0x4E00..0x9FA5) Mandarin Pinyin index + tone metadata
2. 52 shared-baseline 24x46 Pinyin glyphs for textbook 四线三格 (4-line 3-grid) rulings
3. ~3,780 native 104x104 Kaiti SC (华文楷体) 1bpp bitmaps for textbook 田字格 magnification
4. ~1,386 offline Mandarin teacher voice syllables (macOS Tingting 16kHz -> Opus VOIP)
"""
import ctypes
import glob
import os
import re
import struct
import subprocess
import tempfile
import time
import wave
from PIL import Image, ImageDraw, ImageFont

ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
OUT_PATH = os.path.join(ROOT_DIR, 'components', 'bsp_board', 'assets', 'hanzi_study_pack.bin')

# Locate Kaiti SC Regular for textbook 田字格 strokes
KAITI_CANDIDATES = (
    glob.glob('/System/Library/AssetsV2/com_apple_MobileAsset_Font*/*/AssetData/Kaiti.ttc') +
    glob.glob('/System/Library/AssetsV2/com_apple_MobileAsset_Font*/*/AssetData/BiauKai.ttc') +
    ['/System/Library/Fonts/Supplemental/Songti.ttc', '/System/Library/Fonts/Hiragino Sans GB.ttc']
)
KAITI_FONT_PATH = next(p for p in KAITI_CANDIDATES if os.path.exists(p))
PINYIN_FONT_PATH = '/System/Library/Fonts/Hiragino Sans GB.ttc'
LIBOPUS_PATH = '/opt/homebrew/opt/opus/lib/libopus.dylib'

TONE_MAP = {
    'ā': 1, 'á': 2, 'ǎ': 3, 'à': 4,
    'ē': 1, 'é': 2, 'ě': 3, 'è': 4,
    'ī': 1, 'í': 2, 'ǐ': 3, 'ì': 4,
    'ō': 1, 'ó': 2, 'ǒ': 3, 'ò': 4,
    'ū': 1, 'ú': 2, 'ǔ': 3, 'ù': 4,
    'ǖ': 1, 'ǘ': 2, 'ǚ': 3, 'ǜ': 4,
}

# Clean monophonic representative characters so isolated TTS always speaks the exact target syllable/tone
MONO_REP_OVERRIDES = {
    'cháng': '常', 'zhǎng': '掌', 'zhàng': '丈', 'chǎng': '厂',
    'yuè': '月', 'lè': '肋', 'jiào': '叫', 'jué': '绝', 'jiāo': '交',
    'xíng': '形', 'háng': '航', 'shǔ': '鼠', 'shù': '树',
    'hǎo': '好', 'hào': '号', 'zhòng': '仲', 'chóng': '虫', 'zhōng': '钟', 'zhǒng': '肿',
    'shǎo': '少', 'shào': '绍', 'zhī': '汁', 'zhǐ': '纸', 'zhì': '志',
    'fā': '发', 'fà': '珐', 'kàn': '看', 'kān': '刊',
    'méi': '梅', 'mò': '沫', 'wèi': '卫', 'wéi': '围',
    'gān': '甘', 'gàn': '赣', 'chuán': '船', 'zhuàn': '赚', 'zhuǎn': '转',
    'kōng': '空', 'kòng': '控', 'bēi': '杯', 'bèi': '倍',
    'xīng': '星', 'xìng': '姓', 'nán': '男', 'nàn': '捺',
    'jiǎ': '甲', 'jià': '嫁', 'fēn': '芬', 'fèn': '奋',
    'shān': '山', 'shàn': '善', 'dōu': '兜', 'dū': '督',
    'hái': '孩', 'huán': '环', 'de': '的', 'dì': '弟', 'dé': '德',
    'zhe': '着', 'zháo': '灼', 'zhuó': '桌', 'le': '了', 'liǎo': '僚',
    'ma': '吗', 'ne': '呢', 'ba': '吧', 'ya': '呀', 'wa': '哇',
    'cān': '餐', 'shēn': '深', 'cēn': '岑', 'chā': '插', 'chà': '岔', 'chāi': '拆',
    'piào': '票', 'piāo': '飘', 'piǎo': '瞟', 'dǎo': '岛', 'dào': '到',
    'hē': '喝', 'hè': '贺', 'quān': '圈', 'juàn': '倦', 'chù': '触', 'chǔ': '楚',
    'qǔ': '取', 'qū': '区', 'dá': '达', 'dā': '搭', 'gǔ': '古',
    'tǔ': '土', 'tù': '兔', 'dǒu': '抖', 'dòu': '豆', 'cáng': '藏', 'zàng': '葬',
}

POLYPHONE_AVOID = set("长行重乐觉着数还都得地为中少好教切种只干系便传弹降露盛调参差朝称乘处大担当倒的度发缝更供冠龟哈哄哗会奇纪夹假间将角脚结解禁尽劲看壳可空落率论埋蒙模摩抹难宁弄迫胖炮喷片漂仆强悄翘曲圈雀散扫色塞煞扇折舍什识似宿踏汤提挑帖吐吓鲜相削血咽要叶应佣涌与载脏藏扎轧占涨折正挣只中种轴属转幢综钻")

def detect_tone(py: str) -> int:
    for ch in py:
        if ch in TONE_MAP:
            return TONE_MAP[ch]
    return 5

def extract_all_cjk_pinyin():
    cf = ctypes.cdll.LoadLibrary('/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation')
    cf.CFStringCreateMutableCopy.restype = ctypes.c_void_p
    cf.CFStringCreateMutableCopy.argtypes = [ctypes.c_void_p, ctypes.c_long, ctypes.c_void_p]
    cf.CFStringCreateWithCString.restype = ctypes.c_void_p
    cf.CFStringCreateWithCString.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_uint32]
    cf.CFStringTransform.restype = ctypes.c_bool
    cf.CFStringTransform.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_bool]
    cf.CFStringGetCString.restype = ctypes.c_bool
    cf.CFStringGetCString.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_long, ctypes.c_uint32]
    cf.CFRelease.argtypes = [ctypes.c_void_p]
    kUTF8 = 0x08000100
    tr = cf.CFStringCreateWithCString(None, b'Any-Latin', kUTF8)
    buf = ctypes.create_string_buffer(64)

    # Child-friendly default single-char readings for common polyphones
    CHAR_DEFAULT_PY = {
        '长': 'cháng', '乐': 'lè', '觉': 'jué', '着': 'zhe', '数': 'shù',
        '行': 'xíng', '还': 'hái', '得': 'dé', '地': 'dì', '的': 'de',
        '了': 'le', '吗': 'ma', '呢': 'ne', '吧': 'ba', '呀': 'ya',
        '都': 'dōu', '为': 'wèi', '中': 'zhōng', '少': 'shǎo', '只': 'zhī',
        '重': 'zhòng', '发': 'fā', '教': 'jiāo', '空': 'kōng', '种': 'zhǒng',
        '背': 'bèi', '假': 'jiǎ', '分': 'fēn', '转': 'zhuǎn', '干': 'gān',
        '难': 'nán', '兴': 'xīng', '当': 'dāng', '正': 'zhèng', '相': 'xiāng',
        '传': 'chuán', '弹': 'tán', '奇': 'qí', '落': 'luò', '便': 'biàn',
        '更': 'gèng', '结': 'jié', '切': 'qiē', '应': 'yīng', '朝': 'cháo',
        '扇': 'shàn', '漂': 'piào', '倒': 'dào', '喝': 'hē', '圈': 'quān',
        '处': 'chù', '答': 'dá', '吐': 'tǔ', '藏': 'cáng', '龟': 'guī',
    }

    cjk_pinyins = []
    for cp in range(0x4E00, 0x9FA6):
        ch = chr(cp)
        if ch in CHAR_DEFAULT_PY:
            py = CHAR_DEFAULT_PY[ch]
        else:
            s = cf.CFStringCreateWithCString(None, ch.encode('utf-8'), kUTF8)
            m = cf.CFStringCreateMutableCopy(None, 0, s)
            cf.CFStringTransform(m, None, tr, False)
            cf.CFStringGetCString(m, buf, 64, kUTF8)
            py = buf.value.decode('utf-8').strip().split()[0].lower()
            cf.CFRelease(m)
            cf.CFRelease(s)
        cjk_pinyins.append(py)
    cf.CFRelease(tr)

    # Ensure extra polyphone target syllables used in C context rules exist in syllable table
    extra_context_syls = [
        'zhǎng', 'cháng', 'yuè', 'lè', 'jiào', 'jué', 'zhe', 'zháo', 'zhuó',
        'shǔ', 'shù', 'háng', 'xíng', 'hǎo', 'hào', 'huán', 'hái', 'dū', 'dōu',
        'zhǒng', 'zhòng', 'chóng', 'kōng', 'kòng', 'dì', 'de', 'děi', 'dé',
        'jiǎ', 'jià', 'fēn', 'fèn', 'zhuǎn', 'zhuàn', 'gān', 'gàn', 'nán', 'nàn',
        'xīng', 'xìng', 'shān', 'shàn', 'piào', 'piāo', 'dǎo', 'dào', 'hē', 'hè',
        'quān', 'juàn', 'chù', 'chǔ', 'cáng', 'zàng', 'tǔ', 'tù', 'bēi', 'bèi',
        'fā', 'fà', 'kàn', 'kān', 'méi', 'mò', 'wèi', 'wéi', 'zhī', 'zhǐ',
    ]
    all_syls = sorted(set(cjk_pinyins) | set(extra_context_syls))
    syl_to_id = {py: i for i, py in enumerate(all_syls)}

    # Build GB2312 Level-1 set (3,755 everyday standard characters)
    gb_level1 = set()
    for qu in range(16, 56):
        for wei in range(1, 95):
            if qu == 55 and wei > 89:
                continue
            try:
                c = bytes([0xA0 + qu, 0xA0 + wei]).decode('gb2312')
                if len(c) == 1 and 0x4E00 <= ord(c) <= 0x9FA5:
                    gb_level1.add(c)
            except Exception:
                pass

    # Choose best monophonic representative character for each syllable
    syl_rep = {}
    # Pass 1: GB2312 Level-1 non-polyphone characters
    for cp in range(0x4E00, 0x9FA6):
        ch = chr(cp)
        py = cjk_pinyins[cp - 0x4E00]
        if py not in syl_rep and ch in gb_level1 and ch not in POLYPHONE_AVOID:
            syl_rep[py] = ch
    # Pass 2: Any non-polyphone character
    for cp in range(0x4E00, 0x9FA6):
        ch = chr(cp)
        py = cjk_pinyins[cp - 0x4E00]
        if py not in syl_rep and ch not in POLYPHONE_AVOID:
            syl_rep[py] = ch
    # Pass 3: Fallback any character
    for cp in range(0x4E00, 0x9FA6):
        ch = chr(cp)
        py = cjk_pinyins[cp - 0x4E00]
        if py not in syl_rep:
            syl_rep[py] = ch
    # Pass 4: Explicit verified overrides
    for py, ch in MONO_REP_OVERRIDES.items():
        if py in syl_to_id:
            syl_rep[py] = ch
    for py in all_syls:
        if py not in syl_rep:
            syl_rep[py] = '得' if py == 'děi' else '啊'

    cjk_syl_ids = [syl_to_id[py] for py in cjk_pinyins]
    return all_syls, cjk_syl_ids, syl_rep, gb_level1

class LibOpusEncoder:
    def __init__(self, sr=16000, bitrate=15000):
        self.lib = ctypes.CDLL(LIBOPUS_PATH)
        self.lib.opus_encoder_create.restype = ctypes.c_void_p
        self.lib.opus_encoder_create.argtypes = [ctypes.c_int32, ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_int)]
        self.lib.opus_encoder_ctl.restype = ctypes.c_int
        self.lib.opus_encode.restype = ctypes.c_int32
        self.lib.opus_encode.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_int16), ctypes.c_int, ctypes.c_char_p, ctypes.c_int32]
        self.lib.opus_encoder_destroy.argtypes = [ctypes.c_void_p]
        self.lib.opus_encoder_ctl(None, 0) if False else None
        err = ctypes.c_int(0)
        # OPUS_APPLICATION_VOIP = 2048
        self.enc = self.lib.opus_encoder_create(sr, 1, 2048, ctypes.byref(err))
        assert self.enc and err.value == 0, f"opus_encoder_create failed: {err.value}"
        # OPUS_SET_BITRATE_REQUEST = 4002, OPUS_SET_VBR_REQUEST = 4006, OPUS_SET_COMPLEXITY_REQUEST = 4010
        self.lib.opus_encoder_ctl(ctypes.c_void_p(self.enc), 4002, ctypes.c_int32(bitrate))
        self.lib.opus_encoder_ctl(ctypes.c_void_p(self.enc), 4006, ctypes.c_int32(1))

    def reset_state(self):
        # OPUS_RESET_STATE = 4028
        self.lib.opus_encoder_ctl(ctypes.c_void_p(self.enc), 4028)

    def encode_clip(self, samples_i16):
        self.reset_state()
        # Normalize peak to ~23000 for crisp, uniform teacher pronunciation volume
        peak = max((abs(x) for x in samples_i16), default=1)
        if peak > 200:
            gain = min(3.2, 23000.0 / peak)
            samples_i16 = [max(-32767, min(32767, int(x * gain))) for x in samples_i16]

        # Cap max syllable length to 0.56s (8960 samples) + gentle 20ms tail fade
        if len(samples_i16) > 8960:
            samples_i16 = samples_i16[:8960]
        fade_n = min(320, len(samples_i16))
        for i in range(fade_n):
            samples_i16[-fade_n + i] = int(samples_i16[-fade_n + i] * (fade_n - i) / fade_n)

        frame_sz = 320  # 20ms @ 16kHz
        while len(samples_i16) % frame_sz != 0 or len(samples_i16) == 0:
            samples_i16.append(0)

        out = bytearray()
        pkt_buf = ctypes.create_string_buffer(256)
        arr_t = ctypes.c_int16 * frame_sz
        for off in range(0, len(samples_i16), frame_sz):
            frm = arr_t(*samples_i16[off:off + frame_sz])
            nbytes = self.lib.opus_encode(self.enc, frm, frame_sz, pkt_buf, 250)
            assert 1 <= nbytes <= 250
            out.append(nbytes)
            out.extend(pkt_buf.raw[:nbytes])
        return bytes(out)

    def close(self):
        if self.enc:
            self.lib.opus_encoder_destroy(self.enc)
            self.enc = None

def split_wav_segments(wav_path, thresh=110, min_sil=2200):
    with wave.open(wav_path, 'rb') as w:
        n = w.getnframes()
        pcm = struct.unpack(f'<{n}h', w.readframes(n))
    segs = []
    in_seg = False
    start = 0
    last_loud = 0
    for i, s in enumerate(pcm):
        if abs(s) > thresh:
            if not in_seg:
                start = max(0, i - 240)  # keep 15ms consonant attack!
                in_seg = True
            last_loud = i
        elif in_seg and (i - last_loud) >= min_sil:
            end = min(n, last_loud + 480)  # keep 30ms natural tail
            segs.append(list(pcm[start:end]))
            in_seg = False
    if in_seg:
        segs.append(list(pcm[start:min(n, last_loud + 480)]))
    return segs

def synth_single_char_pcm(ch, tmp_dir):
    wpath = os.path.join(tmp_dir, 'single.wav')
    subprocess.run(['say', '-v', 'Tingting', '-r', '175', '-o', wpath, '--data-format=LEI16@16000', ch], check=True)
    segs = split_wav_segments(wpath, thresh=90, min_sil=1600)
    if segs:
        # Concatenate if internal pause split a dipping 3rd tone
        merged = []
        for s in segs:
            merged.extend(s)
        return merged
    return [0] * 1600

def synthesize_all_syllables_opus(all_syls, syl_rep):
    enc = LibOpusEncoder(sr=16000, bitrate=15000)
    opus_clips = []
    BATCH = 45
    with tempfile.TemporaryDirectory() as tmp_dir:
        for b0 in range(0, len(all_syls), BATCH):
            batch_syls = all_syls[b0:b0 + BATCH]
            batch_chars = [syl_rep[py] for py in batch_syls]
            script = ' [[slnc 340]] '.join(batch_chars)
            wpath = os.path.join(tmp_dir, 'batch.wav')
            subprocess.run(['say', '-v', 'Tingting', '-r', '175', '-o', wpath, '--data-format=LEI16@16000', script], check=True)
            segs = split_wav_segments(wpath, thresh=110, min_sil=2600)
            if len(segs) == len(batch_syls):
                for seg in segs:
                    opus_clips.append(enc.encode_clip(seg))
            else:
                # 100% deterministic fallback if any 3rd-tone glottal stop split inside a syllable
                for ch in batch_chars:
                    seg = synth_single_char_pcm(ch, tmp_dir)
                    opus_clips.append(enc.encode_clip(seg))
    enc.close()
    return opus_clips

def build_pinyin_4line_glyphs():
    """
    Renders 52 Pinyin characters onto a 24x46 canvas with shared typographic baseline at y=32:
      Line 1 (top): y=2
      Line 2 (x-height top): y=15
      Line 3 (baseline): y=32
      Line 4 (bottom descender): y=45
    """
    chars = sorted(set("abcdefghijklmnopqrstuvwxyzāáǎàēéěèīíǐìōóǒòūúǔùǖǘǚǜü'"))
    hira_w6_idx = 2 if 'Hiragino' in PINYIN_FONT_PATH else 0
    font = ImageFont.truetype(PINYIN_FONT_PATH, 25, index=hira_w6_idx)
    W, H = 24, 46
    row_bytes = W // 8  # 3 bytes * 46 = 138 bytes/glyph
    cps = []
    advs = []
    bmps = bytearray()
    for ch in chars:
        img = Image.new('L', (W, H), 255)
        d = ImageDraw.Draw(img)
        bb = d.textbbox((2, 32), ch, font=font, anchor='ls')
        adv = max(10, min(22, (bb[2] - bb[0]) + 2))
        x_draw = max(1, (adv - (bb[2] - bb[0])) // 2 - (bb[0] - 2) + 1)
        d.text((x_draw, 32), ch, font=font, fill=0, anchor='ls')
        px = img.load()
        out = bytearray(row_bytes * H)
        for ry in range(H):
            for rx in range(W):
                if px[rx, ry] < 164:
                    out[ry * row_bytes + (rx >> 3)] |= (0x80 >> (rx & 7))
        cps.append(ord(ch))
        advs.append(adv)
        bmps.extend(out)
    return cps, advs, bytes(bmps)

def collect_project_story_chars():
    extra = set()
    scan_paths = (
        glob.glob(os.path.join(ROOT_DIR, 'main', '*.c')) +
        glob.glob(os.path.join(ROOT_DIR, 'sdcard_starter_pack', 'stories', '*.txt'))
    )
    for p in scan_paths:
        try:
            txt = open(p, 'r', encoding='utf-8', errors='ignore').read()
            for ch in txt:
                if 0x4E00 <= ord(ch) <= 0x9FA5:
                    extra.add(ch)
        except Exception:
            pass
    return extra

def render_kaiti_104_glyphs(char_set):
    DIM = 104
    row_bytes = (DIM + 7) // 8  # 13 bytes * 104 = 1,352 bytes/glyph
    # Use Kaiti SC Bold (index=3 in Kaiti.ttc) for crisp, high-contrast textbook strokes!
    try:
        font = ImageFont.truetype(KAITI_FONT_PATH, 94, index=3 if 'Kaiti.ttc' in KAITI_FONT_PATH else 0)
    except Exception:
        font = ImageFont.truetype(KAITI_FONT_PATH, 94)
    sorted_cps = sorted(ord(c) for c in char_set if 0x4E00 <= ord(c) <= 0x9FA5)
    bmps = bytearray()
    for cp in sorted_cps:
        ch = chr(cp)
        img = Image.new('L', (DIM, DIM), 255)
        d = ImageDraw.Draw(img)
        bb = d.textbbox((0, 0), ch, font=font)
        gw = bb[2] - bb[0]
        gh = bb[3] - bb[1]
        x = (DIM - gw) // 2 - bb[0]
        y = (DIM - gh) // 2 - bb[1]
        d.text((x, y), ch, font=font, fill=0)
        px = img.load()
        out = bytearray(row_bytes * DIM)
        for ry in range(DIM):
            for rx in range(DIM):
                if px[rx, ry] < 165:
                    out[ry * row_bytes + (rx >> 3)] |= (0x80 >> (rx & 7))
        bmps.extend(out)
    return sorted_cps, bytes(bmps), DIM

def align4(b: bytearray):
    rem = len(b) & 3
    if rem:
        b.extend(b'\x00' * (4 - rem))

def try_reuse_existing_opus(out_path, expected_syls):
    if not os.path.exists(out_path):
        return None
    try:
        data = open(out_path, 'rb').read()
        if len(data) < 64 or data[:4] != b'HZPK':
            return None
        vals = struct.unpack('<4sHHHH8I20x', data[:64])
        syl_count = vals[1]
        off_syl_meta = vals[5]
        off_opus_blob = vals[12]
        if syl_count != len(expected_syls):
            return None
        clips = []
        for i in range(syl_count):
            py_b, tone, _, aoff, abytes = struct.unpack_from('<12sBBIH', data, off_syl_meta + i * 20)
            py = py_b.split(b'\x00', 1)[0].decode('utf-8')
            if py != expected_syls[i]:
                return None
            clips.append(data[off_opus_blob + aoff : off_opus_blob + aoff + abytes])
        return clips
    except Exception:
        return None

def main():
    t0 = time.time()
    print(f"[1/4] Extracting 20,902 CJK Pinyin & GB2312 Level-1 set...")
    all_syls, cjk_syl_ids, syl_rep, gb_level1 = extract_all_cjk_pinyin()
    print(f"      {len(all_syls)} unique syllables, {len(gb_level1)} GB2312 Level-1 chars.")

    opus_clips = try_reuse_existing_opus(OUT_PATH, all_syls)
    if opus_clips is not None:
        print(f"[2/4] Reused {len(opus_clips)} verified Tingting 16kHz Opus clips from existing pack ({sum(len(c) for c in opus_clips)/1024:.1f} KB).")
    else:
        print(f"[2/4] Synthesizing {len(all_syls)} Mandarin syllables with macOS Tingting -> 16kHz Opus...")
        t_tts = time.time()
        opus_clips = synthesize_all_syllables_opus(all_syls, syl_rep)
        print(f"      Synthesized & Opus-encoded {len(opus_clips)} syllables in {time.time()-t_tts:.1f}s "
              f"(total {sum(len(c) for c in opus_clips)/1024:.1f} KB).")

    print(f"[3/4] Rendering 4-line Pinyin glyphs & 104x104 Kaiti SC ({os.path.basename(KAITI_FONT_PATH)}) glyphs...")
    py_cps, py_advs, py_bmps = build_pinyin_4line_glyphs()
    kaiti_chars = gb_level1 | collect_project_story_chars()
    kaiti_cps, kaiti_bmps, kaiti_dim = render_kaiti_104_glyphs(kaiti_chars)
    print(f"      Rendered {len(kaiti_cps)} native {kaiti_dim}x{kaiti_dim} Kaiti glyphs ({len(kaiti_bmps)/1024:.1f} KB).")

    print(f"[4/4] Packing {OUT_PATH}...")
    # Buildopus blob and syllable metadata (20 bytes each: 12s B B I H)
    opus_blob = bytearray()
    syl_meta = bytearray()
    for py, clip in zip(all_syls, opus_clips):
        py_bytes = py.encode('utf-8')[:11]
        tone = detect_tone(py)
        off_in_blob = len(opus_blob)
        opus_blob.extend(clip)
        syl_meta.extend(struct.pack('<12sBBIH', py_bytes, tone, 0, off_in_blob, len(clip)))

    cjk_idx_sec = struct.pack(f'<{len(cjk_syl_ids)}H', *cjk_syl_ids)
    py_cps_sec = struct.pack(f'<{len(py_cps)}H', *py_cps)
    py_adv_sec = struct.pack(f'<{len(py_advs)}B', *py_advs)
    kaiti_cps_sec = struct.pack(f'<{len(kaiti_cps)}H', *kaiti_cps)

    sections = [
        syl_meta,
        cjk_idx_sec,
        py_cps_sec,
        py_adv_sec,
        py_bmps,
        kaiti_cps_sec,
        kaiti_bmps,
        bytes(opus_blob),
    ]

    payload = bytearray()
    offsets = []
    HDR_SIZE = 64
    for sec in sections:
        align4(payload)
        offsets.append(HDR_SIZE + len(payload))
        payload.extend(sec)
    align4(payload)

    # Header (64 bytes): 4s H H H H 8I 16x = 4 + 8 + 32 + 20 = 64
    hdr = struct.pack(
        '<4sHHHH8I20x',
        b'HZPK',
        len(all_syls),
        len(py_cps),
        len(kaiti_cps),
        kaiti_dim,
        *offsets
    )
    assert len(hdr) == 64

    os.makedirs(os.path.dirname(OUT_PATH), exist_ok=True)
    with open(OUT_PATH, 'wb') as f:
        f.write(hdr)
        f.write(payload)

    print(f"Done in {time.time()-t0:.1f}s! Generated {OUT_PATH}: {os.path.getsize(OUT_PATH)/1024/1024:.2f} MB")

if __name__ == '__main__':
    main()
