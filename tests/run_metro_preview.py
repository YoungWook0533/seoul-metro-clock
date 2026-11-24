"""Pixel-check the Metro renderer against the bundled TFT host mocks."""
import os
from pathlib import Path
import shlex
import subprocess
from PIL import Image, ImageDraw

root = Path(__file__).resolve().parents[1]
out = root / '.pio/metro-preview'
out.mkdir(parents=True, exist_ok=True)
labels = Image.new('1', (240, 240))
draw = ImageDraw.Draw(labels)
draw.text((10, 10), 'Station', fill=1)
draw.text((10, 38), 'Line', fill=1)
for x in (15, 75, 135, 195):
    draw.text((x, 100), 'Stop', fill=1)
(out / 'labels.bin').write_bytes(labels.tobytes())
source = (root / 'src/metro.cpp').read_text(encoding='utf-8')
english = source[source.index('String latinName('):source.index('int position(const Train& train)')]
(out / 'metro_render.inc').write_text(english + source[source.index('void drawWeather()'):], encoding='utf-8')
mock = root / 'tests/mock'
compiler = shlex.split(os.environ.get('CXX', 'c++'))
binary = out / 'preview.exe'
command = compiler + ['-std=c++17', '-O2', '-UNDEBUG', '-I' + str(out), '-I' + str(mock),
                      '-I' + str(root / 'src'), '-I' + str(root / 'include'),
                      '-I' + str(root / '.pio/libdeps/esp12e/TFT_eSPI'),
                      '-I' + str(root / '.pio/libdeps/esp12e/ArduinoJson/src'),
                      str(root / 'tests/metro_preview.cpp'), str(mock / 'TFT_eSPI.cpp'),
                      '-o', str(binary)]
subprocess.run(command, check=True)
subprocess.run([str(binary), str(out)], check=True)
for frame in out.glob('*.raw'):
    values = memoryview(frame.read_bytes()).cast('H')
    image = Image.new('RGB', (240, 240))
    image.putdata([((v >> 11)*255//31, ((v >> 5)&63)*255//63, (v&31)*255//31) for v in values])
    image.resize((720, 720), Image.Resampling.NEAREST).save(frame.with_suffix('.png'))
print('PASS: display geometry, gauge movement, and incremental redraw')
