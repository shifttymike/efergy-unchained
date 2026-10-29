// Exercise the real embedded script with slow/failing HTTP and visibility events.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const elements = new Map(), events = new Map(), timers = new Map();
let nextTimer = 1, requests = 0, active = 0, maxActive = 0;
let hold = true, rejectStatus = false, release;
const config = {device_name:'Efergy gateway',host:'broker.local',port:1883,voltage_v:230,username:'mqtt',username_set:true,password_set:true};
const status = {valid_readings:1,current_mantissa:8192,current_shift:13,interval:6,rssi_raw:180,voltage_v:230,battery_ok:1,mqtt_state:4,mqtt_error:0,ip:'192.168.23.13',legacy_id:827781,radio_captures:1};
const document = {
  hidden:false,title:'',head:{appendChild(){}},createElement(){return {};},
  getElementById(id){if(!elements.has(id))elements.set(id,{value:'',textContent:'',className:'',checked:false,disabled:false,addEventListener(){}});return elements.get(id);},
  addEventListener(type,fn){events.set(type,fn);}
};
const sandbox = {document,AbortController,URLSearchParams,Date,Math:Object.assign(Object.create(Math),{random:()=>0}),
  setTimeout(fn,delay){const id=nextTimer++;timers.set(id,{fn,delay});return id;},
  clearTimeout(id){timers.delete(id);},
  async fetch(path){
    ++requests;++active;maxActive=Math.max(maxActive,active);
    try{
      if(path==='/status'&&hold)await new Promise(resolve=>{release=resolve;});
      if(path==='/status'&&rejectStatus)throw Error('network unavailable');
      return {ok:true,json:async()=>path==='/config'?config:status};
    }finally{--active;}
  }
};
const source=fs.readFileSync('assets/index.html','utf8').match(/<script>([\s\S]*?)<\/script>/)[1];
vm.createContext(sandbox);vm.runInContext(source,sandbox);
async function settle(){for(let i=0;i<8;i++)await Promise.resolve();}
function scheduledDelay(){assert.equal(timers.size,1);return [...timers.values()][0].delay;}
async function fire(){const [id,timer]=[...timers.entries()][0];timers.delete(id);await timer.fn();await settle();}
(async()=>{
  await settle();assert.equal(elements.get('username').value,'mqtt');assert.equal(elements.get('password').value,'');assert.equal(requests,2);assert.equal(active,1);
  await sandbox.poll();await sandbox.poll();assert.equal(requests,2); // No concurrent poll while response is pending.
  hold=false;release();await settle();assert.equal(active,0);assert.equal(scheduledDelay(),3000);
  await fire();assert.equal(scheduledDelay(),3000);assert.equal(maxActive,1);
  rejectStatus=true;await fire();assert.equal(scheduledDelay(),6000);
  await fire();assert.equal(scheduledDelay(),12000);await fire();assert.equal(scheduledDelay(),15000);
  rejectStatus=false;await fire();assert.equal(scheduledDelay(),3000);
  document.hidden=true;events.get('visibilitychange')();assert.equal(timers.size,0);
  const before=requests;await sandbox.poll();assert.equal(requests,before);
  document.hidden=false;events.get('visibilitychange')();await settle();assert.equal(requests,before+1);assert.equal(scheduledDelay(),3000);
  sandbox.Math.random=()=>0.5;await fire();assert.equal(scheduledDelay(),3250);
  console.log('UI polling: completion-based 3s schedule, no overlap, bounded retry backoff, recovery and hidden-page pause passed');
})().catch(error=>{console.error(error);process.exitCode=1;});
