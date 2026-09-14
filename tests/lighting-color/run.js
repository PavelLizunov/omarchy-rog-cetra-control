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
  'UI handlers use host slider, explicit Apply, accessibility and no startup autosend': () => {
    assert.match(widget, /PanelSlider \{/);
    assert.doesNotMatch(widget, /ColorDialog|ColorPicker|QtQuick\.Dialogs|#[0-9a-f]{6}/i);
    assert.doesNotMatch(widget, /on(?:UseThemeColor|LightingRgb|SelectedLightingColor|Accent|Connected)Changed/);
    assert.match(widget, /Component\.onCompleted: root\.languageButton = this/);
    assert.match(widget, /onReleased: function \(value\) \{ root\.setLightingSetting\(modelData\.key, value\) \}/);
    assert.match(widget, /onMoved: root\.focusControl\(channelSlider\)/);
    assert.match(widget, /onClicked: root\.applyLightingColor\(\)/);
    assert.match(widget, /\? root\.tr\("lighting\.applyColor", "Apply color"\) : root\.tr\("lighting\.applyStaticColor", "Apply static color"\)/);
    assert.match(widget, /Accessible\.role: Accessible\.Slider/);
    assert.match(widget, /Accessible\.name: modelData\.label/);
    assert.match(widget, /Keys\.forwardTo: \[panelRoot.keyTarget\]/);
    assert.match(widget, /onTabRequested: function \(direction\) \{ root\.moveFocus\(direction, true\) \}/);
    assert.match(widget, /root\.tr\("lighting\.lastSent",[^\n]+root\.lightingText\(root\.lighting\)/);
    assert.match(widget, /active: root\.lighting === modelData\.value/);
    const { view, writes, status } = fixture({ useThemeColor: false, lightingRed: 17 });
    const a = view();
    status();
    let focused = false;
    a.Qt = { Key_Right: 1, Key_Up: 2, Key_Left: 3, Key_Down: 4, Key_Return: 5, Key_Enter: 6 };
    a.opened = true;
    a.viewport = { contentItem: {}, contentY: 0, contentHeight: 1000, height: 100 };
    a.applyColorButton = { visible: true, enabled: true, height: 30,
      forceActiveFocus() { focused = true; },
      mapToItem(content) { assert.equal(content, a.viewport.contentItem); return { y: 700 }; },
    };
    a.modelData = { key: 'lightingRed' };
    a.value = 17;
    const handler = widget.match(/id: channelSlider[\s\S]*?Keys\.onPressed: function \(event\) \{([\s\S]*?)^            \}/m)[1];
    a.event = { key: a.Qt.Key_Right };
    vm.runInContext(handler, a);
    assert.equal(a.lightingRgb[0], 18);
    assert.equal(a.event.accepted, true);
    a.event = { key: a.Qt.Key_Return };
    vm.runInContext(handler, a);
    assert.equal(focused, true);
    assert.equal(a.viewport.contentY, 630, 'Slider Return reveals Apply using production focusControl');
    focused = false;
    a.viewport.contentY = 0;
    a.event = { key: a.Qt.Key_Enter };
    vm.runInContext(handler, a);
    assert.equal(focused, true);
    assert.equal(a.viewport.contentY, 630);
    assert.deepEqual(writes, []);
  },
  'collapsed color controls are disabled and expansion has a keyboard entry': () => {
    assert.match(widget, /id: deviceSettingsToggle[\s\S]*?focusable: true/);
    assert.match(widget, /visible: root\.connected && root\.settingsExpanded\s+enabled: visible/);
    assert.match(widget, /visible: root\.lightingColorExpanded\s+enabled: visible/);
    assert.match(widget, /enabled: root\.opened && root\.settingsExpanded && root\.lightingColorExpanded && root\.connected/);
    assert.match(widget, /onSettingsExpandedChanged:[\s\S]*?root\.focusControl\(deviceSettingsToggle\)/);
    assert.match(widget, /onLightingColorExpandedChanged:[\s\S]*?root\.focusControl\(lightingPaletteToggle\)/);
  },
  'real Qt QColor and variant calls preserve normalized theme and manual channels': () => {
    const temp = fs.mkdtempSync(path.join(os.tmpdir(), 'cetra-qt-color-'));
    try {
      const flags = execFileSync('pkg-config', ['--cflags', '--libs', 'Qt6Quick', 'Qt6Qml', 'Qt6Gui'], { encoding: 'utf8' }).trim().split(/\s+/);
      const binary = path.join(temp, 'qt-color');
      execFileSync('c++', ['-std=c++17', '-Wall', '-Wextra', '-Werror', path.join(__dirname, 'qt-color.cpp'), '-o', binary, ...flags], { stdio: 'inherit' });
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
