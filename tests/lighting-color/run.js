// Production QML functions/bindings, mocked shell and transport; no HID or QML process.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const os = require('node:os');
const { execFileSync } = require('node:child_process');

const dir = path.resolve(__dirname, '../..');
const { widget, service } = require('../qml-source.js');
const manifest = JSON.parse(fs.readFileSync(path.join(dir, 'manifest.json'), 'utf8'));
const panel = fs.readFileSync('/usr/share/omarchy/shell/Ui/Panel.qml', 'utf8');

function functions(ctx, source) {
  for (const match of source.matchAll(/^  function \w+\([^)]*\) \{[\s\S]*?^  \}/gm))
    vm.runInContext(match[0], ctx);
}

function bind(ctx, source, name) {
  const expression = source.match(new RegExp(`^  readonly property \\w+ ${name}: (.+)$`, 'm'))[1];
  const body = expression === '{'
    ? source.match(new RegExp(`^  readonly property \\w+ ${name}: \\{\\n([\\s\\S]*?)^  \\}`, 'm'))[1]
    : `return (${expression})`;
  Object.defineProperty(ctx, name, {
    get: () => vm.runInContext(`(function() {${body}})()`, ctx),
    set: () => { throw new Error(`readonly binding overwritten: ${name}`); },
  });
}

function fixture(saved = {}) {
  const writes = [];
  const persisted = [];
  const entry = { id: manifest.id, alwaysCallContext: true, showPercentage: false, customOther: { keep: true }, ...saved };
  // Only the public, self-scoped API is injected. Updates replace, not merge.
  const host = {
    barConfig: { layout: { left: [], center: [], right: [entry] } },
    updateEntryInline(id, settings) {
      if (id !== manifest.id) return false;
      const config = JSON.parse(JSON.stringify(this.barConfig));
      let dirty = false;
      for (const section of ['left', 'center', 'right'])
        config.layout[section] = (config.layout[section] || []).map(current => {
          if (current?.id !== id) return current;
          const next = { ...settings, id };
          if (JSON.stringify(current) !== JSON.stringify(next)) dirty = true;
          return next;
        });
      if (!dirty) return false;
      this.barConfig = config;
      persisted.push(config);
      return true;
    },
  };
  const registry = { pluginId: manifest.id, manifest, enabled: true };
  const owner = vm.createContext({
    shell: host, manifest, pluginRegistry: registry,
    deviceWatchProc: { running: true, write(text) { writes.push(text); } },
    modeRequestTimeout: { stop() {} },
    settingsRequestTimeout: { stop() {} },
    preferenceReadback: { stop() {}, restart() {} },
    themeColorDelay: { running: false, stop() { this.running = false; }, restart() { this.running = true; } },
    themeColor: { r: 0.2, g: 0.4, b: 0.6 },
  });
  owner.root = owner;
  for (const match of service.matchAll(/^  property (?:string|bool|int|var) (\w+): (.+)$/gm))
    if (!['shell', 'manifest', 'pluginRegistry'].includes(match[1])) owner[match[1]] = vm.runInContext(`(${match[2]})`, owner);
  for (const name of ['hostReady', 'hostSettings', 'settings', 'autoThemeColor']) bind(owner, service, name);
  functions(owner, service);
  host.serviceFor = id => id === manifest.id ? owner : null;
  function view() {
    const ctx = vm.createContext({ bar: { shell: host }, moduleName: manifest.id, accent: { r: 0.2, g: 0.4, b: 0.6 } });
    ctx.root = ctx;
    Object.defineProperty(ctx, 'settings', { configurable: true, get: () => owner.settings });
    functions(ctx, panel);
    functions(ctx, widget);
    for (const name of ['service', 'connected', 'lighting', 'useThemeColor', 'lightingRgb', 'selectedLightingColor', 'colorApplyEffect'])
      bind(ctx, widget, name);
    return ctx;
  }
  function status(lighting = 'unknown') {
    owner.applyDeviceState(JSON.stringify({ status: 'ok', receiver: true, connected: true, lighting }));
  }
  return { owner, host, view, writes, persisted, status };
}

