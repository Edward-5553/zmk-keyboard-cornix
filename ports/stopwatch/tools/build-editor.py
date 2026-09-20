"""Generate self-contained editor.html using the existing Cornix editor and key names."""
from pathlib import Path
import importlib.util
import json
import re

PORT=Path(__file__).resolve().parents[1]
ROOT=PORT.parents[1]
spec=importlib.util.spec_from_file_location('keymap',PORT/'tools/keymap.py')
keymap=importlib.util.module_from_spec(spec);spec.loader.exec_module(keymap)
source=(ROOT/'config/cornix.keymap').read_text(encoding='utf-8')
header=keymap.compile_keymap(source)
source=re.sub(r'/\*.*?\*/|//[^\n]*','',source,flags=re.S)
schema=int(re.search(r'#define KEYMAP_SCHEMA (\d+)u',header)[1])
macros=re.findall(r'(\w+):\s*\w+\s*\{\s*compatible\s*=\s*"zmk,behavior-macro"',source)
config={'keys':keymap.KEYS,'macros':macros,'schema':schema}
html=(ROOT/'keymap-drawer/cornix.html').read_text(encoding='utf-8')
data=json.loads(re.search(r'^const DATA=(.*);$',html,re.M)[1])
layer_bindings=re.findall(r'display-name\s*=\s*"[^"]+";\s*bindings\s*=\s*<([^>]+)>',source)
assert len(layer_bindings)==5
for layer,bindings in zip(data['layers'],layer_bindings):
    layer['keys']=[' '.join(b.split()) for b in re.findall(r'&[^&]+',bindings)]
html=re.sub(r'^const DATA=.*;$',lambda _: 'const DATA='+json.dumps(data,ensure_ascii=False)+';',html,flags=re.M)
codec=(PORT/'tools/editor-codec.js').read_text(encoding='utf-8')
html=html.replace('const $ =',codec+'\nconst SWCodec=createStopWatchCodec('+json.dumps(config)+');\nconst $ =',1)
html=html.replace("const initial=JSON.stringify(DATA.layers);","for(const binding of options.keys()){try{SWCodec.parse(binding);}catch{options.delete(binding);}}\nconst initial=JSON.stringify(DATA.layers);")
html=re.sub(r'^function valid\(value\).*$', 'function valid(value) {try{SWCodec.parse(value);return true;}catch{return false;}}',html,flags=re.M)
html=html.replace("cornix-keymap-editor-v1-","stopwatch-keymap-editor-v1-")
html=html.replace('导出后交给我同步到固件。','通过 USB 直接写入 StopWatch，断电保存。')
html=html.replace('你的键位，一眼看清。','StopWatch，直接改键。')
html=html.replace('Cornix · 键位编辑器','StopWatch · 在线键位编辑器')
html=html.replace('CORNIX / KEYMAP','CORNIX / STOPWATCH')
html=html.replace('修改会自动暂存在当前浏览器。完成后务必点击「导出修改后的 HTML」，将下载的文件发给我或告诉我保存路径。普通刷新不会把修改写回原文件。',
    '连接后先读取设备键位，编辑完成点击「写入并保存到设备」。浏览器草稿与导出 HTML 只是备份，不会自动写入设备。写入成功后无需重新刷机，重启仍保留。恢复默认只改变草稿，检查后仍须写入。')
html=html.replace('这里只编辑预设，不会操作键盘设备。','仅适配 StopWatch 在线改键固件。支持已有宏的分配，不支持编辑宏内容；暂不支持蓝牙控制、组合触发或新行为。')
html=html.replace('特殊功能 / 手动绑定（切层、蓝牙、透明键等）','特殊功能 / 手动绑定（切层、透明键、已有宏）')
html=html.replace('已暂存到浏览器 · 完成后请导出 HTML','草稿已暂存 · 尚未写入设备')
html=html.replace('已恢复浏览器草稿 · 完成后请导出 HTML','已恢复浏览器草稿 · 连接后先读取设备')
html=html.replace('cornix-edited.html','stopwatch-edited.html')
html=html.replace('请确认保存后提供文件路径','仅为离线备份，设备未改变')
html=html.replace("dirty=false;status('已发起下载", "status('已发起下载")
html=html.replace("root.querySelector('#editor-toolbar')?.remove();","root.querySelector('#device-toolbar')?.remove();root.querySelector('#editor-toolbar')?.remove();")
html=html.replace('</script>',(PORT/'tools/editor-device.js').read_text(encoding='utf-8')+'\n</script>')
(PORT/'editor.html').write_text(html,encoding='utf-8')
(PORT/'tests/editor-config.json').write_text(json.dumps(config,indent=2)+'\n',encoding='utf-8')
print('Generated editor.html; schema',schema)
