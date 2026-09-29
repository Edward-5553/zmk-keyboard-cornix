"""Host tests: python ports/stopwatch/tools/test.py [--cc gcc|clang|path/to/zig]."""
import argparse
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import hashlib
import json
import re

PORT=Path(__file__).resolve().parents[1]
ROOT=PORT.parents[1]
spec=importlib.util.spec_from_file_location('keymap',PORT/'tools/keymap.py')
module=importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
args=argparse.ArgumentParser()
args.add_argument('--cc',default='cc')
cc=args.parse_args().cc
source=(ROOT/'config/cornix.keymap').read_text(encoding='utf-8')
header=module.compile_keymap(source)
assert '#define LAYER_COUNT 5' in header
assert module.key('LC(LA(DELETE))')==(7,76,5)
for bad in [source.replace('&kp ESC','&kp UNKNOWN'), source.replace('&kp ESC','&reset'),
            source.replace('&kp ESC',''),source.replace('tapping-term-ms = <200>','tapping-term-ms = <250>'),
            source.replace('C_VOL_UP C_VOL_DN','C_VOL_DN C_VOL_UP')]:
    try: module.compile_keymap(bad)
    except ValueError: pass
    else: raise AssertionError('Unsupported keymap was accepted')
# Windows antivirus can briefly retain the just-executed EXE; cleanup is best effort.
with tempfile.TemporaryDirectory(prefix='stopwatch-',ignore_cleanup_errors=True) as temp:
    path=Path(temp)
    (path/'keymap_generated.h').write_text(header,encoding='utf-8')
    command=[cc]+(['cc'] if Path(cc).stem=='zig' else [])
    command+=['-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-parameter',
              '-I'+str(PORT/'main'),'-I'+str(path),str(PORT/'main/engine.c'),
              str(PORT/'tests/engine_test.c'),'-o',str(path/'engine-test.exe')]
    subprocess.run(command,check=True)
    subprocess.run([str(path/'engine-test.exe')],check=True)
    for name, sources in [('timing', ['engine.c']), ('usb', ['engine.c','usb.c'])]:
        command=[cc]+(['cc'] if Path(cc).stem=='zig' else [])
        command+=['-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-parameter',
                  '-I'+str(PORT/'tests/stubs'),'-I'+str(PORT/'main'),'-I'+str(path)]
        command += [str(PORT/'main'/s) for s in sources]
        command += [str(PORT/f'tests/{name}_test.c'),'-o',str(path/f'{name}-test.exe')]
        subprocess.run(command,check=True)
        subprocess.run([str(path/f'{name}-test.exe')],check=True)
    command=[cc]+(['cc'] if Path(cc).stem=='zig' else [])
    command+=['-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-parameter',
              '-I'+str(PORT/'main'),'-I'+str(path),str(PORT/'main/engine.c'),
              str(PORT/'main/output_route.c'),str(PORT/'tests/output_route_test.c'),
              '-o',str(path/'output-route-test.exe')]
    subprocess.run(command,check=True)
    subprocess.run([str(path/'output-route-test.exe')],check=True)
    command=[cc]+(['cc'] if Path(cc).stem=='zig' else [])
    command+=['-std=c11','-Wall','-Wextra','-Werror','-I'+str(PORT/'main'),
              str(PORT/'main/display_model.c'),str(PORT/'tests/display_test.c'),
              '-o',str(path/'display-test.exe')]
    subprocess.run(command,check=True)
    subprocess.run([str(path/'display-test.exe')],check=True)
    command=[cc]+(['cc'] if Path(cc).stem=='zig' else [])
    command+=['-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-parameter',
              '-I'+str(PORT/'main'),'-I'+str(path),str(PORT/'main/engine.c'),
              str(PORT/'main/keymap_protocol.c'),str(PORT/'tests/keymap_protocol_test.c'),
              '-o',str(path/'keymap-test.exe')]
    subprocess.run(command,check=True)
    subprocess.run([str(path/'keymap-test.exe')],check=True)
    command=[cc]+(['cc'] if Path(cc).stem=='zig' else [])
    command+=['-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-parameter',
              '-I'+str(PORT/'tests/stubs'),'-I'+str(PORT/'main'),'-I'+str(path),
              str(PORT/'main/engine.c'),str(PORT/'main/keymap_protocol.c'),
              str(PORT/'main/keymap_config.c'),str(PORT/'tests/keymap_store_test.c'),
              '-o',str(path/'keymap-store-test.exe')]
    subprocess.run(command,check=True)
    subprocess.run([str(path/'keymap-store-test.exe')],check=True)
    command=[cc]+(['cc'] if Path(cc).stem=='zig' else [])
    command+=['-std=c11','-Wall','-Wextra','-Werror','-I'+str(PORT/'main'),
              str(PORT/'main/orientation.c'),str(PORT/'tests/orientation_test.c'),
              '-lm','-o',str(path/'orientation-test.exe')]
    subprocess.run(command,check=True)
    subprocess.run([str(path/'orientation-test.exe')],check=True)
for name, entry in json.loads((PORT/'main/assets/manifest.json').read_text()).items():
    data=(PORT/f'main/assets/cat_{name}.bin').read_bytes()
    assert len(data)==entry['bytes']==entry['width']*entry['height']*2*entry['frames']
    assert hashlib.sha256(data).hexdigest()==entry['output_sha256']
    assert hashlib.sha256((PORT/f'design/assets/{name}.gif').read_bytes()).hexdigest()==entry['source_sha256']
print('Animation size and checksum tests passed')
font_codes=set(json.loads((PORT/'main/assets/font_codepoints.json').read_text()))
ui=(PORT/'main/display.c').read_text(encoding='utf-8')
for literal in re.findall(r'"([^"\n]*)"',ui):
    assert all(ord(c) in font_codes for c in literal if ord(c)>127), 'Regenerate the UI font subset'
print('UI Chinese font coverage passed')
print('Keymap generation and rejection tests passed')
