// Execute the production cursor functions and installed host key dispatcher.
// Item focus/geometry are mocked; Qt event forwarding and live layout remain runtime checks.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const { widget: source, read } = require('../qml-source.js');
const host = fs.readFileSync('/usr/share/omarchy/shell/Ui/PanelKeyCatcher.qml', 'utf8');
const dispatchBody = host.match(/^  Keys.onPressed: function\(event\) \{([\s\S]*?)^  \}/m)[1];
const functions = [...source.matchAll(/^  function \w+\([^)]*\) \{[\s\S]*?^  \}/gm)].map(match => match[0]).join('\n');

function fixture() {
  const actions = [], switches = [], controls = [];
  const ctx = vm.createContext({ opened: true, connected: true, settingsExpanded: false,
    ancAdaptive: null, proximity: null, listeningMode: 'off', pendingSettings: {},
    blocked: false, column: { visible: true, enabled: true, children: [] },
    viewport: { contentItem: {}, contentY: 0, contentHeight: 1000, height: 100 },
    switchPanel: direction => switches.push(direction), close: () => { ctx.opened = false; },
  });
  ctx.root = ctx;
  vm.runInContext(functions, ctx);
  for (const name of ['setListeningMode', 'setAncLevel', 'setAncAdaptive', 'setProximity'])
    ctx[name] = value => actions.push([name, value]);
  const names = ['Escape', 'Tab', 'Backtab', 'Down', 'Up', 'Right', 'Left', 'Return', 'Enter', 'Space'];
  ctx.Qt = Object.fromEntries(names.map((name, i) => [`Key_${name}`, i + 1]));
  ctx.Qt.ShiftModifier = 1;
  ctx.closeRequested = () => ctx.close();
  ctx.tabRequested = direction => ctx.moveFocus(direction, true);
  ctx.moveRequested = (dx, dy) => ctx.moveFocus(dx || dy, false);
  ctx.activateRequested = () => ctx.activateFocus();
  ctx.returnRequested = () => {};
  ctx.deleteRequested = () => {};
  ctx.textKey = text => ctx.handleTextKey(text);
  const dispatch = vm.runInContext(`(function(event) {${dispatchBody}})`, ctx);
  const key = (name, text = '', modifiers = 0) => {
    const event = { key: ctx.Qt[`Key_${name}`] || 100, text, modifiers, accepted: false };
    dispatch(event);
    return event;
  };
  function item(name, parent = ctx.column, action = true) {
    const control = { name, visible: true, enabled: true, activeFocusOnTab: true, activeFocus: false,
      height: 30, children: [], mapToItem: () => ({ y: controls.indexOf(control) * 40 }),
      forceActiveFocus() { controls.forEach(item => { item.activeFocus = false; }); this.activeFocus = true; },
    };
    if (action) control.clicked = () => actions.push(name);
    controls.push(control);
    parent.children.push(control);
    return control;
  }
  return { ctx, actions, switches, controls, item, key };
}

{
  const { ctx, item, key, actions, switches, controls } = fixture();
  const first = item('noise');
  item('anc-level');
  item('adaptive');
  const info = item('microphone-info', ctx.column, false);
  const settings = item('settings');
  const hidden = { visible: false, enabled: true, children: [] };
  ctx.column.children.push(hidden);
  item('hidden-proximity', hidden);
  const disabled = item('pending');
  disabled.enabled = false;
  assert.equal(key('Down').accepted, true);
  assert.equal(first.activeFocus, true);
  key('Return');
  assert.deepEqual(actions, ['noise']);
  key('Tab'); key('Space');
  assert.deepEqual(actions, ['noise', 'anc-level']);
  key('Right'); key('Enter');
  assert.equal(actions.at(-1), 'adaptive');
  key('Down');
  assert.equal(info.activeFocus, true);
  key('Return');
  assert.equal(actions.length, 3, 'Microphone information has no action');
  key('Tab');
  assert.equal(settings.activeFocus, true);
  assert.ok(ctx.viewport.contentY > 0, 'Keyboard target is scrolled into view');
  key('Tab');
  assert.deepEqual(switches, [1], 'Tab at final control keeps host panel switching');
  first.forceActiveFocus();
  key('Backtab');
  assert.deepEqual(switches, [1, -1]);
  key('Tab', '', ctx.Qt.ShiftModifier);
  assert.deepEqual(switches, [1, -1, -1]);
  key('Up');
  assert.equal(settings.activeFocus, true, 'Arrow traversal wraps visible enabled controls');
  hidden.visible = true;
  key('Down'); key('Space');
  assert.equal(actions.at(-1), 'hidden-proximity');
  hidden.enabled = false;
  key('Return');
  assert.equal(actions.length, 4, 'A newly hidden/disabled focused item cannot activate');
  key('Escape');
  assert.equal(ctx.opened, false);
  key('Return'); key('Text', 'n');
  assert.equal(actions.length, 4);
  assert.equal(controls.filter(control => control.activeFocus).length, 1);
}

