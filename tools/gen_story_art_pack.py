#!/usr/bin/env python3
"""
Generates components/bsp_board/assets/story_art_pack.bin containing:
1. 32 refined storybook & UI illustrations in 3 native crisp 1bpp sizes:
   - 64x64   (512 B each)  — Game sprites & Memory Flip cards
   - 96x96   (1,152 B each) — Home Launcher 6 cards & AI Gacha 3 reels
   - 176x176 (3,872 B each) — Story Left Panels, Safari Match, Hubs & Sleep Clock
2. 11 giant 38x68 Bold Clock Numerals ('0'..'9', ':') for the Bedside Alarm Clock screen!
"""
import os
import struct
from PIL import Image, ImageDraw, ImageFont, ImageFilter, ImageOps

ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
OUT_BIN = os.path.join(ROOT_DIR, 'components', 'bsp_board', 'assets', 'story_art_pack.bin')
PREVIEW_PNG = os.path.join(ROOT_DIR, 'docs', 'icons_showcase.png')

EMOJI_FONT = '/System/Library/Fonts/Apple Color Emoji.ttc'
CLOCK_FONT = '/System/Library/Fonts/Hiragino Sans GB.ttc'

# Note: Apple Color Emoji.ttc supports exact bitmap strikes: 20, 32, 40, 48, 64, 96, 160!
F_EM_160 = ImageFont.truetype(EMOJI_FONT, 160)
F_EM_96  = ImageFont.truetype(EMOJI_FONT, 96)
F_EM_64  = ImageFont.truetype(EMOJI_FONT, 64)
F_EM_48  = ImageFont.truetype(EMOJI_FONT, 48)

