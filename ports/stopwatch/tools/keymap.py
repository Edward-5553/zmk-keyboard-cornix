"""Compile the supported Cornix ZMK bindings. Fail closed on unsupported syntax.

This is intentionally a subset, not a general ZMK/devicetree interpreter.
The generated header lives in the build directory; config/cornix.keymap is authoritative.
"""
import re
import sys
import hashlib
import json
from pathlib import Path

KEYS = {chr(65+i): (7, 4+i, 0) for i in range(26)}
KEYS.update({f'N{i}': (7, 39 if i == 0 else 29+i, 0) for i in range(10)})
KEYS.update({f'F{i}': (7, 57+i, 0) for i in range(1, 13)})
for code, names in {
    40:'ENTER RET',41:'ESC ESCAPE',42:'BACKSPACE BSPC',43:'TAB',44:'SPACE',
    45:'MINUS',46:'EQUAL',47:'LBKT',48:'RBKT',49:'BSLH',51:'SEMI',52:'SQT',
    53:'GRAVE',54:'COMMA',55:'DOT',56:'FSLH',57:'CAPSLOCK CAPS',
    73:'INSERT INS',74:'HOME',75:'PAGE_UP PG_UP',76:'DELETE DEL',77:'END',
    78:'PAGE_DOWN PG_DN',79:'RIGHT',80:'LEFT',81:'DOWN',82:'UP',
    224:'LEFT_CONTROL LCTRL LCTL',225:'LEFT_SHIFT LSHIFT LSHFT',
    226:'LEFT_ALT LALT',227:'LEFT_GUI LGUI',228:'RIGHT_CONTROL RCTRL',
    229:'RIGHT_SHIFT RSHIFT',230:'RIGHT_ALT RALT',231:'RIGHT_GUI RGUI',
}.items():
    for name in names.split(): KEYS[name] = (7, code, 0)
for name, base in dict(TILDE='GRAVE',EXCL='N1',AT='N2',HASH='N3',DLLR='N4',
    PRCNT='N5',CARET='N6',AMPS='N7',ASTRK='N8',LPAR='N9',RPAR='N0',
    UNDER='MINUS',PLUS='EQUAL',LBRC='LBKT',RBRC='RBKT',PIPE='BSLH',
    LT='COMMA',GT='DOT',COLON='SEMI',DQT='SQT',QMARK='FSLH').items():
    KEYS[name] = (7, KEYS[base][1], 2)
KEYS.update(C_MUTE=(12,226,0), C_VOL_UP=(12,233,0), C_VOL_DN=(12,234,0))
KEYS.update(C_PLAY_PAUSE=(12,205,0),C_NEXT=(12,181,0),C_PREV=(12,182,0))
KEYS.update({f'F{i}':(7,104+i-13,0) for i in range(13,25)})
KEYS.update({f'KP_N{i}':(7,98 if i==0 else 88+i,0) for i in range(10)})
for code,name in {70:'PSCRN',71:'SLCK',72:'PAUSE_BREAK',83:'KP_NUMLOCK',84:'KP_DIVIDE',
                  85:'KP_MULTIPLY',86:'KP_MINUS',87:'KP_PLUS',88:'KP_ENTER',99:'KP_DOT',101:'K_APP'}.items():
    KEYS[name]=(7,code,0)
MODS = dict(LC=1,LS=2,LA=4,LG=8,RC=16,RS=32,RA=64,RG=128)

def key(expr):
    match = re.fullmatch(r'(LC|LS|LA|LG|RC|RS|RA|RG)\((.+)\)', expr)
    if match:
        page, code, mods = key(match[2])
        return page, code, mods | MODS[match[1]]
    if expr not in KEYS: raise ValueError(f'Unsupported key: {expr}')
    return KEYS[expr]

