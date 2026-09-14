// Execute production QML JavaScript, never a copy of its state machine.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');

const { service: source, widget: widgetSource } = require('../qml-source.js');
const functions = ['updateCallContext', 'persistSetting', 'resetCallDetection', 'applyCallContext', 'finishCallContext', 'clearDeviceState', 'watcherStopped'];
const processBlock = id => source.match(new RegExp(`^  (?:Process|Timer) \\{\\n    id: ${id}\\b[\\s\\S]*?^  \\}`, 'm'))?.[0] || '';
function handler(block, name) {
  const match = block.match(new RegExp(`^    ${name}: (?:function \\(([^)]*)\\) )?\\{\\n([\\s\\S]*?)^    \\}`, 'm'));
  if (match) return `(function(${match[1] || ''}) {${match[2]}})`;
  const line = block.match(new RegExp(`^    ${name}: (.+)$`, 'm'));
  return `(function() {${line ? line[1] : ''}})`;
}

function fixture() {
  const writes = [];
  const updates = [];
  const signals = [];
  const timer = () => ({ running: false, restart() { this.running = true; }, stop() { this.running = false; } });
  const ctx = vm.createContext({
    hostReady: true, service: null,
    requestedCallContextActive: false, detectedCallContext: false, callContextActive: false,
    inactiveCallPolls: 0, unknownCallPolls: 0, nonpositiveCallPolls: 0, settings: {}, moduleName: 'installed.entry.id',
    deviceWatchProc: { running: false, write(value) { if (this.running) writes.push(value); } },
    callContextProc: { running: false, processId: null, pending: false, signal(value) { signals.push(value); } },
    callContextOutput: { text: '' }, callContextTimeout: timer(), deviceWatchRestart: timer(), modeRequestTimeout: timer(),
    settingsRequestTimeout: timer(), themeColorDelay: timer(),
    bar: { shell: { updateEntryInline(id, entry) { updates.push({ id, entry: JSON.parse(JSON.stringify(entry)) }); } } },
  });
  ctx.root = ctx;
  for (const match of processBlock('deviceWatchProc').matchAll(/^    property bool (\w+): (.+)$/gm))
    ctx.deviceWatchProc[match[1]] = vm.runInContext(match[2], ctx);
  for (const name of functions) {
    const owner = name === 'persistSetting' ? widgetSource : source;
    const match = owner.match(new RegExp(`^  function ${name}\\([^)]*\\) \\{[\\s\\S]*?^  \\}`, 'm'));
    if (match) vm.runInContext(match[0], ctx);
  }
  const invoke = (id, event, ...args) => vm.runInContext(handler(processBlock(id), event), ctx)(...args);
  const startPoll = () => {
    invoke('callContextPoll', 'onTriggered');
    assert.equal(ctx.callContextProc.pending, true);
    assert.equal(ctx.callContextTimeout.running, true);
    ctx.callContextProc.processId = 123;
  };
  const endPoll = (text, code = 0, status = 0) => {
    ctx.callContextOutput.text = text;
    ctx.callContextProc.running = false;
    invoke('callContextProc', 'onExited', code, status);
    invoke('callContextProc', 'onRunningChanged');
    assert.equal(ctx.callContextProc.pending, false);
    assert.equal(ctx.callContextTimeout.running, false);
  };
  return { ctx, writes, updates, signals, invoke, startPoll, endPoll };
}

