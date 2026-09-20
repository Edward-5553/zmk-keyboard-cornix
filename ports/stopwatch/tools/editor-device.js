// Injected after the existing editor script; the generated HTML is self-contained.
const deviceBar=document.createElement('section');deviceBar.id='device-toolbar';deviceBar.className='panel';
deviceBar.innerHTML='<h2>StopWatch · 在线改键</h2><div class="bar"><button id="device-connect">连接 StopWatch</button><button id="device-read" disabled>读取设备键位</button><button id="device-save" disabled>写入并保存到设备</button><button id="device-defaults" disabled>恢复默认到草稿</button><button id="device-close" disabled>断开</button></div><p id="device-status" role="status">未连接 · 先刷入支持在线改键的 StopWatch 固件，再用桌面 Chrome / Edge 打开本页。</p><p>点选键位后按笔记本或其他键盘录入，也可拖动交换。只有点击“写入并保存到设备”才会改变设备；成功保存后断电保留。旋钮转动功能固定，旋钮按压可以改键。</p>';
$('tabs').before(deviceBar);
let hidDevice=null,deviceBusy=false,deviceRevision=null,deviceBaseline=null;
let sequence=crypto.getRandomValues(new Uint32Array(1))[0];
const deviceMessage=text=>$('device-status').textContent=text;
function deviceButtons(){
  for(const element of [$('board').closest('.panel'),$('detail'),$('tabs'),$('editor-toolbar')])element.inert=deviceBusy;
  $('device-connect').disabled=deviceBusy||!!hidDevice;
  for(const id of ['device-read','device-defaults','device-close'])$(id).disabled=deviceBusy||!hidDevice;
  $('device-save').disabled=deviceBusy||!hidDevice||deviceRevision===null;
}
function bytes32(n){return [n&255,(n>>>8)&255,(n>>>16)&255,(n>>>24)&255];}
function number32(b,o=0){return (b[o]|b[o+1]<<8|b[o+2]<<16|b[o+3]<<24)>>>0;}
const rpcErrors=['','请求格式错误','页面与固件版本不兼容，请使用对应版本编辑器','设备键位已被其他窗口修改，请先重新读取','设备正忙，请松开所有键，或稍后重试','传输顺序错误，请重新保存','键位未通过校验，未保存','设备存储失败，未切换当前键位','写入会话已过期，请重新保存'];
async function rpc(command,payload=[]) {
  const dev=hidDevice;if(!dev?.opened)throw Error('设备已断开');
  const seq=sequence=(sequence+1)>>>0;
  const request=new Uint8Array(63);request.set([83,75,1,command,...bytes32(seq)]);request.set(payload,8);
  await dev.sendFeatureReport(5,request);
  const deadline=Date.now()+6000;
  while(Date.now()<deadline) {
    const report=await dev.receiveFeatureReport(5);
    const raw=new Uint8Array(report.buffer,report.byteOffset,report.byteLength);
    if(raw.length!==64||raw[0]!==5)throw Error('设备协议不匹配，请更新固件');
    const b=raw.subarray(1);
    if(b[0]===83&&b[1]===75&&b[2]===1&&b[3]===command&&number32(b,4)===seq) {
      if(b[8])throw Error(rpcErrors[b[8]]||`设备错误 ${b[8]}`);
      return b.slice(9);
    }
    await new Promise(resolve=>setTimeout(resolve,15));
  }
  throw Error('设备响应超时；若刚才在保存，请重新读取确认实际结果');
}
async function getInfo(){
  const info=await rpc(1);
  if(info[0]!==5||info[1]!==50||info[2]!==6||info[3]!==48||number32(info,4)!==SWCodec.schema)
    throw Error('页面与固件的层数或行为版本不一致，请用本次固件对应的 editor.html');
  return {revision:number32(info,8),saved:!!info[12]};
}
async function readBytes(defaults=false) {
  const before=await getInfo(),bytes=new Uint8Array(1500);
  for(let offset=0;offset<bytes.length;offset+=48) {
    const count=Math.min(48,bytes.length-offset),result=await rpc(2,[offset&255,offset>>>8,count,+defaults]);
    if((result[0]|result[1]<<8)!==offset||result[2]!==count)throw Error('读取数据不完整，请重试');
    bytes.set(result.subarray(3,3+count),offset);
  }
  if((await getInfo()).revision!==before.revision)throw Error('读取期间设备键位发生变化，请重试');
  return {bytes,info:before};
}
function installDraft(bytes) {
  const keys=SWCodec.decode(bytes);history.push(JSON.stringify(DATA.layers));
  DATA.layers.forEach((layer,i)=>layer.keys=keys[i]);
  selected=null;persist();render();$('detail').textContent='点击键位，再按实体键盘录入。';
}
async function readDevice(){
  if(dirty&&!confirm('读取设备键位会替换当前页面草稿。是否继续？'))return;
  const {bytes,info}=await readBytes();installDraft(bytes);
  deviceRevision=info.revision;deviceBaseline=bytes;dirty=false;
  status('已读取设备键位 · 修改后请写入保存');
  deviceMessage(`已连接 · 已读取${info.saved?'已保存键位':'固件默认键位'} · 可开始编辑`);
}
async function operation(fn) {
  if(deviceBusy)return;deviceBusy=true;deviceButtons();
  try{await fn();}catch(e){deviceMessage(e.message+'（页面草稿保留）');}
  finally{deviceBusy=false;deviceButtons();}
}
$('device-connect').onclick=()=>operation(async()=>{
  if(!navigator.hid)throw Error('此浏览器不支持 WebHID，请在桌面 Chrome / Edge 中打开');
  const devices=await navigator.hid.requestDevice({filters:[{usagePage:0xff50,usage:1}]});
  if(!devices.length)return;
  const selectedDevice=devices[0];hidDevice=selectedDevice;
  try{await selectedDevice.open();await getInfo();await readDevice();}
  catch(e){await selectedDevice.close().catch(()=>{});hidDevice=null;deviceRevision=null;throw e;}
});
$('device-read').onclick=()=>operation(readDevice);
$('device-defaults').onclick=()=>operation(async()=>{
  if(!confirm('将固件默认键位载入页面草稿？设备暂不改变，之后可检查并点击“写入并保存”。'))return;
  const {bytes}=await readBytes(true);installDraft(bytes);deviceMessage('默认键位已载入草稿 · 尚未写入设备');
});
$('device-save').onclick=()=>operation(async()=>{
  const bytes=SWCodec.encode(DATA.layers);
  let changes=0;for(let i=0;i<250;i++)if(bytes.subarray(i*6,i*6+6).some((v,j)=>v!==deviceBaseline?.[i*6+j]))changes++;
  if(!changes){deviceMessage('键位与设备一致，无需写入');return;}
  if(!confirm(`将 ${changes} 个键位的修改写入 StopWatch 并断电保存？请松开键盘上的所有按键。`))return;
  const draftAtStart=JSON.stringify(DATA.layers);
  deviceMessage('正在写入，请保持 USB 连接…');
  let token=null;
  try {
    const started=await rpc(3,[...bytes32(deviceRevision),...bytes32(SWCodec.schema)]);token=number32(started);
    for(let offset=0;offset<bytes.length;offset+=48) {
      const part=bytes.slice(offset,offset+48);
      await rpc(4,[...bytes32(token),offset&255,offset>>>8,part.length,...part]);
    }
    await rpc(5,[...bytes32(token),...bytes32(SWCodec.hash(bytes))]);token=null;
    const verified=await readBytes();
    if(!verified.bytes.every((v,i)=>v===bytes[i]))throw Error('保存后回读不一致，请重新读取设备');
    deviceRevision=verified.info.revision;deviceBaseline=verified.bytes;
    dirty=JSON.stringify(DATA.layers)!==draftAtStart;
    deviceMessage(dirty?'设备已保存本次发送的键位 · 页面仍有之后的修改未写入':'写入成功并已回读核对 · 立即生效，断电保留');
    status(dirty?'页面有新修改待写入':'已保存到 StopWatch');
  } catch(e) {
    if(token!==null)await rpc(6,bytes32(token)).catch(()=>{});
    deviceRevision=null;throw Error(e.message+'；请重新读取设备后再保存');
  }
});
$('device-close').onclick=()=>operation(async()=>{
  await hidDevice.close();hidDevice=null;deviceRevision=null;deviceBaseline=null;deviceMessage('已断开 · 页面草稿保留');
});
navigator.hid?.addEventListener('disconnect',e=>{
  if(e.device===hidDevice){hidDevice=null;deviceRevision=null;deviceBaseline=null;deviceButtons();deviceMessage('USB 已断开 · 重新连接并读取后再保存');}
});
if(!navigator.hid)deviceMessage('请使用桌面 Chrome / Edge 打开本页；当前浏览器不支持设备连接，仍可离线编辑。');
deviceButtons();
