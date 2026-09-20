// npm install --prefix .build/stopwatch-editor-tests jsdom@26.1.0
// NODE_PATH=.build/stopwatch-editor-tests/node_modules node ports/stopwatch/tests/editor-dom-test.js
const {JSDOM,VirtualConsole}=require('jsdom');
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {createStopWatchCodec}=require('../tools/editor-codec.js');
const config=require('./editor-config.json'),codec=createStopWatchCodec(config);
const html=fs.readFileSync(path.join(__dirname,'../editor.html'),'utf8');
const preset=JSON.parse(/^const DATA=(.*);$/m.exec(html)[1]);
const stock=codec.encode(preset.layers);
let live=stock.slice(),staging=null,token=0,revision=1,response=new Uint8Array(64),saves=0,failSave=false,allowConfirm=true;
const u32=(a,o)=>(a[o]|a[o+1]<<8|a[o+2]<<16|a[o+3]<<24)>>>0;
const put32=(a,o,v)=>{for(let i=0;i<4;i++)a[o+i]=v>>>(8*i);};
const device={opened:false,productName:'StopWatch Dongle (experimental)',
  async open(){this.opened=true;},async close(){this.opened=false;},
  async sendFeatureReport(id,q){
    assert.equal(id,5);assert.equal(q.length,63);response=new Uint8Array(64);response[0]=5;response.set(q.slice(0,8),1);
    const a=q.slice(8),b=response.subarray(10);let status=0;
    switch(q[3]){
      case 1:b.set([5,50,6,48]);put32(b,4,config.schema);put32(b,8,revision);b[12]=+(saves>0);break;
      case 2:{const o=a[0]|a[1]<<8,n=a[2];b.set([a[0],a[1],n]);b.set((a[3]?stock:live).slice(o,o+n),3);break;}
      case 3:if(u32(a,0)!==revision)status=3;else{token=u32(q,4);staging=new Uint8Array(1500);put32(b,0,token);}break;
      case 4:assert.equal(u32(a,0),token);staging.set(a.slice(7,7+a[6]),a[4]|a[5]<<8);break;
      case 5:assert.equal(u32(a,0),token);assert.equal(u32(a,4),codec.hash(staging));
        if(failSave)status=7;else{live=staging;saves++;revision++;staging=null;put32(b,0,revision);}break;
      case 6:staging=null;break;
      default:throw Error('Unexpected command '+q[3]);
    }
    response[9]=status;
  },async receiveFeatureReport(){return new DataView(response.buffer);}
};
function open(text){
  const errors=[];const console=new VirtualConsole();console.on('jsdomError',e=>errors.push(e));
  const dom=new JSDOM(text,{url:'https://stopwatch.test/',runScripts:'dangerously',virtualConsole:console,
    beforeParse(w){w.confirm=()=>allowConfirm;Object.defineProperty(w.navigator,'hid',{value:{requestDevice:async()=>[device],addEventListener(){}}});}});
  assert.equal(errors.length,0,errors.map(e=>e.message).join('\n'));return dom;
}
(async()=>{
  const dom=open(html),w=dom.window,$=id=>w.document.getElementById(id);
  assert.equal(w.document.querySelectorAll('#board .key').length,50);
  await $('device-connect').onclick();assert.equal($('device-save').disabled,false);
  w.eval('edit(1)');w.document.dispatchEvent(new w.KeyboardEvent('keydown',{key:'z',code:'KeyZ',bubbles:true,cancelable:true}));
  assert.equal(w.eval('DATA.layers[0].keys[1]'),'&kp Z');
  assert.equal(live[8],20); // Merely editing never changes the device.
  await $('device-save').onclick();assert.equal(live[8],29);assert.equal(saves,1);
  assert.match($('device-status').textContent,/写入成功/);
  await $('device-defaults').onclick();assert.equal(live[8],29);assert.equal(saves,1);
  allowConfirm=false;await $('device-save').onclick();assert.equal(saves,1);allowConfirm=true;
  failSave=true;await $('device-save').onclick();assert.equal(live[8],29);assert.match($('device-status').textContent,/存储失败/);
  assert.equal($('device-save').disabled,true);failSave=false;
  await $('device-read').onclick();assert.equal(codec.parse(w.eval('DATA.layers[0].keys[1]'))[2],29);
  const exported=w.eval('exportHtml()');const restored=open(exported);
  assert.equal(restored.window.document.querySelectorAll('#device-toolbar').length,1);
  restored.window.close();
  await $('device-defaults').onclick();await $('device-save').onclick();assert.deepEqual(live,stock);
  await $('device-close').onclick();assert.equal($('device-save').disabled,true);
  dom.window.close();console.log('DOM + mock USB: connect/read, physical capture, save/readback, cancel, defaults, storage error, export and disconnect passed');
})().catch(e=>{console.error(e);process.exit(1);});
