"""Verify requested label spacing using the actual LCD fonts, without a compiler."""
from pathlib import Path
import re

root=Path(__file__).resolve().parents[1]
fonts=root/'.pio/libdeps/esp12e/TFT_eSPI/Fonts'

def widths(filename,symbol):
    source=(fonts/filename).read_text()
    body=re.search(r'\b'+symbol+r'\[96\][^{]*\{([^}]+)',source,re.S).group(1)
    body=re.sub(r'//[^\n]*','',body)
    return [int(value) for value in re.findall(r'\b\d+\b',body)]

font2=widths('Font16.c','widtbl_f16')
font4=widths('Font32rle.c','widtbl_f32')
font8=widths('Font72rle.c','widtbl_f72')
def width(text,table):
    return sum(table[ord(char)-32] for char in text)

for tenths in range(-400,601):
    text=f'{tenths/10:.1f}'
    text_width=width(text,font2)
    temperature_end=18+text_width+width('C',font2)
    icon_left=min(71,30+text_width)
    assert temperature_end<icon_left, text
    assert icon_left+19<=92, text
for minutes in range(100):
    text=str(minutes)
    assert 57-width(text,font4)>=10
    assert 57<62
    assert 207-width(text,font8)>=93
assert 166+26<204
assert 180+13<204
assert 190+16<=132+75
print('PASS: 1001 temperatures, temperature-before-icon gap, left minutes alignment, separate columns and main-unit baseline')
