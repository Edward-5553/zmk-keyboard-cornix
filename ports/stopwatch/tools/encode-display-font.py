"""Generate the UI-only Noto Sans SC subset: python tools/encode-display-font.py FONT.ttf.

Pillow is required only for regeneration. Source and OFL license: main/assets/README.md.
"""
import sys
import re
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

port=Path(__file__).resolve().parents[1]
source=(port/'main/display.c').read_text(encoding='utf-8')
chars=sorted(set(range(32,127)) | {ord(c) for s in re.findall(r'"([^"\n]*)"',source) for c in s if ord(c)>127})
font=ImageFont.truetype(sys.argv[1],16)
data=[]
glyphs=['{0}']
for code in chars:
    char=chr(code)
    tile=Image.new('L',(16,20))
    ImageDraw.Draw(tile).text((0,16),char,font=font,fill=255,anchor='ls')
    pixels=list(tile.getdata())
    index=len(data)
    data.extend(((pixels[i]>>4)<<4)|(pixels[i+1]>>4) for i in range(0,len(pixels),2))
    glyphs.append('{.bitmap_index=%d,.adv_w=%d,.box_w=16,.box_h=20,.ofs_x=0,.ofs_y=-4}' % (index,round(font.getlength(char)*16)))
text='''// Generated Noto Sans SC UI subset. SIL Open Font License: assets/OFL.txt.
#include "lvgl.h"
static const uint8_t bitmap[]={
'''
text+='\n'.join(','.join(str(n) for n in data[i:i+32])+',' for i in range(0,len(data),32))+'\n};\n'
text+='static const lv_font_fmt_txt_glyph_dsc_t glyphs[]={'+',\n'.join(glyphs)+'};\n'
text+='static const uint16_t codes[]={'+','.join(str(c-32) for c in chars)+'};\n'
text+='''static const lv_font_fmt_txt_cmap_t maps[]={
{.range_start=32,.range_length=%d,.glyph_id_start=1,.unicode_list=codes,
 .list_length=%d,.type=LV_FONT_FMT_TXT_CMAP_SPARSE_TINY}};
static const lv_font_fmt_txt_dsc_t descriptor={.glyph_bitmap=bitmap,.glyph_dsc=glyphs,
 .cmaps=maps,.cmap_num=1,.bpp=4,.bitmap_format=0};
const lv_font_t stopwatch_font_16={.get_glyph_dsc=lv_font_get_glyph_dsc_fmt_txt,
 .get_glyph_bitmap=lv_font_get_bitmap_fmt_txt,.line_height=20,.base_line=4,.dsc=&descriptor};
''' % (max(chars)-31,len(chars))
(port/'main/assets/display_font.c').write_text(text,encoding='utf-8')
(port/'main/assets/font_codepoints.json').write_text(__import__('json').dumps(chars)+'\n')
print('Generated',len(chars),'glyphs,',len(data),'bitmap bytes')