def paste_emoji_centered(canvas_rgba, em_str, cx, cy, font):
    tmp = Image.new('RGBA', (220, 220), (0, 0, 0, 0))
    d = ImageDraw.Draw(tmp)
    bb = d.textbbox((0, 0), em_str, font=font, embedded_color=True)
    w, h = bb[2] - bb[0], bb[3] - bb[1]
    tx = (220 - w) // 2 - bb[0]
    ty = (220 - h) // 2 - bb[1]
    d.text((tx, ty), em_str, font=font, embedded_color=True)
    crop = tmp.crop(((220 - w) // 2, (220 - h) // 2, (220 - w) // 2 + w, (220 - h) // 2 + h))
    canvas_rgba.alpha_composite(crop, (int(cx - w // 2), int(cy - h // 2)))

def draw_sparkle(d, cx, cy, r, w=3):
    d.line([(cx - r, cy), (cx + r, cy)], fill=(20, 20, 20, 255), width=w)
    d.line([(cx, cy - r), (cx, cy + r)], fill=(20, 20, 20, 255), width=w)

def build_scene_rgba_200(art_name):
    """Compose a rich 200x200 RGBA storybook scene before 1bpp woodcut halftoning."""
    c = Image.new('RGBA', (200, 200), (255, 255, 255, 0))
    d = ImageDraw.Draw(c)

    if art_name == 'ART_RABBIT':
        paste_emoji_centered(c, '🐰', 94, 96, F_EM_160)
        paste_emoji_centered(c, '🥕', 156, 150, F_EM_64)
    elif art_name == 'ART_TURTLE':
        paste_emoji_centered(c, '🐢', 100, 102, F_EM_160)
    elif art_name == 'ART_TADPOLE_FROG':
        paste_emoji_centered(c, '🐸', 100, 92, F_EM_160)
        # Cute water ripples below frog
        d.arc([28, 166, 172, 190], 0, 180, fill=(30, 30, 30, 255), width=4)
        d.arc([56, 174, 144, 194], 0, 180, fill=(30, 30, 30, 255), width=3)
    elif art_name == 'ART_THREE_PIGS':
        paste_emoji_centered(c, '🏡', 118, 92, F_EM_96)
        paste_emoji_centered(c, '🐷', 62, 134, F_EM_96)
    elif art_name == 'ART_ELEPHANT':
        paste_emoji_centered(c, '🐘', 100, 84, F_EM_160)
        # Wooden weighing boat hull & waves below elephant (Cao Chong Weighs Elephant!)
        d.polygon([(20, 152), (180, 152), (154, 182), (46, 182)], fill=(150, 150, 150, 255), outline=(15, 15, 15, 255), width=5)
        d.line([(36, 166), (164, 166)], fill=(20, 20, 20, 255), width=3)
    elif art_name == 'ART_SIMAGUANG':
        paste_emoji_centered(c, '🏺', 92, 106, F_EM_160)
        paste_emoji_centered(c, '💦', 152, 74, F_EM_64)
    elif art_name == 'ART_MOON_STARS':
        paste_emoji_centered(c, '🌙', 96, 94, F_EM_160)
        paste_emoji_centered(c, '⭐', 152, 48, F_EM_48)
        draw_sparkle(d, 40, 52, 10, 4)
        draw_sparkle(d, 158, 146, 8, 3)
    elif art_name == 'ART_KNIGHT_FOREST':
        paste_emoji_centered(c, '🌲', 66, 98, F_EM_96)
        paste_emoji_centered(c, '🛡️', 128, 114, F_EM_96)
        paste_emoji_centered(c, '⭐', 128, 42, F_EM_48)
    elif art_name == 'ART_SUBMARINE':
        # Crisp custom Kawaii Submarine with portholes, periscope & tropical fish!
        # Periscope
        d.rounded_rectangle([88, 26, 104, 68], radius=6, fill=(140, 140, 140, 255), outline=(15, 15, 15, 255), width=5)
        d.rounded_rectangle([88, 24, 124, 40], radius=6, fill=(140, 140, 140, 255), outline=(15, 15, 15, 255), width=5)
        # Tail fins & propeller
        d.polygon([(16, 92), (44, 104), (44, 128), (16, 140)], fill=(120, 120, 120, 255), outline=(15, 15, 15, 255), width=5)
        # Conning tower
        d.rounded_rectangle([68, 56, 132, 86], radius=12, fill=(175, 175, 175, 255), outline=(15, 15, 15, 255), width=5)
        # Main hull
        d.rounded_rectangle([34, 74, 178, 154], radius=40, fill=(215, 215, 215, 255), outline=(15, 15, 15, 255), width=6)
        d.arc([48, 84, 164, 124], 200, 340, fill=(255, 255, 255, 255), width=6)
        # Two gleaming portholes
        for px in (82, 132):
            d.ellipse([px - 18, 96, px + 18, 132], fill=(255, 255, 255, 255), outline=(15, 15, 15, 255), width=5)
            d.ellipse([px - 9, 105, px + 9, 123], fill=(40, 40, 40, 255))
            d.ellipse([px - 6, 107, px - 1, 112], fill=(255, 255, 255, 255))
        # Bubbles
        d.ellipse([164, 44, 182, 62], fill=(255, 255, 255, 255), outline=(15, 15, 15, 255), width=4)
        d.ellipse([148, 62, 160, 74], fill=(255, 255, 255, 255), outline=(15, 15, 15, 255), width=3)
    elif art_name == 'ART_FERRIS_WHEEL':
        paste_emoji_centered(c, '🎡', 100, 100, F_EM_160)
    elif art_name == 'ART_CARDS_MAGIC':
        # Two cute overlapping rounded magic cards (Moon card + Star card) + sparkles
        d.rounded_rectangle([32, 34, 122, 162], radius=14, fill=(245, 245, 245, 255), outline=(15, 15, 15, 255), width=6)
        paste_emoji_centered(c, '🌙', 74, 96, F_EM_64)
        d.rounded_rectangle([78, 42, 170, 172], radius=14, fill=(255, 255, 255, 255), outline=(15, 15, 15, 255), width=6)
        paste_emoji_centered(c, '⭐', 124, 108, F_EM_64)
        draw_sparkle(d, 162, 26, 9, 4)
        draw_sparkle(d, 24, 152, 7, 3)
    elif art_name == 'ART_BREAKOUT':
        # Crisp arcade bricks + comet ball + rounded paddle
        for r in range(3):
            for col in range(4):
                bx = 22 + col * 40
                by = 28 + r * 22
                fill_v = 60 if (r + col) % 2 == 0 else 165
                d.rounded_rectangle([bx, by, bx + 35, by + 17], radius=5, fill=(fill_v, fill_v, fill_v, 255), outline=(15, 15, 15, 255), width=4)
        d.ellipse([104, 108, 130, 134], fill=(30, 30, 30, 255), outline=(15, 15, 15, 255), width=4)
        d.ellipse([109, 112, 116, 119], fill=(255, 255, 255, 255))
        d.line([(86, 138), (104, 126)], fill=(80, 80, 80, 255), width=4)
        d.rounded_rectangle([48, 156, 152, 178], radius=11, fill=(140, 140, 140, 255), outline=(15, 15, 15, 255), width=5)
    elif art_name == 'ART_LOCK_TREASURE':
        paste_emoji_centered(c, '🔐', 100, 102, F_EM_160)
    elif art_name == 'ART_PARROT_MIC':
        paste_emoji_centered(c, '🦜', 78, 102, F_EM_160)
        paste_emoji_centered(c, '🎙️', 148, 122, F_EM_96)
    elif art_name == 'ART_TROPHY_STARS':
        paste_emoji_centered(c, '🏆', 100, 102, F_EM_160)
    elif art_name == 'ART_OWL_GUARD':
        paste_emoji_centered(c, '🦉', 100, 102, F_EM_160)
    elif art_name == 'ART_SLEEP_BEAR':
        paste_emoji_centered(c, '🧸', 94, 108, F_EM_160)
        paste_emoji_centered(c, '💤', 152, 46, F_EM_64)
    elif art_name == 'ART_GACHA_MAGIC':
        # Adorable Toy Capsule Gacha Machine with glass dome, 5 capsules & rotary dial!
        # Top cap
        d.rounded_rectangle([64, 16, 136, 34], radius=9, fill=(70, 70, 70, 255), outline=(15, 15, 15, 255), width=5)
        # Big glass globe
        d.ellipse([38, 26, 162, 144], fill=(248, 248, 248, 255), outline=(15, 15, 15, 255), width=6)
        # Mini capsules inside globe
        caps = [(74, 96, 60), (118, 102, 150), (96, 70, 220), (126, 72, 70), (72, 66, 160)]
        for cx, cy, shade in caps:
            d.ellipse([cx - 17, cy - 17, cx + 17, cy + 17], fill=(255, 255, 255, 255), outline=(15, 15, 15, 255), width=4)
            d.pieslice([cx - 17, cy - 17, cx + 17, cy + 17], 180, 360, fill=(shade, shade, shade, 255), outline=(15, 15, 15, 255), width=4)
        # Base body
        d.polygon([(48, 126), (152, 126), (164, 184), (36, 184)], fill=(135, 135, 135, 255), outline=(15, 15, 15, 255), width=6)
        # Thumbwheel crank dial + exit door
        d.ellipse([60, 138, 102, 180], fill=(255, 255, 255, 255), outline=(15, 15, 15, 255), width=5)
        d.rounded_rectangle([68, 153, 94, 165], radius=4, fill=(40, 40, 40, 255))
        d.rounded_rectangle([116, 144, 148, 178], radius=7, fill=(45, 45, 45, 255), outline=(255, 255, 255, 255), width=3)
        draw_sparkle(d, 24, 52, 9, 4)
        draw_sparkle(d, 176, 48, 9, 4)
    elif art_name == 'ART_ALARM_CLOCK':
        paste_emoji_centered(c, '⏰', 100, 102, F_EM_160)
    elif art_name == 'ART_DINO':
        paste_emoji_centered(c, '🦖', 100, 102, F_EM_160)
    elif art_name == 'ART_PANDA':
        paste_emoji_centered(c, '🐼', 92, 102, F_EM_160)
        paste_emoji_centered(c, '🎋', 168, 134, F_EM_48)
    elif art_name == 'ART_FOX':
        paste_emoji_centered(c, '🦊', 100, 102, F_EM_160)
    elif art_name == 'ART_PENGUIN':
        paste_emoji_centered(c, '🐧', 100, 102, F_EM_160)
    elif art_name == 'ART_UNICORN':
        paste_emoji_centered(c, '🦄', 100, 102, F_EM_160)
    elif art_name == 'ART_ROBOT':
        paste_emoji_centered(c, '🤖', 100, 102, F_EM_160)
    elif art_name == 'ART_SQUIRREL':
        paste_emoji_centered(c, '🐿️', 100, 102, F_EM_160)
    elif art_name == 'ART_HEDGEHOG':
        paste_emoji_centered(c, '🦔', 100, 102, F_EM_160)
    elif art_name == 'ART_BUTTERFLY':
        paste_emoji_centered(c, '🦋', 100, 102, F_EM_160)
    elif art_name == 'ART_MONKEY':
        paste_emoji_centered(c, '🐵', 100, 102, F_EM_160)
    elif art_name == 'ART_CAT':
        paste_emoji_centered(c, '🐱', 96, 102, F_EM_160)
        paste_emoji_centered(c, '✨', 162, 48, F_EM_48)
    elif art_name == 'ART_LAPTOP_USB':
        paste_emoji_centered(c, '💻', 96, 104, F_EM_160)
        paste_emoji_centered(c, '🔌', 156, 62, F_EM_64)
    elif art_name == 'ART_WIFI_PHONE':
        paste_emoji_centered(c, '📱', 86, 110, F_EM_160)
        # Crisp Wi-Fi radio waves at top-right
        for r in (18, 34, 50):
            d.arc([138 - r, 72 - r, 138 + r, 72 + r], 220, 320, fill=(20, 20, 20, 255), width=5)
        d.ellipse([133, 68, 143, 78], fill=(20, 20, 20, 255))
    elif art_name == 'ART_XYLOPHONE':
        # 6 graduated wooden xylophone bars on rails + two mallets + musical note
        d.line([(18, 82), (182, 92)], fill=(30, 30, 30, 255), width=6)
        d.line([(18, 142), (182, 132)], fill=(30, 30, 30, 255), width=6)
        for i in range(6):
            bx = 24 + i * 26
            bh = 114 - i * 10
            by = 112 - bh // 2
            shade = 110 if (i % 2 == 0) else 205
            d.rounded_rectangle([bx, by, bx + 20, by + bh], radius=6,
                                fill=(shade, shade, shade, 255), outline=(15, 15, 15, 255), width=4)
            d.ellipse([bx + 7, by + 10, bx + 13, by + 16], fill=(255, 255, 255, 255), outline=(15, 15, 15, 255), width=2)
            d.ellipse([bx + 7, by + bh - 16, bx + 13, by + bh - 10], fill=(255, 255, 255, 255), outline=(15, 15, 15, 255), width=2)
        # Crossed mallets at bottom + music note at top-right
        d.line([(54, 184), (126, 144)], fill=(20, 20, 20, 255), width=6)
        d.ellipse([118, 132, 140, 154], fill=(70, 70, 70, 255), outline=(15, 15, 15, 255), width=4)
        d.line([(146, 184), (74, 144)], fill=(20, 20, 20, 255), width=6)
        d.ellipse([60, 132, 82, 154], fill=(210, 210, 210, 255), outline=(15, 15, 15, 255), width=4)
        paste_emoji_centered(c, '🎵', 156, 36, F_EM_48)
    elif art_name == 'ART_TRAIN':
        paste_emoji_centered(c, '🚂', 100, 104, F_EM_160)
    elif art_name == 'ART_CAKE_TOWER':
        paste_emoji_centered(c, '🎂', 100, 102, F_EM_160)
    elif art_name == 'ART_CONNECT4':
        # Classic Connect-4 vertical rack + black/white discs + drop arrow
        d.polygon([(90, 16), (110, 16), (100, 34)], fill=(20, 20, 20, 255))
        d.rounded_rectangle([24, 42, 176, 174], radius=14, fill=(155, 155, 155, 255), outline=(15, 15, 15, 255), width=6)
        # 4x3 disc slots
        board = [
            [0, 0, 2, 1],
            [0, 1, 1, 2],
            [1, 2, 1, 2],
        ]
        for r in range(3):
            for col in range(4):
                cx = 48 + col * 35
                cy = 66 + r * 38
                st = board[r][col]
                if st == 0:
                    d.ellipse([cx - 13, cy - 13, cx + 13, cy + 13], fill=(255, 255, 255, 255), outline=(15, 15, 15, 255), width=4)
                elif st == 1:
                    d.ellipse([cx - 13, cy - 13, cx + 13, cy + 13], fill=(25, 25, 25, 255), outline=(15, 15, 15, 255), width=4)
                    d.ellipse([cx - 6, cy - 6, cx - 1, cy - 1], fill=(255, 255, 255, 255))
                else:
                    d.ellipse([cx - 13, cy - 13, cx + 13, cy + 13], fill=(245, 245, 245, 255), outline=(15, 15, 15, 255), width=4)
                    d.ellipse([cx - 6, cy - 6, cx + 6, cy + 6], fill=(245, 245, 245, 255), outline=(15, 15, 15, 255), width=3)
        d.rounded_rectangle([14, 172, 186, 188], radius=6, fill=(70, 70, 70, 255), outline=(15, 15, 15, 255), width=5)
    elif art_name == 'ART_IDIOM_SCROLL':
        paste_emoji_centered(c, '📜', 92, 104, F_EM_160)
        paste_emoji_centered(c, '🖌️', 152, 132, F_EM_64)
    elif art_name == 'ART_PLANET_TURRET':
        # Outer radar orbit + central planet + rotating turret + incoming meteor
        d.ellipse([22, 22, 178, 178], outline=(90, 90, 90, 255), width=4)
        d.line([(100, 100), (156, 44)], fill=(20, 20, 20, 255), width=7)
        d.ellipse([62, 62, 138, 138], fill=(175, 175, 175, 255), outline=(15, 15, 15, 255), width=6)
        d.arc([44, 82, 156, 122], 160, 20, fill=(20, 20, 20, 255), width=5)
        paste_emoji_centered(c, '☄️', 158, 42, F_EM_64)
        draw_sparkle(d, 42, 50, 8, 4)
    elif art_name == 'ART_SLIDE_PUZZLE':
        # 3x3 Klotski / 8-puzzle frame with bold numbers 1..8 and empty bottom-right gap
        d.rounded_rectangle([22, 22, 178, 178], radius=16, fill=(95, 95, 95, 255), outline=(15, 15, 15, 255), width=6)
        nf = ImageFont.truetype(CLOCK_FONT, 28, index=2)
        nums = ['1', '2', '3', '4', '5', '6', '7', '8', '']
        for idx, ch in enumerate(nums):
            r, col = idx // 3, idx % 3
            tx = 32 + col * 46
            ty = 32 + r * 46
            if ch:
                d.rounded_rectangle([tx, ty, tx + 42, ty + 42], radius=7,
                                    fill=(250, 250, 250, 255), outline=(15, 15, 15, 255), width=4)
                bb = d.textbbox((0, 0), ch, font=nf)
                d.text((tx + (42 - (bb[2] - bb[0])) // 2 - bb[0],
                        ty + (42 - (bb[3] - bb[1])) // 2 - bb[1]), ch, font=nf, fill=(15, 15, 15, 255))
            else:
                d.rounded_rectangle([tx + 2, ty + 2, tx + 40, ty + 40], radius=7,
                                    fill=(60, 60, 60, 255), outline=(255, 255, 255, 255), width=2)

    return c

def convert_rgba_to_woodcut_1bpp(rgba_200, target_dim):
    """
    Resizes 200x200 RGBA scene with LANCZOS to target_dim (64, 96, or 176) and converts
    to crisp Playdate/woodcut 3-tone halftone + bold cartoon silhouette contour!
    """
    pad = 4
    inner = target_dim - 2 * pad
    scaled = rgba_200.resize((inner, inner), Image.Resampling.LANCZOS)
    canvas = Image.new('RGBA', (target_dim, target_dim), (255, 255, 255, 0))
    canvas.paste(scaled, (pad, pad))

    alpha = canvas.split()[3]
    white_bg = Image.new('RGBA', (target_dim, target_dim), (255, 255, 255, 255))
    comp = Image.alpha_composite(white_bg, canvas).convert('L')

    contour_k = 5 if target_dim >= 96 else 3
    dil = alpha.filter(ImageFilter.MaxFilter(contour_k))
    edges = comp.filter(ImageFilter.FIND_EDGES)
    contrast = ImageOps.autocontrast(comp, cutoff=1)
    bright = contrast.point(lambda p: int(255 * ((p / 255.0) ** 0.64)))

    p_br = bright.load()
    p_al = alpha.load()
    p_dl = dil.load()
    p_ed = edges.load()

    img_out = Image.new('L', (target_dim, target_dim), 255)
    po = img_out.load()
    row_bytes = (target_dim + 7) // 8
    bits = bytearray(row_bytes * target_dim)

    edge_thresh = 68 if target_dim >= 96 else 58
    for ry in range(target_dim):
        for rx in range(target_dim):
            a = p_al[rx, ry]
            dl = p_dl[rx, ry]
            ink = False
            if dl > 85 and a < 115:
                ink = True  # Crisp outer cartoon outline
            elif a >= 115:
                v = p_br[rx, ry]
                ed = p_ed[rx, ry]
                if ed > edge_thresh and v < 195:
                    ink = True
                elif v < 78:
                    ink = True
                elif v < 122:
                    ink = (((rx + ry) & 1) == 0)  # 50% checkerboard mid-shadow
                elif v < 176:
                    ink = (((rx & 1) == 0) and ((ry & 1) == 0))  # 25% dot halftone
            if ink:
                po[rx, ry] = 0
                bits[ry * row_bytes + (rx >> 3)] |= (0x80 >> (rx & 7))

    return bytes(bits), img_out

ART_LIST = [
    'ART_RABBIT',
    'ART_TURTLE',
    'ART_TADPOLE_FROG',
    'ART_THREE_PIGS',
    'ART_ELEPHANT',
    'ART_SIMAGUANG',
    'ART_MOON_STARS',
    'ART_KNIGHT_FOREST',
    'ART_SUBMARINE',
    'ART_FERRIS_WHEEL',
    'ART_CARDS_MAGIC',
    'ART_BREAKOUT',
    'ART_LOCK_TREASURE',
    'ART_PARROT_MIC',
    'ART_TROPHY_STARS',
    'ART_OWL_GUARD',
    'ART_SLEEP_BEAR',
    'ART_GACHA_MAGIC',
    'ART_ALARM_CLOCK',
    'ART_DINO',
    'ART_PANDA',
    'ART_FOX',
    'ART_PENGUIN',
    'ART_UNICORN',
    'ART_ROBOT',
    'ART_SQUIRREL',
    'ART_HEDGEHOG',
    'ART_BUTTERFLY',
    'ART_MONKEY',
    'ART_CAT',
    'ART_LAPTOP_USB',
    'ART_WIFI_PHONE',
    'ART_XYLOPHONE',
    'ART_TRAIN',
    'ART_CAKE_TOWER',
    'ART_CONNECT4',
    'ART_IDIOM_SCROLL',
    'ART_PLANET_TURRET',
    'ART_SLIDE_PUZZLE',
]

def render_clock_digits_64():
    """Renders 11 giant bold numerals '0'..'9' and ':' at 40x68 1bpp for the Bedside Sleep Clock!"""
    W, H = 40, 68
    row_bytes = (W + 7) // 8  # 5 * 68 = 340 bytes each * 11 = 3,740 bytes
    f = ImageFont.truetype(CLOCK_FONT, 62, index=2)  # Hiragino Sans GB W6 Bold!
    chars = "0123456789:"
    bmps = bytearray()
    for ch in chars:
        im = Image.new('L', (W, H), 255)
        d = ImageDraw.Draw(im)
        bb = d.textbbox((0, 0), ch, font=f)
        x = (W - (bb[2] - bb[0])) // 2 - bb[0]
        y = (H - (bb[3] - bb[1])) // 2 - bb[1]
        d.text((x, y), ch, font=f, fill=0)
        px = im.load()
        out = bytearray(row_bytes * H)
        for ry in range(H):
            for rx in range(W):
                if px[rx, ry] < 160:
                    out[ry * row_bytes + (rx >> 3)] |= (0x80 >> (rx & 7))
        bmps.extend(out)
    return bytes(bmps), W, H

def main():
    sec64 = bytearray()
    sec96 = bytearray()
    sec176 = bytearray()

    cols = 8
    rows = (len(ART_LIST) + cols - 1) // cols
    sheet = Image.new('L', (cols * 188, rows * 204), 245)
    sd = ImageDraw.Draw(sheet)
    lbl_f = ImageFont.truetype(CLOCK_FONT, 13, index=2)

    for idx, name in enumerate(ART_LIST):
        rgba = build_scene_rgba_200(name)
        b64, _    = convert_rgba_to_woodcut_1bpp(rgba, 64)
        b96, _    = convert_rgba_to_woodcut_1bpp(rgba, 96)
        b176, imL = convert_rgba_to_woodcut_1bpp(rgba, 176)
        sec64.extend(b64)
        sec96.extend(b96)
        sec176.extend(b176)

        c, r = idx % cols, idx // cols
        sx, sy = c * 188 + 6, r * 204 + 6
        sheet.paste(imL, (sx, sy))
        sd.text((sx + 6, sy + 178), f"{idx}.{name[4:]}", font=lbl_f, fill=0)

    clk_bmps, clk_w, clk_h = render_clock_digits_64()

    # Header (32 bytes): 4s H H H H 4I = 4 + 8 + 16 + 4 = 32 bytes
    off64  = 32
    off96  = off64 + len(sec64)
    off176 = off96 + len(sec96)
    offclk = off176 + len(sec176)
    hdr = struct.pack('<4sHHHH4I4x', b'ARTP', len(ART_LIST), clk_w, clk_h, 0, off64, off96, off176, offclk)
    assert len(hdr) == 32

    os.makedirs(os.path.dirname(OUT_BIN), exist_ok=True)
    with open(OUT_BIN, 'wb') as f:
        f.write(hdr)
        f.write(sec64)
        f.write(sec96)
        f.write(sec176)
        f.write(clk_bmps)

    os.makedirs(os.path.dirname(PREVIEW_PNG), exist_ok=True)
    sheet.save(PREVIEW_PNG)
    print(f"Generated {OUT_BIN}: {os.path.getsize(OUT_BIN)/1024:.1f} KB ({len(ART_LIST)} illustrations x 3 sizes + 68px clock digits)")
    print(f"Saved preview sheet: {PREVIEW_PNG}")

if __name__ == '__main__':
    main()
