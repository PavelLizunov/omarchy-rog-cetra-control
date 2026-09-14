// Offline fixtures execute production bindings/functions, not a QML lifecycle.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
require('./keyboard.js');

const dir = path.resolve(__dirname, '../..');
const { service: source, widget: widgetSource } = require('../qml-source.js');
const manifest = JSON.parse(fs.readFileSync(path.join(dir, 'manifest.json'), 'utf8'));
const block = id => source.match(new RegExp(`^  (?:Process|Timer) \\{\\n    id: ${id}\\b[\\s\\S]*?^  \\}`, 'm'))[0];
const timer = (changed = () => {}) => ({
  running: false, restarts: 0,
  start() { if (!this.running) { this.running = true; changed(); } },
  restart() { this.restarts++; if (!this.running) { this.running = true; changed(); } },
  stop() { if (this.running) { this.running = false; changed(); } },
});

function bind(ctx, owner, name) {
  const expr = owner.match(new RegExp(`^  readonly property \\w+ ${name}: (.+)$`, 'm'))[1];
  const body = expr === '{'
    ? owner.match(new RegExp(`^  readonly property \\w+ ${name}: \\{\\n([\\s\\S]*?)^  \\}`, 'm'))[1]
    : `return (${expr})`;
  Object.defineProperty(ctx, name, {
    get: () => vm.runInContext(`(function() {${body}})()`, ctx),
    set: () => { throw new Error(`readonly binding overwritten: ${name}`); },
  });
}

function functions(ctx, owner) {
  for (const match of owner.matchAll(/^  function \w+\([^)]*\) \{[\s\S]*?^  \}/gm))
    vm.runInContext(match[0], ctx);
}

function fixture() {
  const writes = [];
  const signals = [];
  let now = 100000;
  const ctx = vm.createContext({
    Date: { now: () => now },
    shell: null, manifest: null, pluginRegistry: null,
    deviceWatchProc: { running: false, write(value) { writes.push(value); } },
    callContextProc: { running: false, pending: false, processId: 0, signal(value) { signals.push(value); } },
    deviceWatchRestart: timer(), modeRequestTimeout: timer(), callContextTimeout: timer(), settingsRequestTimeout: timer(), preferenceReadback: timer(), themeColorDelay: timer(),
  });
  ctx.root = ctx;
  for (const match of source.matchAll(/^  property (?:string|bool|int|var) (\w+): (.+)$/gm))
    ctx[match[1]] = vm.runInContext(`(${match[2]})`, ctx);
  for (const name of ['hostReady', 'hostSettings', 'settings', 'settingsStatusKey']) bind(ctx, source, name);
  functions(ctx, source);
  for (const match of block('deviceWatchProc').matchAll(/^    property bool (\w+): (.+)$/gm))
    ctx.deviceWatchProc[match[1]] = vm.runInContext(match[2], ctx);
  let watcherRunning = false;
  Object.defineProperty(ctx.deviceWatchProc, 'running', {
    get: () => watcherRunning,
    set: () => { throw new Error('watcher running binding must not be overwritten'); },
  });
  // Native process transitions do not remove the QML running binding.
  function watcherRunningChanged(value) {
    if (watcherRunning === value) return;
    watcherRunning = value;
    invoke('deviceWatchProc', 'onRunningChanged');
  }
  function syncWatcherBinding() {
    watcherRunningChanged(vm.runInContext(block('deviceWatchProc').match(/^    running: (.+)$/m)[1], ctx));
  }
  ctx.deviceWatchRestart = timer(syncWatcherBinding);
  const services = {};
  const host = {
    barConfig: { layout: { left: [], center: [], right: [{ id: manifest.id, alwaysCallContext: true }] } },
    serviceFor(id) { return id === manifest.id ? services[id] || null : null; },
    updateEntryInline(id, entry) {
      if (id !== manifest.id) return false;
      const config = JSON.parse(JSON.stringify(this.barConfig));
      let dirty = false;
      for (const section of ['left', 'center', 'right'])
        config.layout[section] = (config.layout[section] || []).map(current => {
          if (current?.id !== id) return current;
          const next = { ...entry, id };
          if (JSON.stringify(current) !== JSON.stringify(next)) dirty = true;
          return next;
        });
      if (!dirty) return false;
      this.barConfig = config;
      return true;
    },
  };
  const registry = { pluginId: manifest.id, manifest, enabled: true };
  function invoke(id, event) {
    const text = block(id);
    if (event === 'onTriggered' && ctx[id]?.running && !/repeat: true/.test(text)) ctx[id].stop();
    const body = text.match(new RegExp(`^    ${event}: \\{\\n([\\s\\S]*?)^    \\}`, 'm'))?.[1]
      || text.match(new RegExp(`^    ${event}: (.+)$`, 'm'))?.[1];
    if (body === undefined) return;
    vm.runInContext(`(function() {${body}})()`, ctx);
  }
  function ready(started = true) {
    ctx.detectedCallContext = true; // Fixture has an observed capture, not a saved override.
    ctx.shell = host;
    ctx.manifest = manifest;
    ctx.pluginRegistry = registry;
    syncWatcherBinding();
    if (started) invoke('deviceWatchProc', 'onStarted');
    services[manifest.id] = ctx;
  }
  function widget() {
    const view = vm.createContext({ bar: { shell: host }, moduleName: manifest.id, settings: {}, accent: { r: 0.2, g: 0.4, b: 0.6 } });
    view.root = view;
    view.i18n = { text: (key, fallback, params) => fallback.replace(/\{(\w+)\}/g, (match, name) => params?.[name] ?? match) };
    view.setting = (name, fallback) => view.settings[name] ?? fallback;
    for (const name of ['service', 'deviceStatus', 'receiver', 'connected', 'leftLevel', 'rightLevel', 'caseLevel',
      'listeningMode', 'pendingMode', 'ancLevel', 'ancAdaptive', 'voicePrompt', 'lighting', 'callContextActive',
      'pendingSettings', 'settingsRequestTimedOut', 'settingsStatusKey',
      'useThemeColor', 'lightingRgb', 'selectedLightingColor', 'colorApplyEffect'])
      bind(view, widgetSource, name);
    functions(view, widgetSource);
    return view;
  }
  const settingChanged = () => {
    ctx.syncSettings(ctx.hostSettings);
  };
  return { ctx, host, registry, services, writes, signals, invoke, ready, widget, settingChanged,
    watcherRunningChanged, syncWatcherBinding, jumpClock: ms => { now += ms; },
    tick: (count = 1) => {
      for (let i = 0; i < count; i++)
        if (ctx.settingsRequestTimeout.running) invoke('settingsRequestTimeout', 'onTriggered');
    } };
}