const cases = {
  'obsolete override cannot force a call or stop automatic detection': () => {
    const { ctx, writes, invoke, startPoll, endPoll } = fixture();
    ctx.deviceWatchProc.running = true;
    ctx.settings = { alwaysCallContext: true };
    ctx.receiver = true;
    invoke('deviceWatchProc', 'onStarted');
    assert.deepEqual(writes, ['call off\n']);
    const running = processBlock('callContextPoll').match(/^    running: (.+)$/m)[1];
    assert.equal(vm.runInContext(running, ctx), true);
    startPoll();
    endPoll('active');
    assert.equal(writes.at(-1), 'call on\n', 'A fresh detection can restore call mode');
    ctx.applyCallContext('inactive');
    ctx.applyCallContext('inactive');
    assert.equal(writes.at(-1), 'call off\n');
    assert.doesNotMatch(widgetSource, /alwaysCallContext|setAlwaysCallContext|settings\.experimental|settings\.callHelp/);
    assert.doesNotMatch(source, /alwaysCallContext/);
  },
  'receiver loss invalidates detection and a pending poll': () => {
    const { ctx, writes, startPoll, endPoll } = fixture();
    ctx.deviceWatchProc.running = true;
    ctx.applyCallContext('active');
    startPoll();
    ctx.receiver = false;
    vm.runInContext(source.match(/^  onReceiverChanged: \{\n([\s\S]*?)^  \}/m)[1], ctx);
    endPoll('active');
    assert.equal(ctx.detectedCallContext, false);
    assert.deepEqual(writes, ['call on\n', 'call off\n']);
  },
  'stopped transition stays unsent; start forces call on': () => {
    const { ctx, writes, invoke } = fixture();
    ctx.applyCallContext('active');
    assert.equal(ctx.requestedCallContextActive, false);
    assert.deepEqual(writes, []);
    ctx.deviceWatchProc.running = true;
    invoke('deviceWatchProc', 'onStarted');
    assert.deepEqual(writes, ['call on\n']);
    assert.equal(ctx.requestedCallContextActive, true);
    ctx.updateCallContext();
    assert.equal(writes.length, 1);
  },
  'watcher exit clears sent/reported state; restart resends desired': () => {
    const { ctx, writes, invoke } = fixture();
    ctx.deviceWatchProc.running = true;
    ctx.applyCallContext('active');
    ctx.callContextActive = true;
    ctx.deviceWatchProc.running = false;
    invoke('deviceWatchProc', 'onExited');
    assert.equal(ctx.requestedCallContextActive, false);
    assert.equal(ctx.callContextActive, false);
    assert.equal(ctx.detectedCallContext, true);
    assert.equal(ctx.deviceWatchRestart.running, true);
    ctx.deviceWatchProc.running = true;
    invoke('deviceWatchProc', 'onStarted');
    assert.deepEqual(writes, ['call on\n', 'call on\n']);
    assert.equal(ctx.deviceWatchRestart.running, false);
  },
  'successful start cancels pending retry and resends current call intent': () => {
    for (const active of [false, true]) {
      const { ctx, writes, invoke } = fixture();
      ctx.detectedCallContext = !active;
      ctx.requestedCallContextActive = !active;
      ctx.deviceWatchRestart.restart();
      ctx.detectedCallContext = active;
      ctx.deviceWatchProc.running = true;
      invoke('deviceWatchProc', 'onRunningChanged');
      invoke('deviceWatchProc', 'onStarted');
      assert.equal(ctx.deviceWatchRestart.running, false);
      assert.deepEqual(writes, [active ? 'call on\n' : 'call off\n']);
      assert.equal(ctx.requestedCallContextActive, active);
      assert.equal(ctx.callContextActive, false);
    }
  },
  'forced start also reconciles media mode': () => {
    const { ctx, writes, invoke } = fixture();
    ctx.deviceWatchProc.running = true;
    invoke('deviceWatchProc', 'onStarted');
    assert.deepEqual(writes, ['call off\n']);
  },
  'unknown resets inactive debounce; third nonpositive result falls back': () => {
    const { ctx, writes } = fixture();
    ctx.deviceWatchProc.running = true;
    ctx.applyCallContext('active');
    ctx.applyCallContext('inactive');
    ctx.applyCallContext('unknown');
    assert.equal(ctx.inactiveCallPolls, 0);
    ctx.applyCallContext('inactive');
    assert.equal(ctx.detectedCallContext, false);
    assert.equal(ctx.unknownCallPolls, 0);
    for (const text of ['unknown', '', 'active\nunknown']) ctx.applyCallContext(text);
    assert.equal(ctx.detectedCallContext, false);
    assert.deepEqual(writes, ['call on\n', 'call off\n']);
    for (let i = 0; i < 10; i++) ctx.applyCallContext('unknown');
    assert.equal(ctx.unknownCallPolls, 3);
    ctx.applyCallContext('active');
    assert.equal(ctx.unknownCallPolls, 0);
    assert.equal(ctx.detectedCallContext, true);
  },
  'alternating inactive/unknown is bounded; only active resets budget': () => {
    for (const manual of [false, true]) {
      const { ctx, writes } = fixture();
      ctx.settings = { alwaysCallContext: manual };
      ctx.deviceWatchProc.running = true;
      ctx.applyCallContext('active');
      for (let i = 1; i <= 100; i++) {
        ctx.applyCallContext(i % 2 ? 'unknown' : 'inactive');
        assert.equal(ctx.detectedCallContext, i < 3);
        assert.equal(ctx.nonpositiveCallPolls, Math.min(i, 3));
      }
      assert.deepEqual(writes, ['call on\n', 'call off\n']);
      ctx.applyCallContext('active');
      assert.equal(ctx.nonpositiveCallPolls, 0);
      ctx.applyCallContext('unknown');
      ctx.applyCallContext('inactive');
      assert.equal(ctx.detectedCallContext, true);
      ctx.applyCallContext('active');
      assert.equal(ctx.nonpositiveCallPolls, 0);
    }
  },
  'two inactive polls disable automatic context': () => {
    const { ctx } = fixture();
    ctx.applyCallContext('active');
    ctx.applyCallContext('inactive');
    assert.equal(ctx.detectedCallContext, true);
    ctx.applyCallContext('inactive');
    assert.equal(ctx.detectedCallContext, false);
  },
  'poll result applied once, failures reject valid-looking output': () => {
    const { ctx, startPoll, endPoll } = fixture();
    startPoll();
    endPoll('active\n');
    assert.equal(ctx.detectedCallContext, true);
    for (let i = 1; i <= 3; i++) {
      startPoll();
      const [code, status] = [[1, 0], [137, 0], [9, 1]][i - 1];
      endPoll('active\n', code, status);
      assert.equal(ctx.unknownCallPolls, i);
      assert.equal(ctx.detectedCallContext, i < 3);
    }
    startPoll();
    endPoll('inactive\n');
    assert.equal(ctx.inactiveCallPolls, 1);
  },
  'watchdog sends ALRM to KILL supervisor; no overlap or double counting': () => {
    const { ctx, signals, invoke, startPoll, endPoll } = fixture();
    ctx.applyCallContext('active');
    for (let i = 1; i <= 3; i++) {
      startPoll();
      invoke('callContextTimeout', 'onTriggered');
      assert.equal(ctx.unknownCallPolls, i);
      invoke('callContextPoll', 'onTriggered');
      assert.equal(ctx.callContextProc.pending, false);
      endPoll('active\n');
      assert.equal(ctx.unknownCallPolls, i);
    }
    assert.deepEqual(signals, [14, 14, 14]);
    assert.equal(ctx.detectedCallContext, false);
    assert.match(processBlock('callContextTimeout'), /interval: 4000/);
    const command = JSON.parse(processBlock('callContextProc').match(/^    command: (.+)$/m)[1]);
    assert.deepEqual(command.slice(0, 5), ['timeout', '--signal=KILL', '2s', 'bash', '-c']);
    assert.equal(command.length, 6);
    assert.match(command[5], /^set -o pipefail; pactl -f json list source-outputs \| jq -r '/);
  },
  'launch failure counts once; poll cannot overlap running process': () => {
    const { ctx, signals, invoke, startPoll } = fixture();
    startPoll();
    ctx.callContextProc.running = false;
    ctx.callContextProc.processId = null;
    invoke('callContextProc', 'onRunningChanged');
    assert.equal(ctx.unknownCallPolls, 1);
    invoke('callContextTimeout', 'onTriggered');
    assert.equal(ctx.unknownCallPolls, 1);
    assert.deepEqual(signals, []);
    ctx.callContextProc.running = true;
    invoke('callContextPoll', 'onTriggered');
    assert.equal(ctx.callContextProc.pending, false);
  },
};

let failed = 0;
for (const [name, run] of Object.entries(cases)) {
  try { run(); console.log(`PASS ${name}`); }
  catch (error) { failed++; console.error(`FAIL ${name}: ${error.message}`); }
}
console.log(`Call-context: ${Object.keys(cases).length - failed}/${Object.keys(cases).length} passed`);
process.exitCode = failed ? 1 : 0;
