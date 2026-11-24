"""Generate compact Korean LCD labels with the same font as station labels."""
import argparse
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--font', required=True, type=Path, help='Path to a font containing Korean glyphs')
parser.add_argument("--font-index", type=int, default=0, help="Font index for a TTC collection")
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]


out = ['#pragma once', '#include <Arduino.h>', '// LCD glyphs rendered by scripts/metro_arrival_glyphs.py.']
for name, text, size, width, height in [('SOON', '곧', 64, 64, 66),
                                      ('ARRIVED', '도착', 16, 32, 18),
                                      ('NEXT_TRAIN', '다음 열차', 11, 64, 14),
                                      ('SOON_SMALL', '곧 도착', 12, 80, 20),
                                      ('MINUTE', '분', 16, 24, 20),
                                      ('MINUTE_SMALL', '분', 13, 24, 20),
                                      ('SETUP_STATION', '역 설정', 12, 48, 12)]:
    image = Image.new('1', (width, height))
    draw = ImageDraw.Draw(image)
    draw.text((0, 0), text, font=ImageFont.truetype(str(args.font), size, index=args.font_index), fill=1, anchor='lt')
    out.append(f'static const uint8_t {name}[] PROGMEM = {{' + ','.join(str(b) for b in image.tobytes()) + '};')
(root / 'include/metro_arrival_glyphs.h').write_text('\n'.join(out)+'\n')