{
  const { ctx, item, key, actions } = fixture();
  for (const name of ['noise-off', 'noise-anc', 'noise-ambient', 'low', 'mid', 'high', 'adaptive',
    'microphone-info', 'settings', 'lighting-off', 'lighting-cycle', 'static', 'breathing', 'strobing',
    'palette', 'theme-color', 'red', 'green', 'blue', 'apply', 'voice-english', 'voice-chinese', 'voice-sound',
     'always-call']) item(name, ctx.column, name !== 'microphone-info');
  for (let i = 0; i < ctx.column.children.length; i++) {
    key('Down');
    assert.equal(ctx.column.children[i].activeFocus, true);
  }
  for (const text of ['1', '2', '3', 'p', 'm']) key('Text', text);
  assert.deepEqual(actions, [], 'Shortcuts do not act on collapsed sections');
  ctx.listeningMode = 'anc';
  key('Text', '2');
  assert.deepEqual(actions, [['setAncAdaptive', false], ['setAncLevel', 2]], 'Manual selection explicitly disables unknown adaptive');
  ctx.ancAdaptive = true;
  key('Text', '1');
  assert.deepEqual(actions.slice(-2), [['setAncAdaptive', false], ['setAncLevel', 1]]);
  ctx.settingsExpanded = true;
  const beforeRemovedShortcut = actions.length;
  for (const text of ['p', 'P', 'з', 'З', 'm', 'M', 'ь', 'Ь']) key('Text', text);
  assert.equal(actions.length, beforeRemovedShortcut, 'Removed auto-pause shortcuts do nothing');
  for (const text of ['n', 'N', '\u0442', '\u0422']) {
    const count = actions.length;
    key('Text', text);
    assert.equal(actions.length, count + 1);
    assert.deepEqual(actions.at(-1), ['setListeningMode', 'anc']);
  }
  ctx.connected = false;
  const count = actions.length;
  for (const text of ['o', 'n', 'a', '1', 'p', 'm']) key('Text', text);
  assert.equal(actions.length, count);
}

{
  const buttonAction = source.match(/onClicked: (root\.chooseAncLevel\(modelData.value\))/)[1];
  for (const adaptive of [null, true, false]) for (const pending of [{}, { anc_level: {} }, { anc_adaptive: {} }]) {
    const { ctx, key, actions } = fixture();
    ctx.listeningMode = 'anc';
    ctx.ancAdaptive = adaptive;
    ctx.pendingSettings = pending;
    for (const level of [1, 2, 3]) {
      const expected = Object.keys(pending).length ? []
        : [...(adaptive !== false ? [['setAncAdaptive', false]] : []), ['setAncLevel', level]];
      actions.length = 0;
      key('Text', String(level));
      assert.deepEqual(actions, expected);
      actions.length = 0;
      ctx.modelData = { value: level };
      vm.runInContext(buttonAction, ctx);
      assert.deepEqual(actions, expected, 'Button and shortcut execute the same guarded production helper');
    }
  }
  const { ctx, actions } = fixture();
  ctx.listeningMode = 'anc';
  for (const value of [null, '1', 0, 4, 1.5]) ctx.chooseAncLevel(value);
  ctx.opened = false;
  ctx.chooseAncLevel(1);
  assert.deepEqual(actions, []);
}