const cases = {
  'battery and mode freshness veto historical values and invalid domains': () => {
    const {ctx,ready}=fixture(); ready(false);
    const data={status:'ok',receiver:true,connected:true,left:50,right:60,case:70,mode:'anc',battery_fresh:true,mode_fresh:true};
    ctx.applyDeviceState(JSON.stringify(data)); assert.equal(ctx.leftLevel,50); assert.equal(ctx.listeningMode,'anc');
    ctx.applyDeviceState(JSON.stringify({...data,battery_fresh:false,mode_fresh:false}));
    assert.equal(ctx.connected,false); assert.equal(ctx.leftLevel,null); assert.equal(ctx.listeningMode,'unknown');
    for(const value of [-1,101,0.5,'50',null]) {
      ctx.applyDeviceState(JSON.stringify({...data,left:value})); assert.equal(ctx.leftLevel,null);
    }
    ctx.applyDeviceState(JSON.stringify({...data,left:0})); assert.equal(ctx.leftLevel,0);
  },
  'ANC readback at eight seconds confirms and a late matching reply clears timeout': () => {
    const { ctx, ready, tick, writes } = fixture();
    ready();
    const report = fields => ctx.applyDeviceState(JSON.stringify({status:'ok', receiver:true, connected:true, mode:'anc', ...fields}));
    report({anc_level:1, anc_adaptive:false});
    ctx.setAncLevel(3);
    tick(32);
    assert.equal(ctx.settingsRequestTimedOut, false);
    assert.equal(ctx.pendingSettings.anc_level.desired, 3);
    report({anc_level:3, anc_adaptive:false});
    assert.deepEqual(Object.keys(ctx.pendingSettings), []);
    ctx.setAncLevel(2);
    ctx.setAncAdaptive(true);
    tick(48);
    assert.equal(ctx.settingsRequestTimedOut, true);
    report({anc_level:2, anc_adaptive:false});
    assert.equal(ctx.settingsRequestTimedOut, true, 'Another setting remains unconfirmed');
    report({anc_level:2, anc_adaptive:true});
    assert.equal(ctx.settingsRequestTimedOut, false);
    assert.deepEqual(Object.keys(ctx.timedOutSettings), []);
    assert.deepEqual(writes, ['call on\n','anc_level 3\n','anc_level 2\n','anc_adaptive on\n']);
  },
  'saved preferences reject stale snapshots, accept external edits and bound unconfirmed writes': () => {
    const { ctx, ready, widget, invoke } = fixture();
    ready();
    const a = widget(), b = widget();
    const config = entry => JSON.stringify({ version: 1, bar: { layout: { right: [entry] } } });
    const initial = { id: manifest.id, locale: 'en', alwaysCallContext: false };
    ctx.applySavedConfig(config(initial));
    a.setLocaleSetting('ru');
    assert.equal(b.preference('locale', 'system'), 'ru');
    ctx.applySavedConfig(config(initial));
    assert.equal(b.preference('locale', 'system'), 'ru');
    ctx.applySavedConfig(config({ ...initial, locale: 'ru' }));
    assert.deepEqual(Object.keys(ctx.pendingPreferences), []);
    assert.equal(ctx.preferenceReadback.running, false);
    for (const invalid of ['{', 'null', '[]', '{"version":2}', '{"version":1}', '{"version":1,"bar":{"layout":{"right":false}}}'])
      ctx.applySavedConfig(invalid);
    assert.equal(b.preference('locale', 'system'), 'ru');
    ctx.applySavedConfig(config({ id: 'foreign', locale: 'de' }));
    assert.equal(b.preference('locale', 'system'), 'system', 'Valid removal clears stale preferences');
    ctx.applySavedConfig(config({ ...initial, locale: 'de' }));
    assert.equal(a.preference('locale', 'system'), 'de');
    a.setLocaleSetting('fr');
    assert.equal(ctx.preferenceReadback.running, true);
    let reloaded = false;
    ctx.queueSettingsRead = () => { reloaded = true; ctx.applySavedConfig(config(initial)); };
    invoke('preferenceReadback', 'onTriggered');
    assert.equal(reloaded, true);
    assert.equal(b.preference('locale', 'system'), 'en');
    assert.deepEqual(Object.keys(ctx.pendingPreferences), []);
    assert.doesNotMatch(widgetSource, /onSettingsChanged|service\.syncSettings/);
    assert.match(source, /onFileChanged: root.queueSettingsRead\(\)/);
    assert.match(source, /preload: false/);
    assert.match(source, /root.hostReady && generation === root.settingsReadGeneration && exitCode === 0/);
  },
  'manual ANC highlights require both a known level and confirmed adaptive off': () => {
    const { ctx, ready, widget } = fixture();
    ready();
    const view = widget();
    const active = widgetSource.match(/^\s*active: (root\.ancLevel === .+)$/m)[1];
    for (const level of [null, 1, 2, 3]) for (const adaptive of [null, true, false]) {
      ctx.applyDeviceState(JSON.stringify({ status: 'ok', receiver: true, connected: true,
        mode: 'anc', anc_level: level, anc_adaptive: adaptive }));
      const selected = [1, 2, 3].filter(value => {
        view.modelData = { value };
        return vm.runInContext(active, view);
      });
      assert.deepEqual(selected, level !== null && adaptive === false ? [level] : []);
    }
  },
  'manual ANC requests explicitly disable unknown adaptive and share pending guards': () => {
    for (const adaptive of [null, true, false]) {
      const { ctx, ready, widget, writes } = fixture();
      ready();
      const a = widget(), b = widget();
      a.opened = b.opened = true;
      ctx.applyDeviceState(JSON.stringify({ status: 'ok', receiver: true, connected: true,
        mode: 'anc', anc_level: 1, anc_adaptive: adaptive }));
      a.chooseAncLevel(2);
      assert.deepEqual(writes, ['call on\n', ...(adaptive !== false ? ['anc_adaptive off\n'] : []), 'anc_level 2\n']);
      assert.equal(a.ancAdaptive, adaptive);
      assert.equal(a.ancLevel, 1);
      b.handleTextKey('3');
      assert.equal(writes.at(-1), 'anc_level 2\n');
      const count = writes.length;
      ctx.applyDeviceState('{"status":"ok","receiver":true,"connected":true,"mode":"anc","anc_level":2,"anc_adaptive":null}');
      if (adaptive !== false) {
        assert.deepEqual(Object.keys(ctx.pendingSettings), ['anc_adaptive']);
        b.chooseAncLevel(3);
        b.handleTextKey('1');
        assert.equal(writes.length, count, 'Neither route bypasses pending Adaptive');
      }
      ctx.applyDeviceState('{"status":"ok","receiver":true,"connected":true,"mode":"anc","anc_level":2,"anc_adaptive":false}');
      b.handleTextKey('3');
      assert.equal(writes.at(-1), 'anc_level 3\n');
      assert.equal(writes.length, count + 1);
    }
  },
  'settings start unknown, strictly validate each snapshot and clear stale readbacks': () => {
    const { ctx, ready, widget } = fixture();
    const a = widget();
    const values = view => [view.ancLevel, view.ancAdaptive, view.voicePrompt, ctx.proximity];
    const unknown = [null, null, 'unknown', null];
    assert.deepEqual(values(a), unknown);
    ready();
    const report = fields => ctx.applyDeviceState(JSON.stringify({ status: 'ok', receiver: true, connected: true, ...fields }));
    for (const level of [1, 2, 3]) for (const flag of [true, false]) for (const voice of ['english', 'chinese', 'sound']) {
      report({ anc_level: level, anc_adaptive: flag, voice_prompt: voice, proximity: flag });
      assert.deepEqual(values(a), [level, flag, voice, flag]);
      report({});
      assert.deepEqual(values(a), unknown);
    }
    for (const invalid of [null, '', 'true', 'false', '1', 0, -1, 4, 1.5, {}, [], 'en']) {
      report({ anc_level: invalid, anc_adaptive: invalid, voice_prompt: invalid, proximity: invalid });
      assert.deepEqual(values(a), unknown);
    }
    for (const reset of [{ receiver: false }, { connected: false }, { status: 'busy' },
      { status: 'permission-denied' }, { presence_raw: 0, left_present: null, right_present: null }]) {
      report({ anc_level: 3, anc_adaptive: true, voice_prompt: 'english', proximity: true });
      report({ anc_level: 3, anc_adaptive: true, voice_prompt: 'english', proximity: true, ...reset });
      assert.deepEqual(values(a), unknown);
    }
    for (const malformed of ['null', '[]', 'false', '0', '{']) ctx.applyStatus(malformed);
  },
  'setting arguments, stopped owner and offline requests never write or queue': () => {
    const { ctx, ready, writes, watcherRunningChanged } = fixture();
    ready();
    const settings = [
      ['setAncLevel', 2, [0, 4, 1.5, '2', true, NaN, Infinity]],
      ['setAncAdaptive', false, [0, 1, 'false', 'on']],
      ['setVoicePrompt', 'english', ['en', 'unknown', 'EN', 'english\ncall on', 1]],
      ['setProximity', true, [0, 1, 'true', 'off']],
    ];
    for (const [setter, valid, invalid] of settings) {
      assert.equal(ctx[setter](valid), false);
      ctx.connected = true;
      for (const value of [...invalid, null, undefined, {}, []]) assert.equal(ctx[setter](value), false);
      ctx.connected = false;
    }
    watcherRunningChanged(false);
    ctx.connected = true;
    for (const [setter, valid] of settings) assert.equal(ctx[setter](valid), false);
    assert.deepEqual(Object.keys(ctx.pendingSettings), []);
    assert.deepEqual(writes, ['call on\n']);
    assert.equal(ctx.settingsRequestTimeout.running, false);
  },
  'two views share per-setting requests; only matching valid readbacks confirm': () => {
    const { ctx, ready, widget, writes } = fixture();
    ready();
    const a = widget(), b = widget();
    const report = fields => ctx.applyDeviceState(JSON.stringify({ status: 'ok', receiver: true, connected: true, ...fields }));
    report({ anc_level: 1, anc_adaptive: true, voice_prompt: 'sound', proximity: false });
    a.setAncLevel(3);
    b.setAncAdaptive(false);
    a.setVoicePrompt('english');
    ctx.setProximity(true);
    assert.deepEqual(writes, ['call on\n', 'anc_level 3\n', 'anc_adaptive off\n', 'voice_prompt english\n', 'proximity on\n']);
    assert.deepEqual([a.ancLevel, b.ancAdaptive, a.voicePrompt, ctx.proximity], [1, true, 'sound', false]);
    assert.equal(a.pendingSettings, b.pendingSettings);
    assert.equal(a.settingsStatusKey, 'settings.pending');
    const requests = JSON.stringify(a.pendingSettings);
    a.setAncLevel(2);
    b.setAncAdaptive(true);
    a.setVoicePrompt('chinese');
    ctx.setProximity(false);
    assert.equal(writes.length, 5);
    for (const fields of [{}, { anc_level: '3', anc_adaptive: 0, voice_prompt: 'en', proximity: 1 },
      { anc_level: 2, anc_adaptive: true, voice_prompt: 'chinese', proximity: false }]) {
      report(fields);
      assert.equal(JSON.stringify(b.pendingSettings), requests);
    }
    report({ anc_level: 3, anc_adaptive: false });
    assert.deepEqual(Object.keys(a.pendingSettings), ['voice_prompt', 'proximity']);
    assert.equal(ctx.settingsRequestTimeout.running, true);
    report({ voice_prompt: 'english', proximity: true });
    assert.deepEqual(Object.keys(a.pendingSettings), []);
    assert.equal(ctx.settingsRequestTimeout.running, false);
    assert.equal(b.settingsStatusKey, '');
  },
  'requests allow a full readback cycle and expire independently of wall-clock corrections': () => {
    const { ctx, ready, widget, tick, jumpClock, writes, watcherRunningChanged } = fixture();
    ready();
    ctx.connected = true;
    const a = widget(), b = widget();
    a.setAncLevel(2);
    assert.equal(a.pendingSettings.anc_level.remainingTicks, 48);
    const initial = JSON.stringify(a.pendingSettings);
    for (const delta of [86400000, -172800000, 86400000]) {
      jumpClock(delta);
      assert.equal(JSON.stringify(a.pendingSettings), initial, 'A wall-clock jump without a tick changes nothing');
    }
    tick(44);
    assert.equal(a.pendingSettings.anc_level.remainingTicks, 4);
    ctx.setProximity(true);
    assert.equal(a.pendingSettings.anc_level.remainingTicks, 4, 'New requests do not renew old ones');
    assert.equal(a.pendingSettings.proximity.remainingTicks, 48);
    for (const delta of [86400000, -172800000, 86400000]) {
      jumpClock(delta);
      tick();
    }
    assert.equal(a.pendingSettings.anc_level.remainingTicks, 1);
    assert.equal(a.settingsRequestTimedOut, false);
    tick();
    assert.equal(b.settingsRequestTimedOut, true);
    assert.equal(a.pendingSettings.anc_level, undefined);
    assert.equal(a.pendingSettings.proximity.remainingTicks, 44);
    assert.equal(b.settingsStatusKey, 'settings.notConfirmed');
    assert.equal(a.ancLevel, null);
    assert.equal(ctx.settingsRequestTimeout.running, true);
    tick(43);
    assert.equal(a.pendingSettings.proximity.remainingTicks, 1);
    tick();
    assert.equal(ctx.settingsRequestTimeout.running, false);
    assert.deepEqual(Object.keys(b.pendingSettings), []);
    assert.equal(writes.length, 3, 'Expiry never retries or queues a write');
    a.setVoicePrompt('sound');
    assert.equal(b.settingsRequestTimedOut, false);
    watcherRunningChanged(false);
    assert.equal(b.settingsRequestTimedOut, false);
    assert.deepEqual(Object.keys(a.pendingSettings), []);
    assert.equal(ctx.settingsRequestTimeout.running, false);
    assert.match(block('settingsRequestTimeout'), /interval: 250\s+repeat: true/);
    assert.doesNotMatch(source, /Date\.now|deadline:/);
  },
  'matching confirmation before the final tick cancels expiry without false timeout': () => {
    const { ctx, ready, tick, writes } = fixture();
    ready();
    ctx.connected = true;
    ctx.setAncAdaptive(false);
    tick(47);
    assert.equal(ctx.pendingSettings.anc_adaptive.remainingTicks, 1);
    for (const value of [null, true, 0]) {
      ctx.applyDeviceState(JSON.stringify({ status: 'ok', receiver: true, connected: true, anc_adaptive: value }));
      assert.equal(ctx.pendingSettings.anc_adaptive.remainingTicks, 1);
    }
    ctx.applyDeviceState('{"status":"ok","receiver":true,"connected":true,"anc_adaptive":false}');
    assert.deepEqual(Object.keys(ctx.pendingSettings), []);
    assert.equal(ctx.settingsRequestTimeout.running, false);
    tick(48);
    assert.equal(ctx.settingsRequestTimedOut, false);
    assert.deepEqual(writes, ['call on\n', 'anc_adaptive off\n']);
  },
  'repeated FailedToStart need no successful runningChanged notification': () => {
    const { ctx, host, registry, invoke } = fixture();
    ctx.shell = host;
    ctx.manifest = manifest;
    ctx.pluginRegistry = registry;
    // QProcess failures notify false without a preceding successful-start signal.
    ctx.deviceWatchRestart = timer();
    for (let attempt = 1; attempt <= 3; attempt++) {
      invoke('deviceWatchProc', 'onRunningChanged');
      assert.equal(ctx.deviceWatchRestart.running, true);
      assert.equal(ctx.deviceWatchRestart.restarts, attempt);
      invoke('deviceWatchRestart', 'onTriggered');
      assert.equal(ctx.deviceWatchProc.stopHandled, false);
    }
  },
  'panel height uses content and screen limit without a fixed 560 cap': () => {
    assert.match(widgetSource, /contentHeight: panel\.fittedContentHeight\(column\.implicitHeight\)/);
    assert.doesNotMatch(widgetSource, /fittedContentHeight\(column\.implicitHeight\s*,/);
    assert.match(widgetSource, /flickableDirection: Flickable\.VerticalFlick/);
    assert.match(widgetSource, /contentHeight: column\.implicitHeight/);
  },
  'manifest and source enforce service-only ownership': () => {
    assert.deepEqual(manifest.kinds, ['bar-widget', 'service']);
    assert.deepEqual(manifest.entryPoints, { barWidget: 'Cetra.qml', service: 'CetraService.qml' });
    assert.match(widgetSource, new RegExp(`moduleName: "${manifest.id.replaceAll('.', '\\.')}"`));
    assert.match(source, /^Item \{/m);
    assert.doesNotMatch(source, /\bbar\b\s*:/);
    assert.match(source, /readonly property color themeColor: Color\.accent/);
    assert.doesNotMatch(widgetSource, /\b(?:Process|Timer)\s*\{|Quickshell\.Io|deviceWatchProc|callContextProc|onAlwaysCallContextChanged|service\.\w+\s*=(?!=)/);
    assert.equal((source.match(/^  Process \{/gm) || []).length, 2);
    assert.doesNotMatch(source, /findEntryLocation|shellConfig/);
    assert.match(source, /shell\.barConfig\.layout/);
    assert.match(widgetSource, /readonly property var service: bar\?\.shell\?\.serviceFor\(root\.moduleName\) \|\| null/);
  },
  'injection gates watcher, polling, restart and call writes': () => {
    const { ctx, host, registry, writes, invoke, syncWatcherBinding } = fixture();
    const assertStopped = () => {
      assert.equal(ctx.hostReady, false);
      syncWatcherBinding();
      for (const id of ['deviceWatchProc'])
        assert.equal(vm.runInContext(block(id).match(/^    running: (.+)$/m)[1], ctx), false);
      invoke('deviceWatchRestart', 'onTriggered');
      assert.equal(ctx.deviceWatchProc.running, false);
      ctx.updateCallContext(true);
      assert.deepEqual(writes, []);
    };
    assertStopped();
    ctx.shell = host;
    assertStopped();
    ctx.manifest = manifest;
    assertStopped();
    ctx.pluginRegistry = registry;
    syncWatcherBinding();
    assert.equal(ctx.hostReady, true);
    ctx.detectedCallContext = true;
    invoke('deviceWatchRestart', 'onTriggered');
    invoke('deviceWatchProc', 'onStarted');
    assert.deepEqual(writes, ['call on\n']);
    assert.match(source, /AudioTopology \{ id: audioTopology; active: root.hostReady && root.receiver \}/);
  },
  'FailedToStart without exited retries on each timer expiry and recovers': () => {
    const { ctx, writes, ready, invoke, watcherRunningChanged } = fixture();
    ready(false);
    assert.equal(ctx.deviceWatchProc.running, true);
    for (let attempt = 1; attempt <= 3; attempt++) {
      watcherRunningChanged(false);
      assert.equal(ctx.deviceStatus, 'helper-error');
      assert.equal(ctx.connected, false);
      assert.equal(ctx.deviceWatchRestart.running, true);
      assert.equal(ctx.deviceWatchRestart.restarts, attempt);
      assert.deepEqual(writes, []);
      invoke('deviceWatchRestart', 'onTriggered');
      assert.equal(ctx.deviceWatchProc.running, true);
    }
    invoke('deviceWatchProc', 'onStarted');
    assert.equal(ctx.deviceWatchRestart.running, false);
    assert.deepEqual(writes, ['call on\n']);
    ctx.applyDeviceState('{"status":"ok","receiver":true,"connected":true,"mode":"off"}');
    assert.equal(ctx.deviceStatus, 'ok');
    assert.equal(ctx.connected, true);
    assert.match(block('deviceWatchRestart'), /interval: 2000/);
  },
  'watcher stop events are idempotent in either order and clear stale state': () => {
    for (const exitFirst of [true, false]) {
      const { ctx, ready, invoke, watcherRunningChanged } = fixture();
      ready();
      ctx.applyDeviceState('{"status":"ok","receiver":true,"connected":true,"left":81,"mode":"off","call_context":true}');
      ctx.setListeningMode('anc');
      if (exitFirst) invoke('deviceWatchProc', 'onExited');
      watcherRunningChanged(false);
      invoke('deviceWatchProc', 'onExited');
      invoke('deviceWatchProc', 'onRunningChanged');
      assert.equal(ctx.deviceWatchRestart.restarts, 1);
      assert.equal(ctx.deviceStatus, 'helper-error');
      assert.equal(ctx.receiver, false);
      assert.equal(ctx.connected, false);
      assert.equal(ctx.leftLevel, null);
      assert.equal(ctx.pendingMode, '');
      assert.equal(ctx.modeRequestTimeout.running, false);
      assert.equal(ctx.modeRequestTimedOut, false);
      assert.equal(ctx.callContextActive, false);
      assert.equal(ctx.requestedCallContextActive, false);
      invoke('deviceWatchRestart', 'onTriggered');
      invoke('deviceWatchProc', 'onStarted');
      watcherRunningChanged(false);
      assert.equal(ctx.deviceWatchRestart.restarts, 2);
    }
  },
  'host loss prevents restart after watcher failure': () => {
    const { ctx, ready, invoke, syncWatcherBinding } = fixture();
    ready();
    ctx.shell = null;
    syncWatcherBinding();
    invoke('deviceWatchProc', 'onExited');
    assert.equal(ctx.deviceWatchRestart.running, false);
    assert.equal(ctx.deviceWatchRestart.restarts, 0);
    invoke('deviceWatchRestart', 'onTriggered');
    assert.equal(ctx.deviceWatchProc.running, false);
  },
  'mode requests validate enum and require both connection and running watcher': () => {
    const { ctx, writes, ready, watcherRunningChanged } = fixture();
    ready();
    ctx.setListeningMode('anc');
    assert.equal(ctx.pendingMode, '');
    ctx.connected = true;
    for (const mode of ['unknown', '', 'ANC', 'anc\ncall on', null, undefined, 1, {}])
      ctx.setListeningMode(mode);
    assert.equal(ctx.pendingMode, '');
    assert.equal(ctx.modeRequestTimeout.restarts, 0);
    assert.deepEqual(writes, ['call on\n']);
    watcherRunningChanged(false);
    ctx.connected = true;
    ctx.setListeningMode('anc');
    assert.equal(ctx.pendingMode, '');
    assert.deepEqual(writes, ['call on\n']);
  },
  'mode timeout reports only unconfirmed requests and clears on retry, match or reset': () => {
    const { ctx, ready, invoke } = fixture();
    ready();
    const report = mode => ctx.applyDeviceState(JSON.stringify({ status: 'ok', receiver: true, connected: true, mode }));
    report('off');
    invoke('modeRequestTimeout', 'onTriggered');
    assert.equal(ctx.modeRequestTimedOut, false);
    for (const mode of ['anc', 'ambient', 'off']) {
      const confirmed = ctx.listeningMode;
      ctx.setListeningMode(mode);
      assert.equal(ctx.modeRequestTimedOut, false);
      assert.equal(ctx.pendingMode, mode);
      assert.equal(ctx.listeningMode, confirmed);
      invoke('modeRequestTimeout', 'onTriggered');
      assert.equal(ctx.modeRequestTimedOut, true);
      assert.equal(ctx.pendingMode, '');
      assert.equal(ctx.listeningMode, confirmed);
      ctx.setListeningMode(mode);
      assert.equal(ctx.modeRequestTimedOut, false);
      report(mode);
      assert.equal(ctx.pendingMode, '');
      assert.equal(ctx.modeRequestTimeout.running, false);
      assert.equal(ctx.modeRequestTimedOut, false);
      invoke('modeRequestTimeout', 'onTriggered');
      assert.equal(ctx.modeRequestTimedOut, false);
    }
    ctx.setListeningMode('anc');
    report('off');
    assert.equal(ctx.pendingMode, 'anc');
    invoke('modeRequestTimeout', 'onTriggered');
    assert.equal(ctx.modeRequestTimedOut, true);
    ctx.clearDeviceState();
    assert.equal(ctx.modeRequestTimedOut, false);
    report('off');
    ctx.setListeningMode('anc');
    ctx.applyDeviceState('{"status":"ok","receiver":true,"presence_raw":0,"left_present":false,"right_present":false}');
    assert.equal(ctx.pendingMode, '');
    assert.equal(ctx.modeRequestTimeout.running, false);
    invoke('modeRequestTimeout', 'onTriggered');
    assert.equal(ctx.modeRequestTimedOut, false);
  },
  'two views share telemetry, pending, confirmation and timeout': () => {
    const { ctx, writes, invoke, ready, widget } = fixture();
    const a = widget();
    assert.equal(a.service, null);
    a.setListeningMode('anc');
    ready();
    const b = widget();
    assert.equal(a.service, b.service);
    ctx.applyDeviceState(JSON.stringify({ status: 'ok', receiver: true, connected: true, left: 81, right: 72, mode: 'off' }));
    assert.equal(a.leftLevel, 81);
    assert.equal(b.rightLevel, 72);
    a.setListeningMode('anc');
    assert.equal(b.pendingMode, 'anc');
    b.setListeningMode('ambient');
    assert.deepEqual(writes, ['call on\n', 'mode anc\n']);
    ctx.applyDeviceState('{"status":"ok","receiver":true,"connected":true,"mode":"anc"}');
    assert.equal(a.pendingMode, '');
    assert.equal(b.listeningMode, 'anc');
    assert.equal(ctx.modeRequestTimeout.running, false);
    b.cycleListeningMode();
    assert.equal(a.pendingMode, 'ambient');
    invoke('modeRequestTimeout', 'onTriggered');
    assert.equal(b.pendingMode, '');
    a.setAncLevel(2);
    b.setAncAdaptive(false);
    a.setVoicePrompt('sound');
    ctx.setProximity(true);
    a.setLighting('static');
    assert.deepEqual([b.ancLevel, a.ancAdaptive, b.voicePrompt, ctx.proximity, b.lighting], [null, null, 'unknown', null, 'unknown']);
    assert.equal(writes.at(-1), 'lighting static 51 102 153\n');
    ctx.applyDeviceState('{"status":"ok","receiver":true,"connected":true,"lighting":"static"}');
    assert.equal(b.lighting, 'static');
    for (const text of ['garbage', 'null']) ctx.applyDeviceState(text);
    assert.equal(a.connected, true);
    ctx.applyDeviceState('{"status":"busy"}');
    assert.equal(a.connected, false);
    assert.equal(a.pendingMode, '');
    assert.equal(a.leftLevel, null);
  },
  'canonical settings survive view loss, reorder and replacement without call off': () => {
    const { ctx, host, writes, ready, widget, settingChanged } = fixture();
    ready();
    const a = widget();
    ctx.connected = true;
    a.setListeningMode('anc');
    a.settings = {};
    a.bar = null;
    assert.equal(a.service, null);
    assert.equal(ctx.detectedCallContext, true);
    const config = JSON.parse(JSON.stringify(host.barConfig));
    config.layout.left = [{ id: 'neighbor' }, config.layout.right.pop()];
    host.barConfig = config;
    settingChanged();
    const b = widget();
    assert.equal(b.service, ctx);
    assert.equal(b.pendingMode, 'anc');
    assert.deepEqual(writes, ['call on\n', 'mode anc\n']);
    b.settings = { alwaysCallContext: true, showPercentage: false };
    b.setLightingSetting('useThemeColor', false);
    settingChanged();
    assert.equal(ctx.settings.useThemeColor, false);
    assert.equal(host.barConfig.layout.left[1].showPercentage, false);
    assert.equal(writes.at(-1), 'mode anc\n');
    host.barConfig = { layout: { right: [{ id: manifest.id, alwaysCallContext: true }] } };
    settingChanged();
    assert.equal(writes.at(-1), 'mode anc\n');
    host.barConfig = { layout: { left: [manifest.id] } };
    settingChanged();
    assert.equal(ctx.detectedCallContext, true);
    host.barConfig = {};
    settingChanged();
    assert.equal(ctx.detectedCallContext, true);
  },
  'presence overrides stale batteries and expiry does not re-enable controls': () => {
    const { ctx, writes, ready } = fixture();
    ready();
    const state = { status: 'ok', receiver: true, connected: true, left: 91, right: 98 };
    ctx.applyDeviceState(JSON.stringify(state));
    assert.equal(ctx.connected, true);
    ctx.applyDeviceState(JSON.stringify({ ...state, presence_raw: 0, left_present: false, right_present: false,
      left_charging: true, right_charging: true, case_charging: false }));
    assert.equal(ctx.connected, false);
    assert.equal(ctx.leftLevel, 91);
    assert.equal(ctx.leftCharging, true);
    const count = writes.length;
    ctx.setListeningMode('anc');
    assert.equal(writes.length, count);
    ctx.applyDeviceState(JSON.stringify({ ...state, presence_raw: 0, left_present: null, right_present: null }));
    assert.equal(ctx.connected, false);
    assert.equal(ctx.leftCharging, null);
    ctx.applyDeviceState(JSON.stringify({ ...state, presence_raw: 16, left_present: false, right_present: true }));
    assert.equal(ctx.connected, true);
    ctx.applyDeviceState(JSON.stringify({ ...state, presence_raw: 255, left_present: 'true', right_present: 1 }));
    assert.equal(ctx.connected, false);
    ctx.applyDeviceState(JSON.stringify({ ...state, receiver: false, left_present: true }));
    assert.equal(ctx.connected, false);
  },
  'report labels never call absence in-case or infer mute': () => {
    const { widget } = fixture();
    const view = widget();
    assert.equal(view.reportText(false, true, false), 'Unavailable\nCharging reported');
    assert.equal(view.reportText(true, false, false), 'Present\nNot charging');
    assert.equal(view.reportText(null, null, false), 'Presence unknown\nCharging state unknown');
    assert.equal(view.reportText(null, false, true), 'Not charging');
    assert.match(widgetSource, /text: root\.tr\("battery\.lastReported", "Battery values are last reported\."\)/);
    assert.equal((widgetSource.match(/root\.tr\("battery\.lastReported"/g) || []).length, 1);
    assert.match(widgetSource, /visible: text !== ""\s+text: modelData.present === true && modelData.value === null\s*\? root.tr\("battery.presentNoLevel"/);
    assert.match(widgetSource, /: root\.batteryStatusText\(modelData.present, modelData.charging, modelData.isCase\)/);
  },
  'event detector owns no subprocess needing descendant cleanup': () => {
    assert.doesNotMatch(require('../qml-source.js').read('CallDetector.qml'), /Process|\.signal\(/);
  },
};

let failed = 0;
for (const [name, run] of Object.entries(cases)) {
  try { run(); console.log(`PASS ${name}`); }
  catch (error) { failed++; console.error(`FAIL ${name}: ${error.stack}`); }
}
console.log(`Service-lifecycle fixtures: ${Object.keys(cases).length - failed}/${Object.keys(cases).length} passed (not live QML reorder)`);
process.exitCode = failed ? 1 : 0;