const cases = {
  'automatic theme color is opt-in, session-gated, deduplicated and preserves Off/Cycle': () => {
    const { owner, view, status, writes } = fixture({ autoThemeColor: true });
    const a = view(), b = view();
    status('static');
    owner.scheduleThemeColor();
    assert.equal(owner.themeColorDelay.running, false, 'Saved permission alone cannot write on startup');
    assert.equal(owner.applyThemeColor(), false);
    a.setLighting('breathing');
    status('breathing');
    owner.themeColor = { r: 1, g: 0, b: 0 };
    owner.scheduleThemeColor();
    owner.themeColor = { r: 0, g: 1, b: 0 };
    owner.scheduleThemeColor();
    assert.equal(writes.length, 1, 'Theme changes only arm the one-shot timer');
    assert.equal(owner.applyThemeColor(), true);
    assert.equal(writes.at(-1), 'lighting breathing 0 255 0\n');
    assert.equal(owner.applyThemeColor(), false, 'Repeated color is deduplicated');
    assert.equal(owner.lighting, 'breathing', 'No invented color readback');
    b.setLightingSetting('useThemeColor', false);
    owner.themeColor = { r: 0, g: 0, b: 1 };
    assert.equal(owner.applyThemeColor(), false);
    b.setLightingSetting('useThemeColor', true);
    for (const effect of ['off', 'cycle']) {
      a.setLighting(effect); status(effect);
      assert.equal(owner.sessionLightingEffect, '');
      assert.equal(owner.applyThemeColor(), false);
    }
    a.setLighting('static'); status('static');
    owner.deviceWatchProc.running = false;
    assert.equal(owner.applyThemeColor(), false);
    owner.clearDeviceState();
    owner.deviceWatchProc.running = true;
    status('static');
    assert.equal(owner.applyThemeColor(), false, 'Receiver/helper reset requires a new explicit apply');
    assert.match(service, /id: themeColorDelay\s+interval: 350\s+onTriggered: root\.applyThemeColor\(\)/);
  },
  'explicit auto-color enable applies an existing colored effect without replacing Off or Cycle': () => {
    for (const effect of ['unknown', 'off', 'cycle', 'static', 'breathing', 'strobing']) {
      const { owner, status, writes } = fixture();
      status(effect);
      assert.equal(owner.setAutoThemeColor(true), true);
      const colored = ['static', 'breathing', 'strobing'].includes(effect);
      assert.equal(writes.length, colored ? 1 : 0);
      assert.equal(owner.setAutoThemeColor(false), true);
      assert.equal(owner.applyThemeColor(), false);
    }
  },
  'manifest declares persisted integer RGB and theme enabled by default': () => {
    assert.equal(manifest.barWidget.defaults.useThemeColor, true);
    for (const key of ['lightingRed', 'lightingGreen', 'lightingBlue']) {
      const field = manifest.barWidget.schema.find(field => field.key === key);
      assert.equal(field.type, 'integer');
      assert.deepEqual([field.min, field.max, field.step, field.defaultValue], [0, 255, 1, 255]);
      assert.equal(manifest.barWidget.defaults[key], 255);
    }
  },
  'startup, settings, theme, two views and reconnect do not send lighting': () => {
    const { view, owner, status, writes, persisted } = fixture();
    const a = view();
    const b = view();
    assert.equal(a.useThemeColor, true);
    assert.equal(a.lighting, 'unknown');
    assert.deepEqual(writes, []);
    assert.deepEqual(persisted, []);
    status();
    a.setLightingSetting('lightingRed', 17);
    a.setLightingSetting('lightingGreen', 34);
    a.setLightingSetting('lightingBlue', 51);
    a.setLightingSetting('useThemeColor', false);
    assert.equal(b.selectedLightingColor.r, 17 / 255);
    a.accent = { r: 1, g: 0, b: 0 };
    owner.clearDeviceState();
    status();
    assert.deepEqual(writes, []);
    assert.equal(a.applyLightingColor(), true);
    assert.deepEqual(writes, ['lighting static 17 34 51\n']);
    assert.equal(b.lighting, 'unknown');
    status('static');
    assert.equal(b.lighting, 'static');
  },
  'scoped settings API preserves the canonical own entry in every bar section': () => {
    for (const section of ['left', 'center', 'right']) {
      const { owner, view, host, writes, persisted } = fixture();
      if (section !== 'right') host.barConfig.layout[section].push(host.barConfig.layout.right.pop());
      const a = view();
      const b = view();
      assert.equal(a.setLightingSetting('lightingRed', 0), true);
      assert.equal(b.setLightingSetting('lightingGreen', 127), true);
      assert.equal(a.setLightingSetting('lightingBlue', 255), true);
      assert.equal(b.setLightingSetting('useThemeColor', false), true);
      assert.equal(b.setLightingSetting('useThemeColor', false), false);
      const actual = owner.settings;
      assert.equal(actual.id, manifest.id);
      assert.equal(actual.alwaysCallContext, true);
      assert.equal(actual.showPercentage, false);
      assert.equal(actual.customOther.keep, true);
      assert.deepEqual(Array.from(a.lightingRgb), [0, 127, 255]);
      assert.equal(persisted.length, 4);
      const moved = section === 'left' ? 'right' : 'left';
      host.barConfig.layout[moved].push(host.barConfig.layout[section].pop());
      b.setLightingSetting('lightingBlue', 12);
      assert.equal(a.lightingRgb[2], 12);
      assert.deepEqual(writes, []);
      a.bar = null;
      assert.equal(a.setLightingSetting('lightingRed', 99), false);
      assert.equal(a.setLighting('static'), false);
    }
  },
  'stale widget settings cannot undo another view or an external canonical update': () => {
    const { owner, view, host } = fixture();
    const a = view(), b = view();
    Object.defineProperty(a, 'settings', { value: { lightingRed: 1, customFallback: 'keep' } });
    host.barConfig = { layout: { center: [null, 'neighbor', { id: 'neighbor', lightingRed: 200 },
      { ...owner.settings, lightingRed: 17, locale: 'en' }] } };
    assert.equal(a.setLightingSetting('lightingGreen', 34), true);
    assert.equal(b.setLightingSetting('useThemeColor', false), true);
    assert.equal(owner.settings.lightingRed, 17);
    assert.equal(owner.settings.lightingGreen, 34);
    assert.equal(owner.settings.locale, 'en');
    assert.equal(owner.settings.customFallback, 'keep');
    assert.equal(owner.settings.useThemeColor, false);
    assert.equal(a.setLightingSetting('lightingBlue', 51), true);
    assert.equal(owner.settings.useThemeColor, false);
  },
  'empty or unavailable service settings preserve the widget fallback': () => {
    for (const unavailable of [null, {}, { settings: {} }, { settings: undefined }]) {
      const { view, host } = fixture();
      const a = view();
      host.serviceFor = () => unavailable;
      Object.defineProperty(a, 'settings', { value: { showPercentage: false, locale: 'en', lightingRed: 17, customOther: { keep: true } } });
      assert.equal(a.setLightingSetting('lightingGreen', 34), true);
      assert.equal(a.setLightingSetting('useThemeColor', false), true);
      const saved = host.barConfig.layout.right[0];
      assert.equal(saved.showPercentage, false);
      assert.equal(saved.locale, 'en');
      assert.equal(saved.lightingRed, 17);
      assert.equal(saved.customOther.keep, true);
    }
  },
  'missing layout and foreign entries yield no canonical own settings': () => {
    const { owner, host } = fixture();
    for (const config of [undefined, {}, { layout: {} }, { layout: { left: null, center: {}, right: [null, manifest.id, { id: 'neighbor', alwaysCallContext: true }] } }]) {
      host.barConfig = config;
      assert.deepEqual(Object.keys(owner.settings), []);
    }
  },
  'theme selection and all explicit effects emit the selected RGB only on action': () => {
    const { view, writes, status } = fixture({ lightingRed: 17, lightingGreen: 34, lightingBlue: 51 });
    const a = view();
    status();
    for (const effect of ['static', 'breathing', 'strobing']) {
      a.setLighting(effect);
      assert.equal(writes.at(-1), `lighting ${effect} 51 102 153\n`);
    }
    a.setLightingSetting('useThemeColor', false);
    for (const effect of ['static', 'breathing', 'strobing']) {
      a.setLighting(effect);
      assert.equal(writes.at(-1), `lighting ${effect} 17 34 51\n`);
      status(effect);
      a.applyLightingColor();
      assert.equal(writes.at(-1), `lighting ${effect} 17 34 51\n`);
    }
    for (const effect of ['off', 'cycle']) {
      a.setLighting(effect);
      assert.equal(writes.at(-1), `lighting ${effect} 0 0 0\n`);
      status(effect);
      const count = writes.length;
      a.setLightingSetting('lightingBlue', 52);
      assert.equal(writes.length, count);
      assert.equal(a.colorApplyEffect, 'static');
      a.applyLightingColor();
      assert.equal(writes.at(-1), 'lighting static 17 34 52\n');
    }
    a.setLightingSetting('useThemeColor', true);
    a.accent = { r: 0, g: 0.5, b: 1 };
    a.applyLightingColor();
    assert.equal(writes.at(-1), 'lighting static 0 128 255\n');
  },
  'invalid saved RGB rejects colored writes before QColor can clamp or coerce it': () => {
    for (const key of ['lightingRed', 'lightingGreen', 'lightingBlue']) {
      for (const value of [null, -1, 256, 1.5, NaN, Infinity, -Infinity, '12', false, {}, []]) {
        const { view, writes, status } = fixture({ useThemeColor: false, [key]: value });
        const a = view();
        status();
        assert.equal(a.selectedLightingColor, null);
        assert.equal(a.applyLightingColor(), false);
        assert.deepEqual(writes, []);
        // Off remains available even with corrupt custom settings.
        assert.equal(a.setLighting('off'), true);
        assert.equal(writes.at(-1), 'lighting off 0 0 0\n');
      }
    }
  },
  'invalid settings updates never persist or write': () => {
    const { view, persisted, writes } = fixture();
    const a = view();
    for (const key of ['lightingRed', 'lightingGreen', 'lightingBlue'])
      for (const value of [undefined, null, -1, 256, 1.1, NaN, Infinity, '12', true, {}])
        assert.equal(a.setLightingSetting(key, value), false);
    for (const value of [undefined, null, 0, 1, 'true', {}]) {
      assert.equal(a.setLightingSetting('useThemeColor', value), false);
    }
    assert.equal(a.setLightingSetting('alwaysCallContext', false), false);
    assert.deepEqual(persisted, []);
    assert.deepEqual(writes, []);
  },
  'service validates effects and finite normalized RGB without optimistic state': () => {
    const { owner, writes, status } = fixture();
    const good = { r: 0, g: 0.5, b: 1 };
    assert.equal(owner.setLighting('static', good), false);
    status('breathing');
    owner.deviceWatchProc.running = false;
    assert.equal(owner.setLighting('static', good), false);
    owner.deviceWatchProc.running = true;
    for (const effect of ['', 'STATIC', 'static\ncall on', undefined, {}, 1])
      assert.equal(owner.setLighting(effect, good), false);
    for (const color of [null, undefined, {}, 'red', { r: 0, g: 0 }])
      assert.equal(owner.setLighting('static', color), false);
    for (const key of ['r', 'g', 'b'])
      for (const value of [undefined, null, -0.01, 1.01, NaN, Infinity, -Infinity, '0.5', false])
        assert.equal(owner.setLighting('static', { ...good, [key]: value }), false);
    assert.deepEqual(writes, []);
    assert.equal(owner.lighting, 'breathing');
    assert.equal(owner.setLighting('strobing', good), true);
    assert.equal(owner.lighting, 'breathing');
    assert.equal(writes.at(-1), 'lighting strobing 0 128 255\n');
    status('strobing');
    assert.equal(owner.lighting, 'strobing');
  },
  'all 256 custom channel integers round-trip through service serialization': () => {
    const { view, status, writes } = fixture({ useThemeColor: false });
    const a = view();
    status();
    for (let channel = 0; channel <= 255; channel++) {
      a.setLightingSetting('lightingRed', channel);
      a.setLightingSetting('lightingGreen', 255 - channel);
      a.setLightingSetting('lightingBlue', channel);
      a.applyLightingColor();
      assert.equal(writes.at(-1), `lighting static ${channel} ${255 - channel} ${channel}\n`);
    }
  },
  'UI handlers use visual palette, explicit Apply, accessibility and no startup autosend': () => {
    assert.match(widget, /LightingColorField \{/);
    assert.doesNotMatch(widget, /ColorDialog|ColorPicker|QtQuick\.Dialogs|#[0-9a-f]{6}/i);
    assert.doesNotMatch(widget, /on(?:UseThemeColor|LightingRgb|SelectedLightingColor|Accent)Changed/);
    assert.match(widget, /onConnectedChanged: \{\s*if \(connected\) root\.bluetoothExpanded = false\s*else root\.cancelLightingEdit\(\)\s*if \(opened\) Qt.callLater\(root.focusCurrentTab\)\s*\}/, "Connection handler collapses details and repairs focus; no lighting replay");
    assert.match(widget, /Component\.onCompleted: root\.languageButton = this/);
    assert.match(widget, /root\.focusControl\(wheel\)/);
    assert.match(widget, /onClicked: root\.applyLightingColor\(\)/);
    assert.match(widget, /\? root\.tr\("lighting\.applyColor", "Apply color"\) : root\.tr\("lighting\.applyStaticColor", "Apply static color"\)/);
    assert.match(widget, /Accessible\.role: Accessible\.Slider/);
    assert.match(widget, /Accessible\.name: root\.tr\("lighting\.palette"/);
    assert.match(widget, /Keys\.forwardTo: \[panelRoot.keyTarget\]/);
    assert.match(widget, /onTabRequested: function \(direction\) \{ root\.moveFocus\(direction, true\) \}/);
    assert.match(widget, /root\.tr\("lighting\.lastSent",[^\n]+root\.lightingText\(root\.lighting\)/);
    assert.match(widget, /active: root\.lighting === modelData\.value/);
    const source = fs.readFileSync(path.join(dir, 'LightingColorField.qml'), 'utf8');
    const ctx = vm.createContext({hue: 0.5, saturation: 0.5, brightness: 0.5,
      wheel: {width: 200, height: 200}, publishes: 0});
    functions(ctx, source);
    ctx.publishDraft = () => ctx.publishes++;
    ctx.choosePoint(200,100); assert.equal(ctx.hue,0); assert.equal(ctx.saturation,1);
    ctx.choosePoint(100,0); assert.equal(ctx.hue,0.25);
    ctx.choosePoint(100,100); assert.equal(ctx.hue,0.25); assert.equal(ctx.saturation,0);
    ctx.choosePoint(500,100); assert.equal(ctx.saturation,1);
    ctx.wheel.width = 0; ctx.choosePoint(0,0); assert.equal(ctx.saturation,1);
    ctx.wheel.width = 200;
    let accepted = false;
    ctx.field = ctx; ctx.width = 200; ctx.height = 200;
    ctx.acceptRequested = () => { accepted = true; };
    ctx.Qt = {Key_Left:1,Key_Right:2,Key_Up:3,Key_Down:4,Key_Return:5,Key_Enter:6};
    const handler = source.match(/id: wheel[\s\S]*?Keys\.onPressed: function\(event\) \{([\s\S]*?)^    \}/m)[1];
    ctx.event = {key:2}; ctx.hue = 0; ctx.saturation = 0.5;
    vm.runInContext(`(function(){${handler}})()`,ctx); assert.ok(Math.abs(ctx.hue-1/360)<1e-9);
    ctx.event = {key:4}; vm.runInContext(`(function(){${handler}})()`,ctx); assert.equal(ctx.saturation,0.49);
    ctx.event = {key:5}; vm.runInContext(`(function(){${handler}})()`,ctx); assert.equal(accepted,true);

  },
  'collapsed color controls are disabled and expansion has a keyboard entry': () => {
    assert.match(widget, /id: deviceTab[\s\S]*?onClicked: root\.showPage\("device"\)/);
    assert.match(widget, /visible: root\.panelPage === "device" && !root\.languageExpanded\s+enabled: visible/);
    assert.match(widget, /visible: root\.lightingColorExpanded\s+enabled: visible/);
    assert.match(widget, /enabled: root\.opened && root\.settingsExpanded && !root\.lightingColorExpanded && root\.connected/);
    assert.match(widget, /root\.panelPage === "device" \? deviceTab : soundTab/);
    assert.match(widget, /onLightingColorExpandedChanged:[\s\S]*?root\.focusControl\(root\.lightingPaletteToggle\)/);
  },
  'atomic RGB selection validates input and preserves canonical settings': () => {
    const {owner,host,persisted,writes}=fixture({autoThemeColor:true,locale:'en'});
    for(const bad of [null,{},[1,2],[1,2,3,4],[-1,0,0],[256,0,0],[0,.5,0],[NaN,0,0],[Infinity,0,0],['1',2,3]])
      assert.equal(owner.updateLightingColor(bad),false);
    assert.equal(persisted.length,0);
    assert.equal(owner.updateLightingColor([12,34,56],{locale:'ru',fallbackKey:'keep'}),true);
    assert.equal(persisted.length,1);
    assert.equal(owner.settings.locale,'en');assert.equal(owner.settings.fallbackKey,'keep');
    assert.equal(owner.settings.autoThemeColor,true);assert.equal(owner.settings.customOther.keep,true);
    for(const [key,value] of Object.entries({lightingRed:12,lightingGreen:34,lightingBlue:56,useThemeColor:false})) {
      assert.equal(owner.settings[key],value);assert.equal(owner.pendingPreferences[key],value);
    }
    assert.equal(owner.updateLightingColor([12,34,56]),true);assert.equal(persisted.length,1);
    assert.deepEqual(writes,[]);
    const state=JSON.stringify(owner.inlineSettings),pending=JSON.stringify(owner.pendingPreferences);
    host.updateEntryInline=()=>false;
    assert.equal(owner.updateLightingColor([1,2,3]),false);
    assert.equal(JSON.stringify(owner.inlineSettings),state);assert.equal(JSON.stringify(owner.pendingPreferences),pending);
  },
  'color draft cancels without saving and applies in one action': () => {
    const {owner,view,status,persisted,writes,host}=fixture();status('static');const a=view();
    Object.assign(a,{opened:true,panelPage:'device',devicePage:'color',lightingColorExpanded:false});
    a.beginLightingEdit();assert.equal(a.lightingColorExpanded,true);
    assert.deepEqual(Array.from(a.lightingDraftRgb),[51,102,153]);
    a.setLightingDraftChannel(0,200);assert.equal(persisted.length,0);
    a.accent={r:1,g:0,b:0};assert.deepEqual(Array.from(a.lightingDraftRgb),[200,102,153]);
    a.cancelLightingEdit();assert.equal(a.lightingColorExpanded,false);assert.equal(persisted.length,0);
    a.beginLightingEdit();assert.deepEqual(Array.from(a.lightingDraftRgb),[255,0,0]);
    for(const [i,v] of [[-1,2],[3,1],[0,NaN],[0,1.5],[0,256]]) a.setLightingDraftChannel(i,v);
    assert.deepEqual(Array.from(a.lightingDraftRgb),[255,0,0]);
    a.setLightingDraftChannel(0,200);
    const save=host.updateEntryInline;host.updateEntryInline=()=>false;
    assert.equal(a.commitLightingEdit(),false);assert.equal(a.lightingDraftError,true);assert.equal(a.lightingColorExpanded,true);
    assert.deepEqual(writes,[],'Failed save cannot send a color');
    host.updateEntryInline=save;
    assert.equal(a.commitLightingEdit(),true);assert.equal(persisted.length,1);assert.equal(a.lightingColorExpanded,false);
    assert.equal(owner.settings.useThemeColor,false);assert.equal(owner.settings.lightingRed,200);
    assert.deepEqual(writes,['lighting static 200 0 0\n'],'One Apply sends exactly one command with the accepted draft');
    assert.equal(a.commitLightingEdit(),false,'Closed editor cannot apply a second time');
    assert.equal(writes.length,1);
    a.beginLightingEdit();owner.connected=false;
    assert.equal(a.commitLightingEdit(),false);assert.equal(persisted.length,1);
    a.cancelLightingEdit();a.beginLightingEdit();assert.equal(a.lightingColorExpanded,false);
  },
  'one-step Apply preserves colored effects, handles a stopped helper and avoids stale binding RGB': () => {
    for (const effect of ['unknown','off','cycle','static','breathing','strobing']) {
      const {owner,view,status,writes,persisted}=fixture();status(effect);const a=view();
      Object.assign(a,{opened:true,panelPage:'device',devicePage:'color',lightingColorExpanded:false});
      a.beginLightingEdit();a.setLightingDraftChannel(0,17);a.setLightingDraftChannel(1,34);a.setLightingDraftChannel(2,51);
      owner.deviceWatchProc.running=false;
      assert.equal(a.commitLightingEdit(),false);assert.equal(a.lightingColorExpanded,true);
      assert.equal(a.lightingDraftError,false);assert.equal(a.lightingFeedback,'saved-not-sent');
      assert.equal(persisted.length,1);assert.deepEqual(writes,[]);
      owner.deviceWatchProc.running=true;
      // A stale host snapshot must not change the exact draft passed to the command.
      Object.defineProperty(a,'settings',{value:{useThemeColor:true}});
      assert.equal(a.commitLightingEdit(),true);assert.equal(persisted.length,1);
      const expected=['static','breathing','strobing'].includes(effect)?effect:'static';
      assert.deepEqual(writes,[`lighting ${expected} 17 34 51\n`]);
      assert.equal(a.lightingColorExpanded,false);assert.equal(a.lightingFeedback,'sent');
    }
  },
  'real Qt QColor and variant calls preserve normalized theme and manual channels': () => {
    const temp = fs.mkdtempSync(path.join(os.tmpdir(), 'cetra-qt-color-'));
    try {
      const flags = execFileSync('pkg-config', ['--cflags', '--libs', 'Qt6Quick', 'Qt6Qml', 'Qt6Gui'], { encoding: 'utf8' }).trim().split(/\s+/);
      const binary = path.join(temp, 'qt-color');
      execFileSync('c++', ['-std=c++17', '-fPIC', '-Wall', '-Wextra', '-Werror', path.join(__dirname, 'qt-color.cpp'), '-o', binary, ...flags], { stdio: 'inherit' });
      execFileSync(binary, [dir], { stdio: 'inherit', env: { ...process.env, QT_QPA_PLATFORM: 'offscreen', QML_DISABLE_DISK_CACHE: '1' }, timeout: 30000 });
    } finally {
      fs.rmSync(temp, { recursive: true, force: true });
    }
  },
};

let failed = 0;
for (const [name, run] of Object.entries(cases)) {
  try { run(); console.log(`PASS ${name}`); }
  catch (error) { failed++; console.error(`FAIL ${name}: ${error.stack}`); }
}
console.log(`Lighting-color fixtures: ${Object.keys(cases).length - failed}/${Object.keys(cases).length} passed (offline, no HID writes)`);
process.exitCode = failed ? 1 : 0;
