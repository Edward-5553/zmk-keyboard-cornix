// Shared by the standalone HTML and Node regression tests. No device access here.
function createStopWatchCodec(config) {
  const mods={LC:1,LS:2,LA:4,LG:8,RC:16,RS:32,RA:64,RG:128};
  const layers=['BASE','NUM_NAV','FN_SYMBOLS','SCROLL','SNIPE'];
  const reverse=new Map();
  for(const [name,[page,code,m]] of Object.entries(config.keys))if(!m&&!reverse.has(`${page}:${code}`))reverse.set(`${page}:${code}`,name);
  function key(value) {
    const wrap=/^(LC|LS|LA|LG|RC|RS|RA|RG)\((.+)\)$/.exec(value);
    if(wrap){const k=key(wrap[2]);k[2]|=mods[wrap[1]];return k;}
    if(!config.keys[value])throw Error(`暂不支持键码 ${value}`);
    return [...config.keys[value]];
  }
  function parse(value) {
    const s=value.trim().replace(/\s+/g,' ').split(' ');
    if(s.length===1&&s[0]==='&none')return [0,0,0,0,0,0];
    if(s.length===1&&s[0]==='&trans')return [1,0,0,0,0,0];
    const macro=config.macros.indexOf(s[0].slice(1));
    if(s.length===1&&macro>=0)return [4,0,macro,0,0,0];
    let kind,layer=0,k;
    if(s.length===2&&s[0]==='&kp'){kind=2;k=key(s[1]);}
    else if(s.length===3&&s[0]==='&lt') {
      kind=3;layer=layers.indexOf(s[1]);if(layer<0&&/^[1-4]$/.test(s[1]))layer=Number(s[1]);
      if(layer<1||layer>4)throw Error('长按目标层须为 L1–L4');k=key(s[2]);
      if(k[0]!==7)throw Error('轻点/长按暂不支持多媒体键');
    } else throw Error(`StopWatch 暂不支持 ${value}`);
    if(k[0]===12&&k[2])throw Error('多媒体键暂不支持组合修饰键');
    return [kind,k[2],k[1]&255,k[1]>>>8,k[0],layer];
  }
  function format(a) {
    if(a[0]===0)return '&none';if(a[0]===1)return '&trans';
    const code=a[2]|a[3]<<8;
    if(a[0]===4){if(!config.macros[code])throw Error('未知宏');return '&'+config.macros[code];}
    let k=reverse.get(`${a[4]}:${code}`);if(!k)throw Error(`此页面不能编辑设备键码 ${a[4]}:${code}`);
    for(const [name,bit]of Object.entries(mods))if(a[1]&bit)k=`${name}(${k})`;
    if(a[0]===2)return '&kp '+k;
    if(a[0]===3&&a[5]>0&&a[5]<5)return `&lt ${layers[a[5]]} ${k}`;
    throw Error('未知绑定类型');
  }
  function encode(list) {
    if(list.length!==5||list.some(l=>l.keys.length!==50))throw Error('需要 5 层 × 50 键');
    return Uint8Array.from(list.flatMap((l,li)=>l.keys.flatMap((v,pi)=>{
      try{return parse(v);}catch(e){throw Error(`L${li} / 位置 ${pi}：${e.message}`);}
    })));
  }
  function decode(bytes) {
    if(bytes.length!==1500)throw Error('设备键位数据长度错误');
    return Array.from({length:5},(_,l)=>Array.from({length:50},(_,p)=>format(bytes.slice((l*50+p)*6,(l*50+p+1)*6))));
  }
  function hash(bytes){let h=2166136261;for(const b of bytes)h=Math.imul(h^b,16777619);return h>>>0;}
  return {parse,format,encode,decode,hash,schema:config.schema};
}
if(typeof module!=='undefined')module.exports={createStopWatchCodec};
