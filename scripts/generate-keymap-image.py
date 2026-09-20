"""Generate README SVG/PNG diagrams from the same source as the HTML editor.

Requires Pillow. Run after keymap edits: python scripts/generate-keymap-image.py
"""
import html
from pathlib import Path
import runpy
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
data = runpy.run_path(str(ROOT / 'scripts/generate-keymap-html.py'))
layers, positions = data['layers'], data['positions']
labels = dict(C_MUTE='Mute', TAB='Tab', ESC='Esc', BACKSPACE='Bksp', BSLH='\\', LEFT_SHIFT='L Shift', RIGHT_SHIFT='R Shift', LEFT_CONTROL='L Ctrl', LEFT_ALT='L Alt', CAPSLOCK='Caps', SPACE='Space', ENTER='Enter', DELETE='Delete', SEMI=';', SQT="'", COMMA=',', DOT='.', FSLH='/', UP='Up', DOWN='Down', LEFT='Left', RIGHT='Right', TILDE='~', EXCL='!', AT='@', HASH='#', DLLR='$', PRCNT='%', CARET='^', AMPS='&', ASTRK='*', LPAR='(', RPAR=')', GRAVE='`', LBRC='{', LBKT='[', LT='<', MINUS='-', UNDER='_', PLUS='+', EQUAL='=', GT='>', RBKT=']', RBRC='}', PIPE='|')
WIDTH, HEIGHT = 1510, 2020
svg = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{WIDTH}" height="{HEIGHT}" viewBox="0 0 {WIDTH} {HEIGHT}" role="img" aria-label="Cornix current 50-position keymap, layers 0 to 2">', '<rect width="100%" height="100%" fill="#10171d"/>']
canvas = Image.new('RGBA', (WIDTH, HEIGHT), '#10171d')
font_path = next((p for p in [Path('C:/Windows/Fonts/segoeui.ttf'), Path('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf')] if p.exists()), None)

def text(x, y, value, size=18, color='#e8edef', anchor='start', target=None):
    font = ImageFont.truetype(str(font_path), size) if font_path else ImageFont.load_default()
    ImageDraw.Draw(target if target is not None else canvas).text((x,y), value, font=font, fill=color, anchor='mm' if anchor=='middle' else 'lm')
    svg.append(f'<text x="{x}" y="{y}" font-family="Segoe UI,DejaVu Sans,sans-serif" font-size="{size}" fill="{color}" text-anchor="{anchor}" dominant-baseline="central">{html.escape(value)}</text>')

def label(binding):
    pairs = {'&pair_braces': '{|}', '&pair_brackets': '[|]', '&pair_angles': '<|>'}
    if binding in pairs: return pairs[binding]
    p=binding.split()
    if p[0]=='&none': return '—'
    if p[0]=='&trans': return 'Transparent'
    if p[0]=='&bt': return 'BT Clear' if p[1]=='BT_CLR' else f'BT {int(p[2])+1}'
    code=p[2] if p[0]=='&lt' else p[1]
    if code.startswith('LC(') and code.endswith(')'):
        return 'Ctrl+' + code[3:-1]
    return labels.get(code, code[1:] if len(code)==2 and code[0]=='N' and code[1].isdigit() else code)

text(30,40,'CORNIX / CURRENT KEYMAP',32)
text(30,82,'50 positions per layer · source: config/cornix.keymap · US symbols',18,'#a7b6bf')
for index, title in enumerate(['L0 / BASE','L1 / NUMBERS + NAVIGATION','L2 / SYMBOLS + FUNCTION KEYS']):
    top=110+index*590
    svg.append(f'<rect x="15" y="{top}" width="1480" height="575" rx="20" fill="#16212a"/>')
    ImageDraw.Draw(canvas).rounded_rectangle((15,top,1495,top+575),radius=20,fill='#16212a')
    text(35,top+33,title,24,'#a9e5cf')
    ox,oy=30,top+65
    svg.append(f'<g transform="translate({ox} {oy})">')
    for i,(w,h,x,y,r,rx,ry) in enumerate(positions):
        raw=layers[index]['keys'][i]
        inherited=raw=='&trans'
        effective=layers[0]['keys'][i] if inherited else raw
        dual=effective.startswith('&lt')
        fill='#265446' if dual else '#1b2831' if effective=='&none' else '#263640'
        color='#a7b6bf' if inherited or effective=='&none' else '#edf4f6'
        tile=Image.new('RGBA',(WIDTH,HEIGHT))
        box=(x*100+4,y*100+4,x*100+w*100-4,y*100+h*100-4)
        ImageDraw.Draw(tile).rounded_rectangle(box,radius=12,fill=fill,outline='#50636f',width=1)
        svg.append(f'<g transform="rotate({r} {rx*100} {ry*100})"><title>{html.escape(raw)}</title><rect x="{box[0]}" y="{box[1]}" width="{w*100-8}" height="{h*100-8}" rx="12" fill="{fill}" stroke="#50636f"/>')
        text(x*100+50,y*100+17,str(i),10,'#90a6b3','middle',tile)
        text(x*100+50,y*100+47,label(effective),18,color,'middle',tile)
        sub='from L0' if inherited else ('hold L1' if 'NUM_NAV' in raw else 'hold L2') if dual else ''
        text(x*100+50,y*100+73,sub,12,'#95d7c0','middle',tile)
        svg.append('</g>')
        if r: tile=tile.rotate(-r, resample=Image.Resampling.BICUBIC, center=(rx*100,ry*100))
        canvas.alpha_composite(tile,(ox,oy))
    svg.append('</g>')
text(30,1910,'Green: tap key / hold layer.  from L0: transparent binding.  —: no action.',18,'#a7b6bf')
text(30,1945,'L3 Scroll and L4 Snipe: reserved; encoder clicks remain Caps / Mute on every layer.',18,'#a7b6bf')
text(30,1980,'ALL LAYERS / Left knob: scroll down/up, press Caps.  Right knob: volume +/-, press Mute. (CW/CCW)',18,'#a9e5cf')
svg.append('</svg>')
out=ROOT/'keymap-drawer'
(out/'cornix.svg').write_text('\n'.join(svg)+'\n',encoding='utf-8')
canvas.convert('RGB').save(out/'cornix.png')
print('Generated cornix.svg and cornix.png: three layers, 150 positions.')
