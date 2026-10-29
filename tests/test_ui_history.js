const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
let now=1700000000000,requests=0,script;
class Clock extends Date{constructor(...args){super(...(args.length?args:[now]));}static now(){return now;}}
const elements=new Map(),events=new Map();
const document={hidden:false,title:'',head:{appendChild(s){script=s;}},createElement(){return {};},
 getElementById(id){if(!elements.has(id))elements.set(id,{value:'',textContent:'',addEventListener(){}});return elements.get(id);},
 addEventListener(type,fn){events.set(type,fn);}};
const empty={valid_readings:0,legacy_id:0,interval:0,current_mantissa:0,current_shift:0,rssi_raw:255,voltage_v:230,mqtt_state:4};
const ctx={document,Date:Clock,Math,AbortController,URLSearchParams,setTimeout(){return 1;},clearTimeout(){},
 async fetch(path){++requests;return {ok:true,json:async()=>path==='/config'?{device_name:'Efergy gateway',port:1883,voltage_v:230}:empty};}};
vm.createContext(ctx);vm.runInContext(fs.readFileSync('assets/index.html','utf8').match(/<script>([\s\S]*?)<\/script>/)[1],ctx);
const history=()=>JSON.parse(vm.runInContext('JSON.stringify(history)',ctx));
function sample(count,voltage=230){ctx.recordHistory({...empty,valid_readings:count,legacy_id:827781,interval:6,current_mantissa:8192,current_shift:13,voltage_v:voltage},now);}
(async()=>{
 for(let i=0;i<8;i++)await Promise.resolve();
 assert.equal(history().length,0);assert.equal(requests,2);
 assert(script.src.startsWith('https://cdn.jsdelivr.net/npm/chart.js@4.5.1/'));
 assert(script.integrity.startsWith('sha384-'));assert.equal(script.crossOrigin,'anonymous');
 sample(1);assert.equal(history().length,1);assert.equal(history()[0].power,230);
 now+=3000;sample(1,240);assert.equal(history().length,1); // Poll/config change does not duplicate a reading.
 now+=3000;sample(2,240);assert.equal(history().length,2);assert.equal(history()[0].power,230);assert.equal(history()[1].power,240);
 now+=6000;sample(3,240);assert.equal(history().length,3); // Same watts, new reading: retain it.
 let drawn;
 ctx.Chart=class{constructor(el,options){this.data=options.data;this.options=options.options;drawn=this;}update(mode){assert.equal(mode,'none');}};
 script.onload();assert.equal(drawn.data.datasets.length,1);assert.equal(drawn.data.datasets[0].label,'Estimated power');assert.equal(drawn.data.datasets[0].data.length,3);
 now+=6000;sample(6);assert.equal(history().at(-2).power,null);assert.equal(history().at(-1).count,6);
 now+=6000;ctx.recordHistory(empty,now);sample(1);assert.equal(history().at(-2).power,null); // Restart after observing empty startup.
 now+=6000;sample(65535);now+=6000;sample(0);assert.equal(history().at(-2).power,230);assert.equal(history().at(-1).count,0); // 16-bit wrap.
 now=1700003600001;ctx.drawHistory(now);assert(history().every(p=>p.t>=now-3600000));
 now+=3600001;ctx.drawHistory(now);assert.equal(history().length,0);
 script.onerror();assert(document.getElementById('chart_note').textContent.includes('Live readings'));
 const before=requests;sample(10);assert.equal(requests,before);assert.equal(history().length,1); // Plotting never fetches device data.
 console.log('Power history: unique readings, unchanged watts, voltage snapshots, gaps/restart/wrap, one-hour pruning, late CDN loading and no extra requests passed');
})().catch(e=>{console.error(e);process.exitCode=1;});
