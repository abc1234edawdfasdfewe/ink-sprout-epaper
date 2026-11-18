#!/usr/bin/env python3
"""
Builds simulator/index.html: a 100% standalone, zero-CDN, single-file shareable HTML
firmware simulator that embeds the exact 1bpp woodcut bitmaps from
components/bsp_board/assets/story_art_pack.bin!
"""
import base64
import os
import re

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
ART_BIN = os.path.join(ROOT, 'components', 'bsp_board', 'assets', 'story_art_pack.bin')
SRC_HTML = os.environ.get('INKSPROUT_SIM_TEMPLATE', os.path.join(ROOT, 'simulator', 'index.html'))
OUT_HTML = os.path.join(ROOT, 'simulator', 'index.html')

STANDALONE_CSS = """
  <style>
    *, *::before, *::after { box-sizing: border-box; }
    :root {
      --background: #f3f2ee;
      --foreground: #1b1c1b;
      --card: #ffffff;
      --border: #dcdad2;
      --muted-foreground: #5c5e5a;
      --primary: #1b1c1b;
      --primary-foreground: #ffffff;
      --secondary: #e8e6de;
      --secondary-foreground: #1b1c1b;
    }
    body {
      margin: 0;
      padding: 20px;
      background: var(--background);
      color: var(--foreground);
      font-family: -apple-system, BlinkMacSystemFont, "PingFang SC", "Hiragino Sans GB", "Microsoft YaHei", sans-serif;
      user-select: none;
      -webkit-user-select: none;
    }
    .max-w-5xl { max-width: 1024px; }
    .max-w-4xl { max-width: 896px; }
    .mx-auto { margin-left: auto; margin-right: auto; }
    .space-y-4 > * + * { margin-top: 16px; }
    .bg-\\[var\\(--card\\)\\] { background: var(--card); }
    .bg-\\[var\\(--background\\)\\] { background: var(--background); }
    .bg-\\[var\\(--primary\\)\\] { background: var(--primary); }
    .bg-\\[var\\(--secondary\\)\\] { background: var(--secondary); }
    .text-\\[var\\(--primary-foreground\\)\\] { color: var(--primary-foreground); }
    .text-\\[var\\(--secondary-foreground\\)\\] { color: var(--secondary-foreground); }
    .text-\\[var\\(--muted-foreground\\)\\] { color: var(--muted-foreground); }
    .border { border: 1px solid var(--border); }
    .border-\\[var\\(--border\\)\\] { border-color: var(--border); }
    .rounded-2xl { border-radius: 16px; }
    .rounded-xl { border-radius: 12px; }
    .rounded-lg { border-radius: 8px; }
    .rounded-full { border-radius: 9999px; }
    .rounded-t-2xl { border-top-left-radius: 14px; border-top-right-radius: 14px; }
    .rounded-t-xl { border-top-left-radius: 11px; border-top-right-radius: 11px; }
    .rounded-\\[30px\\] { border-radius: 30px; }
    .p-3 { padding: 12px; }
    .p-3\\.5 { padding: 14px; }
    .p-2 { padding: 10px; }
    .p-5 { padding: 24px; }
    .px-1 { padding-left: 4px; padding-right: 4px; }
    .px-2 { padding-left: 8px; padding-right: 8px; }
    .px-2\\.5 { padding-left: 10px; padding-right: 10px; }
    .px-3 { padding-left: 12px; padding-right: 12px; }
    .px-3\\.5 { padding-left: 14px; padding-right: 14px; }
    .px-8 { padding-left: 36px; padding-right: 36px; }
    .py-0\\.5 { padding-top: 2px; padding-bottom: 2px; }
    .py-1 { padding-top: 5px; padding-bottom: 5px; }
    .py-1\\.5 { padding-top: 6px; padding-bottom: 6px; }
    .pt-7 { padding-top: 24px; }
    .mt-0\\.5 { margin-top: 4px; }
    .mt-3 { margin-top: 12px; }
    .mb-1 { margin-bottom: 4px; }
    .-mb-1 { margin-bottom: -4px; }
    .mr-1 { margin-right: 4px; }
    .flex { display: flex; }
    .flex-wrap { flex-wrap: wrap; }
    .items-center { align-items: center; }
    .justify-between { justify-content: space-between; }
    .gap-1\\.5 { gap: 6px; }
    .gap-2 { gap: 8px; }
    .gap-3 { gap: 12px; }
    .grid { display: grid; }
    .grid-cols-1 { grid-template-columns: repeat(3, minmax(0, 1fr)); }
    @media (max-width: 760px) { .grid-cols-1 { grid-template-columns: 1fr; } .hidden { display: none; } }
    .text-base { font-size: 16px; }
    .text-xs { font-size: 12.5px; line-height: 1.5; }
    .font-bold { font-weight: 700; }
    .font-extrabold { font-weight: 800; }
    .font-semibold { font-weight: 600; }
    .font-medium { font-weight: 500; }
    .text-white { color: #ffffff; }
    .text-neutral-950 { color: #111111; }
    .text-neutral-700 { color: #3f3f46; }
    .text-neutral-600 { color: #52525b; }
    .bg-neutral-800 { background: #27272a; }
    .bg-neutral-700 { background: #3f3f46; }
    .bg-neutral-300 { background: #d4d4d8; }
    .bg-amber-500 { background: #f59e0b; }
    .bg-black { background: #000000; }
    .relative { position: relative; }
    .absolute { position: absolute; }
    .inset-0 { top: 0; right: 0; bottom: 0; left: 0; }
    .z-10 { z-index: 10; }
    .w-3\\.5 { width: 14px; }
    .h-3\\.5 { height: 14px; }
    .top-3\\.5 { top: 14px; }
    .bottom-3\\.5 { bottom: 14px; }
    .left-3\\.5 { left: 14px; }
    .right-3\\.5 { right: 14px; }
    .overflow-hidden { overflow: hidden; }
    .pointer-events-none { pointer-events: none; }
    .opacity-0 { opacity: 0; }
    .opacity-75 { opacity: 0.75; }
    .transition-opacity { transition: opacity 0.15s ease; }
    button {
      border: none;
      cursor: pointer;
      font-family: inherit;
      transition: transform 0.08s ease, opacity 0.12s ease;
    }
    button:hover { opacity: 0.9; }
    button:active { transform: scale(0.96); }
    code { background: #eae8e0; padding: 1px 5px; border-radius: 4px; font-size: 11.5px; }
"""

