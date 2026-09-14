// Actual production reducers and event settlement; no audio server or HID.
const assert = require('node:assert/strict');
const vm = require('node:vm');
const {service:source,read} = require('../qml-source.js');
function fixture() {
  const writes=[];
  const timer=()=>({running:false,restart(){this.running=true},start(){this.running=true},stop(){this.running=false}});
  const ctx=vm.createContext({hostReady:true,receiver:true,detectedCallContext:false,requestedCallContextActive:false,
    inactiveCallPolls:0,unknownCallPolls:0,nonpositiveCallPolls:0,settings:{alwaysCallContext:true},
    deviceWatchProc:{running:true,write(x){writes.push(x)}},themeColorDelay:timer(),settingsRequestTimeout:timer(),modeRequestTimeout:timer(),deviceWatchRestart:timer()});
  ctx.root=ctx;
  for(const name of ['updateCallContext','applyCallContext','resetCallDetection','clearDeviceState','watcherStopped'])
    vm.runInContext(source.match(new RegExp(`^  function ${name}\\([^)]*\\) \\{[\\s\\S]*?^  \\}`,'m'))[0],ctx);
  return {ctx,writes};
}
{
 const {ctx,writes}=fixture(); ctx.updateCallContext(true); assert.deepEqual(writes,['call off\n']);
 ctx.applyCallContext('active'); ctx.applyCallContext('active'); assert.deepEqual(writes,['call off\n','call on\n']);
 ctx.applyCallContext('inactive'); assert.equal(ctx.detectedCallContext,true);
 ctx.applyCallContext('inactive'); assert.equal(ctx.detectedCallContext,false); assert.equal(writes.at(-1),'call off\n');
}
for(const inputs of [['unknown','inactive','unknown'],['inactive','unknown','inactive'],['unknown','','active\nunknown']]) {
 const {ctx}=fixture(); ctx.applyCallContext('active');
 inputs.forEach((x,i)=>{ctx.applyCallContext(x);assert.equal(ctx.detectedCallContext,i<2)});
 for(let i=0;i<100;i++) ctx.applyCallContext('unknown'); assert.equal(ctx.nonpositiveCallPolls,3);
 ctx.applyCallContext('active'); assert.equal(ctx.nonpositiveCallPolls,0);
}
{
 const {ctx,writes}=fixture(); ctx.deviceWatchProc.running=false; ctx.applyCallContext('active'); assert.equal(writes.length,0);
 ctx.deviceWatchProc.running=true; ctx.updateCallContext(true); assert.equal(writes.at(-1),'call on\n');
 ctx.receiver=false; ctx.resetCallDetection(); ctx.updateCallContext(); assert.equal(writes.at(-1),'call off\n');
 ctx.hostReady=false; ctx.detectedCallContext=true; ctx.updateCallContext(true); assert.equal(writes.at(-1),'call off\n');
}
{
 const {ctx}=fixture(); ctx.deviceWatchProc.running=false; ctx.watcherStopped(); assert.equal(ctx.requestedCallContextActive,false);
 assert.equal(ctx.callContextActive,false); assert.equal(ctx.deviceWatchRestart.running,true);
}
{
 const {ctx}=fixture(); const d=read('CallDetector.qml'); ctx.observation='inactive';ctx.confirmations=0;
 ctx.settle={running:false,start(){this.running=true},stop(){this.running=false}};
 vm.runInContext(d.match(/^  function publish\([^)]*\) \{[\s\S]*?^  \}/m)[0],ctx);
 ctx.applyCallContext('active');ctx.publish(); assert.equal(ctx.detectedCallContext,true);assert.equal(ctx.settle.running,true);
 ctx.observation='active';ctx.publish(); assert.equal(ctx.settle.running,false);assert.equal(ctx.detectedCallContext,true);
 assert.doesNotMatch(d,/Process|repeat: true|pactl/);
}
console.log('Call-context: event admission, bounded loss debounce, startup reconciliation, no legacy override passed');
