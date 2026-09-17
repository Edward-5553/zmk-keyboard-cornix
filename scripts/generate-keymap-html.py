"""Generate an offline preview from the user keymap and physical layout."""
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / 'config/cornix.keymap').read_text(encoding='utf-8')
source = re.sub(r'/\*.*?\*/|//[^\n]*', '', source, flags=re.S)
layers = []
for name, body in re.findall(r'(\w+_layer)\s*\{(.*?)\};', source, re.S):
    bindings = re.search(r'(?<![\w-])bindings\s*=\s*<(.*?)>;', body, re.S)
    keys = [' '.join(b.split()) for b in re.findall(r'&[^&]+', bindings[1])]
    assert len(keys) == 50, (name, len(keys))
    layers.append({'name': name, 'keys': keys})
layout = (ROOT / 'boards/jzf/cornix/cornix-layouts.dtsi').read_text()
positions = []
for values in re.findall(r'<&key_physical_attrs\s+([^>]+)>', layout):
    positions.append([int(v) / 100 for v in re.findall(r'-?\d+', values)])
assert len(positions) == 50 and len(layers) == 5
data = json.dumps({'layers': layers, 'positions': positions}, ensure_ascii=False)
template = (ROOT / 'scripts/keymap-editor-template.html').read_text(encoding='utf-8')
editor = (ROOT / 'scripts/keymap-editor.js').read_text(encoding='utf-8')
out = ROOT / 'keymap-drawer/cornix.html'
out.parent.mkdir(exist_ok=True)
out.write_text(template.replace('__DATA__', data).replace('__EDITOR__', editor), encoding='utf-8')
print(f'Generated {out}: {len(layers)} layers, all 50 positions visible by default.')