FIRMWARE_ART_DECODER_JS = """
/* === Exact Firmware story_art_pack.bin 1bpp Decoder (39 Illustrations x 3 Native Sizes) === */
const FW_ART_B64 = "__ART_B64__";
const FW_ART_BYTES = Uint8Array.from(atob(FW_ART_B64), c => c.charCodeAt(0));
const FW_VIEW = new DataView(FW_ART_BYTES.buffer);
const FW_OFF64  = FW_VIEW.getUint32(12, true);
const FW_OFF96  = FW_VIEW.getUint32(16, true);
const FW_OFF176 = FW_VIEW.getUint32(20, true);

const FW_ART_INDEX = {
  RABBIT: 0, TURTLE: 1, FROG: 2, PIGS: 3, ELEPHANT: 4, SIMAGUANG: 5, MOON: 6, KNIGHT: 7,
  SUBMARINE: 8, FERRIS: 9, CARDS: 10, BREAKOUT: 11, LOCK: 12, PARROT: 13, TROPHY: 14, OWL: 15,
  BEAR: 16, GACHA: 17, CLOCK: 18, DINO: 19, PANDA: 20, FOX: 21, PENGUIN: 22, UNICORN: 23,
  ROBOT: 24, SQUIRREL: 25, HEDGEHOG: 26, BUTTERFLY: 27, MONKEY: 28, CAT: 29, USB: 30, WIFI: 31,
  XYLOPHONE: 32, TRAIN: 33, CAKE: 34, CONNECT4: 35, SCROLL: 36, PLANET: 37, SLIDE: 38
};

const artCache = {};
function getWoodcutCanvas(artId, reqDim) {
  const dim = reqDim <= 72 ? 64 : (reqDim <= 122 ? 96 : 176);
  const key = artId + '_' + dim;
  if (artCache[key]) return artCache[key];

  const idx = FW_ART_INDEX[artId] ?? 0;
  const secOff = dim === 64 ? FW_OFF64 : (dim === 96 ? FW_OFF96 : FW_OFF176);
  const rowBytes = (dim + 7) >> 3;
  const base = secOff + idx * rowBytes * dim;

  const out = document.createElement('canvas');
  out.width = dim; out.height = dim;
  const oc = out.getContext('2d');
  const img = oc.createImageData(dim, dim);
  const d = img.data;

  for (let y = 0; y < dim; y++) {
    for (let x = 0; x < dim; x++) {
      const bit = FW_ART_BYTES[base + y * rowBytes + (x >> 3)] & (0x80 >> (x & 7));
      if (bit) {
        const oi = (y * dim + x) * 4;
        d[oi] = 24; d[oi+1] = 25; d[oi+2] = 24; d[oi+3] = 255;
      }
    }
  }
  oc.putImageData(img, 0, 0);
  artCache[key] = out;
  return out;
}
"""

def main():
    with open(ART_BIN, 'rb') as f:
        b64 = base64.b64encode(f.read()).decode('ascii')

    with open(SRC_HTML, 'r', encoding='utf-8') as f:
        html = f.read()

    # Replace external Tailwind script + opening <style> with standalone zero-CDN CSS
    html = html.replace(
        '  <script src="https://www.gstatic.com/antigravity/web/dev/tailwindcss.min.js"></script>\n  <style>',
        STANDALONE_CSS
    )

    # Replace runtime emoji woodcut rasterizer with exact firmware story_art_pack.bin decoder!
    pattern = re.compile(
        r'/\* === 1bpp Woodcut Art Pre-Rasterizer.*?return out;\n\}',
        re.DOTALL
    )
    decoder = FIRMWARE_ART_DECODER_JS.replace('__ART_B64__', b64)
    html, n = pattern.subn(lambda _: decoder, html, count=1)
    assert n == 1, "Failed to splice firmware art decoder"

    # Also let drawArt use 176px native strike for large story panels
    html = html.replace(
        'const d = dim <= 72 ? 64 : (dim <= 122 ? 96 : 156);',
        'const d = dim <= 72 ? 64 : (dim <= 122 ? 96 : 176);'
    )

    os.makedirs(os.path.dirname(OUT_HTML), exist_ok=True)
    with open(OUT_HTML, 'w', encoding='utf-8') as f:
        f.write(html)

    print(f"Generated standalone simulator: {OUT_HTML} ({os.path.getsize(OUT_HTML)/1024:.1f} KB)")

if __name__ == '__main__':
    main()
