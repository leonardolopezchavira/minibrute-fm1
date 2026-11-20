#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""MB-1's start-up screen (app/splash/splash.png, 240 x 240, drawn from app/splash/splash.svg) as app/splash_img.h:
16 colours (RGB565), run-length coded, a byte a run: the colour's index in the high four bits, the run's length
less one in the low four. app.c's app_splash draws it.

    python3 tools/make_splash.py
"""
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
im = Image.open(ROOT / "app" / "splash" / "splash.png").convert("RGB")
assert im.size == (240, 240), im.size
q = im.quantize(colors=16, method=Image.MEDIANCUT, dither=Image.NONE)
pal = q.getpalette()[:48]
px = list(q.get_flattened_data() if hasattr(q, "get_flattened_data") else q.getdata())
runs = []
i = 0
while i < len(px):
    c, n = px[i], 1
    while i + n < len(px) and px[i + n] == c and n < 16:
        n += 1
    runs.append(c << 4 | (n - 1))
    i += n
rgb565 = [((pal[3 * k] >> 3) << 11) | ((pal[3 * k + 1] >> 2) << 5) | (pal[3 * k + 2] >> 3) for k in range(16)]
out = ["/* SPDX-License-Identifier: MIT", " * MB-1's start-up screen: made by tools/make_splash.py from app/splash/splash.png (do not edit) */",
       "static const uint16_t SPLASH_PAL[16] = {" + ", ".join(f"0x{c:04X}u" for c in rgb565) + "};",
       f"#define SPLASH_N {len(runs)}u",
       "static const uint8_t SPLASH_RLE[SPLASH_N] = {"]
for k in range(0, len(runs), 24):
    out.append("    " + ", ".join(f"0x{b:02X}" for b in runs[k:k + 24]) + ",")
out.append("};")
(ROOT / "app" / "splash_img.h").write_text("\n".join(out) + "\n")
print(f"splash   app/splash_img.h: {len(runs)} B")
