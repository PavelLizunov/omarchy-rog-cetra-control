const assert = require('node:assert/strict');
const vm = require('node:vm');
const {read}=require('./qml-source.js');
const ctx=vm.createContext({});
for(const name of ['luminance','readableWarning'])
  vm.runInContext(read('CetraViewModel.qml').match(new RegExp(`^  function ${name}\\([^)]*\\) \\{[\\s\\S]*?^  \\}`,'m'))[0],ctx);
const rgb=h=>({r:parseInt(h.slice(0,2),16)/255,g:parseInt(h.slice(2,4),16)/255,b:parseInt(h.slice(4,6),16)/255});
const bg=rgb('101315'),fg=rgb('cacccc'),urgent=rgb('565d60');
assert.equal(ctx.readableWarning(urgent,bg,fg),fg);
assert.equal(ctx.readableWarning(fg,bg,urgent),fg);
const white=rgb('ffffff'),black=rgb('000000'),lightWarning=rgb('eeeeee');
assert.equal(ctx.readableWarning(lightWarning,white,black),black);
assert.equal(ctx.readableWarning(black,white,lightWarning),black);
console.log('PASS warning contrast: dark/light low-contrast theme token uses foreground fallback');