{
  const { ctx, item } = fixture();
  const palette = item('palette');
  const apply = item('apply');
  let y = 600;
  apply.mapToItem = content => { assert.equal(content, ctx.viewport.contentItem); return { y }; };
  ctx.focusControl(apply);
  assert.equal(apply.activeFocus, true);
  assert.equal(ctx.viewport.contentY, 530);
  y = 550;
  ctx.focusControl(apply);
  assert.equal(ctx.viewport.contentY, 530, 'Already visible target does not jump');
  ctx.lightingPaletteToggle = palette;
  ctx.lightingColorExpanded = false;
  const collapse = source.match(/onLightingColorExpandedChanged: \{([\s\S]*?)^  \}/m)[1];
  vm.runInContext(collapse, ctx);
  assert.equal(palette.activeFocus, true);
  assert.equal(ctx.viewport.contentY, 0, 'Collapsing palette reveals its toggle');
  ctx.deviceSettingsToggle = palette;
  ctx.settingsExpanded = false;
  ctx.viewport.contentY = 530;
  vm.runInContext(source.match(/onSettingsExpandedChanged: \{([\s\S]*?)^  \}/m)[1], ctx);
  assert.equal(ctx.viewport.contentY, 0, 'Collapsing settings reveals its toggle');
  ctx.languageButton = palette;
  ctx.languageExpanded = false;
  apply.forceActiveFocus();
  vm.runInContext(source.match(/onLanguageExpandedChanged: \{([\s\S]*?)^  \}/m)[1], ctx);
  assert.equal(palette.activeFocus, true, 'Language collapse restores its opener');
  for (const field of ['visible', 'enabled']) {
    apply[field] = false;
    ctx.focusControl(apply);
    assert.equal(palette.activeFocus, true);
    apply[field] = true;
  }
  ctx.opened = false;
  ctx.focusControl(apply);
  ctx.focusControl(null);
  assert.equal(palette.activeFocus, true);
}

// Wiring checks tie the exercised cursor model to every actual production control.
assert.equal((source.match(/\bButton \{/g) || []).length, 1, 'Only the shared focusable button may use raw Button');
assert.equal((source.match(/\bToggleSwitch \{/g) || []).length, 1, 'All switches use the single-click-owner row');
assert.equal((source.match(/\bSettingToggle \{/g) || []).length, 3);
assert.doesNotMatch(source, /alwaysCallContext|setAlwaysCallContext/);
assert.doesNotMatch(source, /settings\.autoPause|setProximity|root\.proximity/);
{
  const handler = source.match(/\/\/ Retranslation rebuilds the Repeater; close before saving\.\n([\s\S]*?)^        \}/m)[1];
  let closed = false, saved = '';
  const ctx = vm.createContext({ modelData: { code: 'ru' }, root: {
    set languageExpanded(value) { closed = value === false; },
    setLocaleSetting(code) { assert.equal(closed, true); saved = code; },
  } });
  vm.runInContext(handler, ctx);
  assert.equal(saved, 'ru', 'Click captures the chosen code before retranslation destroys delegates');
}
assert.match(read('ControlButton.qml'), /Button \{[\s\S]*?focusable: true\s*Keys.forwardTo: \[panelRoot.keyTarget\]/);
assert.match(read('SettingToggle.qml'), /FocusScope/);
assert.match(source, /checked: toggleRow.value === true\s*interactive: false/);
assert.match(source, /Accessible.role: Accessible.CheckBox/);
assert.match(source, /Accessible.role: Accessible.StaticText/);
const catcher = source.match(/PanelKeyCatcher \{[\s\S]*?\n      Flickable \{/)[0];
assert.doesNotMatch(catcher, /Keys.onPressed|blocked:/);
assert.match(catcher, /onTextKey: function \(t\) \{ root.handleTextKey\(t\) \}/);
assert.match(catcher, /onCloseRequested: root.close\(\)/);
assert.match(source, /Keys.onTabPressed: root.moveFocus\(1, true\)/);
assert.match(source, /Keys.onBacktabPressed: root.moveFocus\(-1, true\)/);
console.log('PASS keyboard: 4 behavioral groups + control wiring (offline, not Qt event delivery)');