def compile_keymap(text):
    text = re.sub(r'/\*.*?\*/|//[^\n]*', '', text, flags=re.S)
    if re.search(r'\b(combos|conditional_layers)\s*\{',text):
        raise ValueError('Combos and conditional layers are not supported')
    defines = dict(re.findall(r'#define\s+(\w+)\s+(\d+)', text))
    macros = {}
    for name, body in re.findall(r'(\w+):\s*\w+\s*\{([^{}]*)\}', text):
        if 'zmk,behavior-macro' not in body: continue
        bind = re.search(r'bindings\s*=\s*<([^>]+)>', body)[1].strip()
        if not bind.startswith('&macro_tap '): raise ValueError('Only tap macros supported')
        seq = re.findall(r'&kp\s+(\S+)', bind)
        if bind != '&macro_tap ' + ' '.join('&kp '+x for x in seq):
            raise ValueError(f'Unsupported macro {name}')
        for setting in ('wait-ms', 'tap-ms'):
            m = re.search(setting + r'\s*=\s*<(\d+)>', body)
            if not m or m[1] != '30': raise ValueError('Macro timing must be 30 ms')
        macros[name] = [key(x) for x in seq]
    layers = re.findall(r'\w+\s*\{\s*display-name\s*=\s*"[^"]+";\s*bindings\s*=\s*<([^>]+)>;\s*sensor-bindings\s*=\s*<([^>]+)>;\s*\}', text)
    if not layers: raise ValueError('No supported layers found')
    if len(layers)!=len(re.findall(r'display-name\s*=',text)):
        raise ValueError('A layer contains unsupported syntax')
    # Validate the custom sensor behavior instead of silently changing semantics.
    if not re.search(r'bindings\s*=\s*<&msc MOVE_Y\(-100\)>\s*,\s*<&msc MOVE_Y\(100\)>', text):
        raise ValueError('Unsupported encoder scroll bindings')
    if not re.search(r'flavor\s*=\s*"balanced"', text): raise ValueError('Expected balanced layer-tap')
    for setting, value in [('tapping-term-ms',200),('quick-tap-ms',150)]:
        if not re.search(setting+r'\s*=\s*<'+str(value)+r'>', text):
            raise ValueError(f'Expected {setting}={value}')
    def binding(raw):
        parts = raw.split()
        kind, mods, code, page, layer = 'B_NONE',0,0,0,0
        if parts == ['&none']: pass
        elif parts == ['&trans']: kind='B_TRANS'
        elif len(parts)==2 and parts[0]=='&kp':
            kind='B_KEY'; page,code,mods=key(parts[1])
        elif len(parts)==3 and parts[0]=='&lt':
            kind='B_LT'; page,code,mods=key(parts[2]); layer=int(defines.get(parts[1],parts[1]))
            if not 0 < layer < len(layers): raise ValueError('Invalid layer')
        elif len(parts)==1 and parts[0][1:] in macros:
            kind='B_MACRO'; code=list(macros).index(parts[0][1:])
        else: raise ValueError(f'Unsupported binding: {raw}')
        return f'{{{kind},{mods},{code},{page},{layer}}}'
    schema=int.from_bytes(hashlib.sha256(json.dumps([1,len(layers),list(macros.items())],separators=(',',':')).encode()).digest()[:4],'little')
    out=['// Generated from config/cornix.keymap; do not edit.', '#pragma once', '#include "engine.h"',
         f'#define KEYMAP_SCHEMA {schema}u',
         f'#define LAYER_COUNT {len(layers)}', 'static const struct binding keymap[LAYER_COUNT][50] = {']
    for raw, sensors in layers:
        if ' '.join(sensors.split()) != '&encoder_scroll &inc_dec_kp C_VOL_UP C_VOL_DN':
            raise ValueError('Unsupported sensor bindings')
        entries=re.findall(r'&[^&]+', raw)
        if len(entries)!=50: raise ValueError(f'Expected 50 positions, got {len(entries)}')
        out.append('{' + ','.join(binding(x) for x in entries) + '},')
    out += ['};','static const struct binding macros[][8] = {']
    for seq in macros.values():
        if not 0<len(seq)<=8: raise ValueError('Macro length must be 1..8')
        out.append('{'+','.join(f'{{B_KEY,{m},{c},{p},0}}' for p,c,m in seq)+'},')
    out += ['};', 'static const unsigned macro_lengths[] = {'+','.join(str(len(v)) for v in macros.values())+'};']
    return '\n'.join(out)+'\n'

if __name__ == '__main__':
    Path(sys.argv[2]).write_text(compile_keymap(Path(sys.argv[1]).read_text(encoding='utf-8')), encoding='utf-8')
