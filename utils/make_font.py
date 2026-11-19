#!/usr/bin/env python3
import re, struct, sys

if len(sys.argv) != 3:
    print("Usage: make_font.py <font.h> <font.bin>")
    sys.exit(1)

with open(sys.argv[1], "r", encoding="utf-8") as f:
    font_h = f.read()

m_bmp = re.search(r"font_bitmaps\[\]\s*=\s*\{(.*?)\};", font_h, re.S)
bitmaps = bytes(int(x, 16) for x in re.findall(r"0x[0-9a-fA-F]+", m_bmp.group(1)))

metrics = [(0, 0, 0, 0, 0, 0)] * 128
pattern = re.compile(
    r"case\s+'(\\.|[^'])'\s*:\s*return\s*\(GlyphMetrics\)\{\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(\d+)\s*\};"
)
for m in pattern.finditer(font_h):
    ch_raw = m.group(1)
    ch = "'" if ch_raw == r"\'" else ("\\" if ch_raw == r"\\" else ch_raw)
    code = ord(ch)
    if 0 <= code < 128:
        metrics[code] = tuple(int(m.group(i)) for i in range(2, 8))

buf = bytearray()
buf += struct.pack("<IIII", 0x544E4F46, 128, len(bitmaps), 0)
for w, rows, left, top, adv, off in metrics:
    buf += struct.pack("<iiiiiI", w, rows, left, top, adv, off)
buf += bitmaps

with open(sys.argv[2], "wb") as f:
    f.write(buf)
print(f"Written {sys.argv[2]} ({len(buf)} bytes)")
