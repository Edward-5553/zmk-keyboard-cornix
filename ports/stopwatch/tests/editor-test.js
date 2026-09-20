const assert=require('node:assert/strict');
const fs=require('node:fs');
const path=require('node:path');
const vm=require('node:vm');
const {createStopWatchCodec}=require('../tools/editor-codec.js');
const config=require('./editor-config.json');
const codec=createStopWatchCodec(config);
const html=fs.readFileSync(path.join(__dirname,'../editor.html'),'utf8');
const data=JSON.parse(/^const DATA=(.*);$/m.exec(html)[1]);
const encoded=codec.encode(data.layers);
assert.equal(encoded.length,1500);
const decoded=codec.decode(encoded);
assert.deepEqual(codec.encode(decoded.map(keys=>({keys}))),encoded);
for(const key of Object.keys(config.keys)) {
  const binding='&kp '+key;
  assert.deepEqual(codec.parse(codec.format(codec.parse(binding))),codec.parse(binding));
}
for(const binding of ['&kp LC(LA(DELETE))','&lt NUM_NAV SPACE','&pair_angles','&kp RG(RS(A))'])
  assert.deepEqual(codec.parse(codec.format(codec.parse(binding))),codec.parse(binding));
for(const invalid of ['&bt BT_CLR','&reset','&lt BASE SPACE','&lt 9 SPACE','&kp FAKE','&kp LC(C_MUTE)'])
  assert.throws(()=>codec.parse(invalid));
assert.throws(()=>codec.decode(new Uint8Array(1499)));
new vm.Script(/<script>([\s\S]*)<\/script>/.exec(html)[1]);
assert.equal(codec.hash(Uint8Array.from([1,2,3])),1456420779);
console.log('HTML syntax, all 250 bindings, keyboard codes, modifiers, macros, invalid input and wire roundtrip passed');
