#!/usr/bin/env node
'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const { test } = require('node:test');
const { pathToFileURL } = require('node:url');

const repo = path.resolve(__dirname, '../..');
const source = fs.readFileSync(path.join(repo, 'I18n.qml'), 'utf8');
const functions = [...source.matchAll(/^  function \w+\([^\n]*\) \{\n[\s\S]*?^  \}/gm)];
assert.equal(functions.length, (source.match(/^  function /gm) || []).length,
  'Extract every production function, not a test reimplementation');
assert.ok(functions.length > 0);
const production = functions.map(match => match[0]).join('\n');
const english = fs.readFileSync(path.join(repo, 'locales/en.json'), 'utf8');
const index = fs.readFileSync(path.join(repo, 'locales/index.json'), 'utf8');
const { widget } = require('../qml-source.js');
const manifest = JSON.parse(fs.readFileSync(path.join(repo, 'manifest.json'), 'utf8'));

function context() {
  const ctx = vm.createContext({
    language: 'system',
    locale: 'en',
    _registry: {},
    _english: {},
    _selection: { locale: '', generation: 0, catalogs: {} },
    _readers: [],
    _generation: 0,
    _ready: true,
    Qt: { resolvedUrl: file => pathToFileURL(path.join(repo, file)) },
  });
  vm.runInContext(production, ctx, { filename: 'I18n.qml:production-functions' });
  ctx.root = ctx;
  ctx._registry = ctx.parseRegistry(index);
  ctx._catalogReader = {
    createObject(parent, { request }) {
      assert.equal(parent.reloadSelection, ctx.reloadSelection);
      const reader = {
        request: Object.freeze(request),
        path: ctx.localPath(request.file),
        destroyed: false,
        destroy() { this.destroyed = true; },
        complete(content) { ctx.acceptCatalog(this.request, content); },
      };
      return reader;
    },
  };
  return ctx;
}

function register(ctx) {
  ctx._registry = ctx.parseRegistry(JSON.stringify({
    fr: { file: 'fr.json', name: 'Base fixture', direction: 'ltr' },
    'fr-CA': { file: 'fr-CA.json', name: 'Region fixture', direction: 'ltr' },
    ar: { file: 'ar.json', name: 'RTL fixture', direction: 'rtl' },
  }));
}

test('locale normalization, auto/system and invalid requests', () => {
  const ctx = context();
  for (const [input, expected] of [
    ['EN_us', 'en-US'], [' pt_bR ', 'pt-BR'], ['ZH_hANT_tw', 'zh-Hant-TW'],
    ['es_419', 'es-419'], ['sr_Latn_RS', 'sr-Latn-RS'], ['de-CH-1901', 'de-CH-1901'],
    ['EN_us_U_CA_GREGORY', 'en-US-u-ca-gregory'], ['zh-cmn-Hans-CN', 'zh-cmn-Hans-CN'],
    ['en-X-CUSTOM', 'en-x-custom'],
  ]) assert.equal(ctx.normalizeLocale(input), expected, input);
  for (const invalid of [null, {}, 1, '', 'C', 'POSIX.UTF-8', 'en_US.UTF-8',
    'sr_RS@latin', 'en--US', 'en-', 'e-US', 'en-u', 'en-x', 'x-private',
    '../fr', 'en/US', 'en\\US', '/tmp/en', 'https://host/fr', 'file:///fr',
    '%2e%2e', 'en?x', 'en#x', 'en\u0000', 'en-\u0440\u0443']) {
    assert.equal(ctx.normalizeLocale(invalid), '', String(invalid));
    assert.equal(ctx.resolveLocale(invalid, 'en'), 'en');
  }
  for (const selection of ['system', 'auto', ' AUTO ', '', null])
    assert.equal(ctx.resolveLocale(selection, 'pt_BR'), 'pt-BR');
  assert.equal(ctx.resolveLocale('fr-ca', 'pt_BR'), 'fr-CA');
  assert.equal(ctx.resolveLocale('auto', 'C'), 'en');
  for (const [tag, expected] of [
    ['fr_CA', ['fr-CA', 'fr', 'en']], ['en_US', ['en-US', 'en']],
    ['en', ['en']], ['ar', ['ar', 'en']], ['zh-Hant-TW', ['zh-Hant-TW', 'zh-Hant', 'en']],
    ['zh-TW', ['zh-TW', 'zh-Hant', 'en']], ['zh-HK', ['zh-HK', 'zh-Hant', 'en']],
    ['zh-MO', ['zh-MO', 'zh-Hant', 'en']], ['zh-Hant', ['zh-Hant', 'en']],
    ['zh-Hant-CN', ['zh-Hant-CN', 'zh-Hant', 'en']], ['zh-Hans-TW', ['zh-Hans-TW', 'zh', 'en']],
    ['zh-CN', ['zh-CN', 'zh', 'en']], ['zh', ['zh', 'en']],
  ]) assert.deepEqual(Array.from(ctx.localeChain(tag)), expected);
});

test('registry validation rejects unsafe filenames and noncanonical entries', () => {
  const ctx = context();
  const valid = { name: 'Fixture', direction: 'ltr', file: 'fr.json' };
  for (const file of ['../fr.json', '/fr.json', 'sub/fr.json', 'sub\\fr.json',
    'https://host/fr.json', 'file:///fr.json', '%2e%2e.json', 'fr.json?x',
    'fr.json#x', '.json', 'fr.JSON', 'fr_fr.json', 'fr\u0000.json', 'fr.json\n', 'fr.json\r', 42, null]) {
    assert.equal(ctx.parseRegistry(JSON.stringify({ fr: { ...valid, file } })).fr, undefined);
    assert.equal(ctx.localPath(file), '');
  }
  for (const entry of [null, [], 'fr.json', {}, { ...valid, direction: 'RTL' },
    { ...valid, name: ' ' }, { ...valid, name: 1 }])
    assert.equal(ctx.parseRegistry(JSON.stringify({ fr: entry })).fr, undefined);
  for (const key of ['FR', 'fr_ca', 'fr-ca', '../fr', '__proto__'])
    assert.equal(Object.keys(ctx.parseRegistry(JSON.stringify({ [key]: valid }))).length, 1);
  assert.equal(ctx.parseRegistry(JSON.stringify({ fr: valid })).fr.file, 'fr.json');
  for (const invalid of ['', '{', '[]', 'null', 'false', '1', '"text"']) {
    assert.deepEqual(Object.keys(ctx.parseRegistry(invalid)), ['en']);
    assert.equal(ctx.parseRegistry(invalid).en.file, 'en.json');
  }
  const override = ctx.parseRegistry('{"en":{"file":"evil.json","name":"Bad","direction":"rtl"}}');
  assert.equal(override.en.file, 'en.json');
  assert.equal(override.en.direction, 'ltr');
  assert.equal(override.en.name, 'English');
  ctx.Qt.resolvedUrl = file => new URL(file, 'https://example.org/plugin/');
  assert.equal(ctx.localPath('fr.json'), '');
  ctx.Qt.resolvedUrl = file => new URL(file, 'file:///tmp/a%20space/%D0%B0/');
  assert.equal(ctx.localPath('fr.json'), '/tmp/a space/\u0430/locales/fr.json');
});

test('Traditional Chinese never reads the bundled Simplified catalog', () => {
  const ctx = context();
  for (const locale of ['zh-Hant', 'zh-Hant-TW', 'zh-TW', 'zh-HK', 'zh-MO']) {
    ctx.locale = locale;
    ctx.reloadSelection();
    assert.equal(ctx._readers.length, 0);
    assert.equal(ctx.effectiveLanguage(), 'en');
  }
  ctx._registry['zh-Hant'] = { file: 'zh-Hant.json', name: 'Traditional fixture', direction: 'ltr' };
  ctx.locale = 'zh-TW';
  ctx.reloadSelection();
  assert.equal(ctx._readers.length, 1);
  ctx._readers[0].complete('{"noise.off":"[Traditional]"}');
  assert.equal(ctx.text('noise.off', 'Off'), '[Traditional]');
  assert.equal(ctx.effectiveLanguage(), 'zh-Hant');
});

test('catalog parsing keeps flat nonempty strings without inherited keys', () => {
  const ctx = context();
  const catalog = ctx.parseCatalog('{"valid.key":"Value","blank":" ","empty":"","number":1,"nested":{},"array":[],"nothing":null,"__proto__":{"polluted":true},"constructor":"bad","prototype":"bad","invalid-key":"bad"}');
  assert.deepEqual(Object.keys(catalog), ['valid.key']);
  assert.equal(Object.getPrototypeOf(catalog), null);
  for (const invalid of ['', '{', '[]', 'null', 'false', '1', '"text"'])
    assert.equal(Object.keys(ctx.parseCatalog(invalid)).length, 0);
  ctx._english = catalog;
  assert.equal(ctx.text('toString', 'Source'), 'Source');
  assert.equal(ctx.text('__proto__', 'Source'), 'Source');
});

test('startup, missing locale/index/catalog and English failures keep source fallback', () => {
  const ctx = context();
  assert.equal(ctx.text('noise.off', 'Off'), 'Off');
  assert.equal(ctx.text('missing.key'), 'missing.key');
  ctx.locale = 'fi-FI';
  ctx.reloadSelection();
  assert.equal(ctx._readers.length, 0, 'Unknown locale must not trigger file reads');
  assert.equal(ctx.text('noise.off', 'Off'), 'Off');
  assert.equal(ctx.effectiveLanguage(), 'en');
  ctx._english = ctx.parseCatalog(english);
  assert.equal(ctx.text('noise.off', 'Different source'), 'Off');
  ctx._registry = ctx.parseRegistry('{');
  ctx.reloadSelection();
  assert.equal(ctx.text('noise.off', 'Source'), 'Off');
  ctx._english = ctx.parseCatalog('{');
  assert.equal(ctx.text('noise.off', 'Source'), 'Source');
});

test('partial catalogs use exact, base, English, source independent of completion order', () => {
  for (const reverse of [false, true]) {
    const ctx = context();
    register(ctx);
    ctx.locale = 'fr-CA';
    ctx.reloadSelection();
    assert.equal(ctx._readers.length, 2);
    const readers = [...ctx._readers];
    if (reverse) readers.reverse();
    for (const reader of readers) reader.complete(JSON.stringify(reader.request.code === 'fr-CA'
      ? { 'noise.off': '[region]', 'noise.anc': '', 'noise.ambient': 5 }
      : { 'noise.off': '[base]', 'noise.anc': '[base ANC]' }));
    assert.equal(ctx.text('noise.off', 'Source'), '[region]');
    assert.equal(ctx.text('noise.anc', 'Source'), '[base ANC]');
    assert.equal(ctx.text('noise.ambient', 'Ambient'), 'Ambient');
    ctx._english = ctx.parseCatalog(english);
    assert.equal(ctx.text('noise.ambient', 'Source'), 'Ambient');
    assert.equal(ctx.text('missing.key', 'Source'), 'Source');
    assert.equal(ctx.effectiveLanguage(), 'fr-CA');
    ctx._readers[0].complete('{');
    assert.equal(ctx.text('noise.off', 'Source'), '[base]');
    assert.equal(ctx.effectiveLanguage(), 'fr');
    ctx._readers[1].complete('');
    assert.equal(ctx.text('noise.off', 'Source'), 'Off');
    assert.equal(ctx.effectiveLanguage(), 'en');
  }
});

test('language changes clear old translations and reject late A -> B -> A callbacks', () => {
  const ctx = context();
  register(ctx);
  ctx._english = ctx.parseCatalog(english);
  ctx.locale = 'fr-CA';
  ctx.reloadSelection();
  const first = [...ctx._readers];
  first[0].complete('{"noise.off":"[old region]"}');
  assert.equal(ctx.text('noise.off', 'Source'), '[old region]');
  ctx.locale = 'ar';
  assert.equal(ctx.text('noise.off', 'Source'), 'Off', 'Guard even before change handler runs');
  ctx.reloadSelection();
  const second = ctx._readers[0];
  assert.ok(first.every(reader => reader.destroyed));
  first[1].complete('{"noise.off":"[late base]"}');
  assert.equal(ctx.text('noise.off', 'Source'), 'Off');
  assert.equal(ctx.effectiveLanguage(), 'en');
  second.complete('{"noise.off":"[RTL fixture]"}');
  assert.equal(ctx.effectiveLanguage(), 'ar');
  assert.equal(ctx._registry[ctx.effectiveLanguage()].direction, 'rtl');
  ctx.locale = 'fr-CA';
  ctx.reloadSelection();
  first[0].complete('{"noise.off":"[late old region]"}');
  second.complete('{"noise.off":"[late RTL]"}');
  assert.equal(ctx.text('noise.off', 'Source'), 'Off');
  ctx._readers[1].complete('{"noise.off":"[new base]"}');
  assert.equal(ctx.text('noise.off', 'Source'), '[new base]');
  const beforeIndex = ctx._readers[1];
  ctx._registry = ctx.parseRegistry(index);
  ctx.reloadSelection();
  beforeIndex.complete('{"noise.off":"[removed registry entry]"}');
  assert.equal(ctx.text('noise.off', 'Source'), 'Off');
});

test('interpolation is literal, own-property-only, single-pass with brace escapes', () => {
  const ctx = context();
  const values = Object.assign(Object.create({ inherited: 'No' }), {
    name: '$& $1 $$ {other}', zero: 0, flag: false, nil: null, obj: {},
    html: '<b>literal</b>',
  });
  assert.equal(ctx.text('missing', '{name}/{name}', values), '$& $1 $$ {other}/$& $1 $$ {other}');
  assert.equal(ctx.text('missing', '{zero}:{flag}:{nil}:{obj}:{missing}:{inherited}', values),
    '0:false:{nil}:{obj}:{missing}:{inherited}');
  assert.equal(ctx.text('missing', '{{name}} / {{{zero}}} / stray { / }', values),
    '{name} / {0} / stray { / }');
  assert.equal(ctx.text('missing', '{html}', values), '<b>literal</b>',
    'Plain text is the consumer contract, not HTML escaping');
  assert.equal(ctx.text('missing', '{missing}', null), '{missing}');
  assert.equal(ctx.text('missing', '{name}', { name: '' }), '');
});

test('QML async wiring uses fixed fallback readers and immutable per-selection requests', () => {
  assert.match(source, /pragma ComponentBehavior: Bound/);
  assert.match(source, /property string language: "system"/);
  assert.match(source, /resolveLocale\(language, Qt\.locale\(\)\.name\)/);
  assert.match(source, /onLocaleChanged: reloadSelection\(\)/);
  assert.match(source, /Component\.onCompleted: \{\s*_ready = true\s*reloadSelection\(\)/);
  assert.match(source, /rightToLeft: \{\s*var entry = _registry\[effectiveLocale\]\s*return entry !== undefined && entry\.direction === "rtl"/);
  assert.match(source, /path: root\.localPath\("index.json"\)/);
  assert.match(source, /path: root\.localPath\("en.json"\)/);
  assert.match(source, /onLoaded: root\._english = root\.parseCatalog\(text\(\)\)/);
  assert.match(source, /required property var request/);
  assert.match(source, /Component\.onCompleted: path = root\.localPath\(request\.file\)/);
  assert.match(source, /onLoaded: root\.acceptCatalog\(reader\.request, reader\.text\(\)\)/);
  assert.match(source, /onLoadFailed: root\.acceptCatalog\(reader\.request, ""\)/);
  assert.equal((source.match(/blockLoading: false/g) || []).length, 3);
  assert.equal((source.match(/blockAllReads: false/g) || []).length, 3);
  assert.doesNotMatch(source, /\b(?:Process|Socket|XMLHttpRequest|eval)\b|\.setText\(|\.writeAdapter\(/);
  const ctx = context();
  register(ctx);
  ctx.locale = 'fr';
  ctx._ready = false;
  ctx.reloadSelection();
  assert.equal(ctx._readers.length, 0);
  ctx._ready = true;
  ctx._catalogReader.createObject = () => null;
  ctx.reloadSelection();
  assert.equal(ctx._readers.length, 0);
  assert.equal(ctx.text('noise.off', 'Off'), 'Off');
});

test('shipped registry and catalogs are ordinary files with valid keys and placeholders', () => {
  const ctx = context();
  const registry = JSON.parse(index);
  assert.deepEqual(registry.en, { file: 'en.json', name: 'English', direction: 'ltr' });
  assert.deepEqual(registry, JSON.parse(JSON.stringify(ctx.parseRegistry(index))));
  for (const relative of ['I18n.qml', 'locales', 'tests/i18n', 'locales/README.md',
    'locales/index.json', 'tests/i18n/run.js', ...Object.values(registry).map(entry => `locales/${entry.file}`)]) {
    assert.ok(!fs.lstatSync(path.join(repo, relative)).isSymbolicLink(), relative);
  }
  const data = JSON.parse(english);
  assert.deepEqual(Object.keys(ctx.parseCatalog(english)).sort(), Object.keys(data).sort());
  assert.ok(Object.keys(data).length >= 50, 'English baseline covers main labels/status/controls');
  assert.equal(data['microphone.unknown'], 'Microphone mute: unknown');
  assert.equal(data['microphone.callGesture'], 'Call mode requested. Follow the headset voice prompt; tap behavior is not confirmed.');
  assert.equal(data['microphone.mediaGesture'], 'Call mode not requested. A tap may control playback.');
  for (const value of Object.values(data)) assert.doesNotMatch(value, /<\/?[A-Za-z][^>]*>/);
  const placeholders = value => [...new Set([...value.matchAll(/\{\{|\}\}|\{([A-Za-z_][A-Za-z0-9_]*)\}/g)]
    .map(match => match[1]).filter(Boolean))].sort();
  for (const entry of Object.values(registry)) {
    const raw = fs.readFileSync(path.join(repo, 'locales', entry.file), 'utf8');
    const catalog = JSON.parse(raw);
    assert.deepEqual(Object.keys(ctx.parseCatalog(raw)).sort(), Object.keys(catalog).sort());
    for (const [key, value] of Object.entries(catalog)) {
      assert.ok(Object.hasOwn(data, key), `Unrecognized catalog key: ${key}`);
      assert.deepEqual(placeholders(value), placeholders(data[key]), `${entry.file}: ${key}`);
    }
  }
});

test('active widget uses the English catalog with matching keys and complete source fallbacks', () => {
  const data = JSON.parse(english);
  const calls = [...widget.matchAll(/(?:root|toggleRow\.panelRoot)\.tr\(("(?:\\.|[^"\\])*"),\s*("(?:\\.|[^"\\])*")/g)];
  assert.equal(calls.length, (widget.match(/(?:root|toggleRow\.panelRoot)\.tr\(/g) || []).length,
    'Every translation call must use a literal key and complete English fallback');
  for (const [, key, fallback] of calls)
    assert.equal(data[JSON.parse(key)], JSON.parse(fallback), JSON.parse(key));
  assert.deepEqual([...new Set(calls.map(match => JSON.parse(match[1])))].sort(), Object.keys(data).sort(),
    'No missing keys or unused catalog scaffolding');
  assert.deepEqual(Object.keys(JSON.parse(index)), ['en', 'ru', 'de', 'fr', 'es', 'it', 'pt', 'zh', 'ja', 'ko']);
  assert.match(widget, /I18n \{\s*language: root\.preference\("locale", "system"\)/);
  assert.match(widget, /function tr\(key, fallback, params\) \{\s*return i18n\.text\(key, fallback, params\)/);
  assert.equal(manifest.barWidget.defaults.locale, 'system');
  const field = manifest.barWidget.schema.find(field => field.key === 'locale');
  assert.equal(field.type, 'string');
  assert.equal(field.defaultValue, 'system');
  assert.doesNotMatch(widget.replace(/"(?:\\.|[^"\\])*"/g, '""'),
    /pendingMode\.toUpperCase\(\)|text:.*\+|tooltipText:.*\+/);
});

test('visible bindings and models have no untranslated literals; direct Text is plain', () => {
  const lines = widget.split('\n');
  const allowed = new Set(['', 'off', 'anc', 'ambient', 'cycle', 'static', 'breathing', 'strobing',
    'lightingRed', 'lightingGreen', 'lightingBlue', 'sent', 'cetra-left', 'cetra-right', 'cetra-case', 'english', 'chinese', 'sound']);
  for (let i = 0; i < lines.length; i++) {
    const match = lines[i].match(/^(\s*)(?:\{ )?(?:text|title|meta|tooltipText|label|Accessible\.(?:name|description)):\s*(.*)/);
    if (!match) continue;
    let expression = match[2];
    for (let j = i + 1; j < lines.length && lines[j].search(/\S/) > match[1].length; j++)
      expression += '\n' + lines[j];
    expression = expression.replace(/(?:root|toggleRow\.panelRoot)\.tr\("(?:\\.|[^"\\])*",\s*"(?:\\.|[^"\\])*"/g, 'root.tr(key, fallback');
    for (const literal of expression.matchAll(/"(?:\\.|[^"\\])*"/g))
      assert.ok(allowed.has(JSON.parse(literal[0])), `Untranslated visible literal at line ${i + 1}: ${literal[0]}`);
  }
  const texts = [...widget.matchAll(/\bText \{\n/g)];
  assert.ok(texts.length > 0);
  assert.equal(texts.length, (widget.match(/\bText \{\n\s*textFormat: Text\.PlainText/g) || []).length);
  assert.match(widget, /PanelKeyCatcher \{\s*id: keyCatcher\s*LayoutMirroring\.enabled: root\.i18n\.rightToLeft\s*LayoutMirroring\.childrenInherit: true/);
  assert.doesNotMatch(widget, /#[0-9a-f]{6,8}/i);
});

test('compact copy preserves unknown mute, shared battery history and conditional lighting help', () => {
  const data = JSON.parse(english);
  assert.equal(data['status.presenceConfirmed'], 'Connected');
  assert.equal(data['battery.lastReported'], 'Battery values are last reported.');
  assert.equal(data['lighting.useThemeColor'], 'Match desktop theme');
  assert.match(widget, /Accessible.name: root\.tr\("microphone\.unknown", "Microphone mute: unknown"\)/);
  assert.match(widget, /text: root\.tr\("microphone\.unknown", "Microphone mute: unknown"\)/);
  assert.match(widget, /visible: root\.lighting !== "unknown"\s+text: root\.tr\("lighting\.lastSent"/);
  assert.match(widget, /contentWidth: panel\.fittedContentWidth\(Style\.space\(380\)\)/);
  const help = widget.match(/^\s*text: (root\.selectedLightingColor === null[\s\S]*?)^\s*color: root\.dim/m)[1];
  const ctx = vm.createContext({ root: {
    selectedLightingColor: {}, useThemeColor: true, tr: (key, fallback) => fallback,
  } });
  assert.equal(vm.runInContext(help, ctx), 'Turn off Match desktop theme to choose RGB.');
  ctx.root.useThemeColor = false;
  assert.equal(vm.runInContext(help, ctx), 'Choose a color, then apply it.');
  ctx.root.selectedLightingColor = null;
  assert.equal(vm.runInContext(help, ctx), 'Invalid RGB settings. Choose integer channels from 0 to 255.');
});

test('production widget functions consume catalogs, interpolate reports and preserve command enums', () => {
  const i18n = context();
  i18n._english = i18n.parseCatalog(english);
  const sent = [];
  const ctx = vm.createContext({ i18n, service: {
    setListeningMode: mode => sent.push(['mode', mode]),
    setVoicePrompt: voice => sent.push(['voice', voice]),
    setLighting: effect => sent.push(['lighting', effect]),
  } });
  ctx.root = ctx;
  for (const match of widget.matchAll(/^  function \w+\([^)]*\) \{[\s\S]*?^  \}/gm))
    vm.runInContext(match[0], ctx);
  for (const value of [null, undefined]) assert.equal(ctx.levelText(value), 'No data');
  for (const value of [0, 42, 100]) assert.equal(ctx.levelText(value), `${value}%`);
  for (const [value, label] of [[null, 'Unknown'], [true, 'On'], [false, 'Off']])
    assert.equal(ctx.settingStateText(value), label);
  for (const [key, label] of [['', ''], ['settings.pending', 'Waiting for setting readback...'],
    ['settings.notConfirmed', 'Setting change not confirmed. Try again.']]) {
    ctx.settingsStatusKey = key;
    assert.equal(ctx.settingsFeedback(), label);
  }
  assert.equal(ctx.reportText(false, true, false), 'Unavailable\nCharging reported');
  assert.equal(ctx.reportText(false, true, false, true), 'Unavailable / Charging reported');
  assert.equal(ctx.reportText(null, false, true), 'Not charging');
  for (const present of [true, false, null, undefined]) {
    assert.equal(ctx.batteryStatusText(present, true, false), 'Charging');
    for (const charging of [false, null, undefined])
      assert.equal(ctx.batteryStatusText(present, charging, false),
        present === true ? '' : present === false ? 'Unavailable' : 'No live status');
    for (const [charging, label] of [[true, 'Charging'], [false, 'Not charging'], [null, ''], [undefined, '']])
      assert.equal(ctx.batteryStatusText(present, charging, true), label);
  }
  for (const [mode, label] of [['off', 'Off'], ['anc', 'ANC'], ['ambient', 'Ambient'], ['constructor', 'Unknown']])
    assert.equal(ctx.modeText(mode), label);
  for (const [effect, label] of [['off', 'Off'], ['cycle', 'Color Cycle'], ['static', 'Static'],
    ['breathing', 'Breathing'], ['strobing', 'Strobing'], ['__proto__', 'Unknown']])
    assert.equal(ctx.lightingText(effect), label);

  // Sentinel text is a fixture, not a shipped translation. Exercise actual widget expressions.
  i18n._english = { ...i18n._english, 'noise.ambient': '[mode]', 'lighting.static': '[effect]',
    'lighting.channelValue': '{value} = {channel}', 'battery.percentage': '{value} pct',
    'report.chargingCompact': '[charging]', 'report.noLiveStatus': '[no live status]',
    'report.unavailable': '[unavailable]', 'report.notCharging': '[not charging]' };
  assert.equal(ctx.batteryStatusText(true, true, false), '[charging]');
  assert.equal(ctx.batteryStatusText(null, null, false), '[no live status]');
  assert.equal(ctx.batteryStatusText(false, null, false), '[unavailable]');
  assert.equal(ctx.batteryStatusText(null, false, true), '[not charging]');
  i18n._english['report.unavailable'] = 'Unavailable';
  i18n._english['report.notCharging'] = 'Not charging';
  assert.equal(ctx.modeText('ambient'), '[mode]');
  assert.equal(ctx.lightingText('static'), '[effect]');
  assert.equal(ctx.levelText(42), '42 pct');
  ctx.lighting = 'static';
  ctx.lightingRgb = [17, 34, 51];
  ctx.index = 1;
  ctx.modelData = { label: 'Green' };
  const lastSent = widget.match(/^\s*text: (root\.tr\("lighting\.lastSent".*)$/m)[1];
  assert.equal(vm.runInContext(lastSent, ctx), 'Last sent: [effect]');
  const channelValue = widget.match(/^\s*text: (root\.tr\("lighting\.channelValue".*)$/m)[1];
  assert.equal(vm.runInContext(channelValue, ctx), '34 = Green');
  const tooltip = widget.match(/^    tooltipText: ([\s\S]*?)^    \}\)/m)[1] + '})';
  Object.assign(ctx, { statusLabel: 'Status', leftLevel: 0, rightLevel: 42, caseLevel: null,
    leftPresent: false, rightPresent: true, leftCharging: true, rightCharging: false, caseCharging: null });
  assert.equal(vm.runInContext(tooltip, ctx), 'ROG Cetra SpeedNova\nStatus\nLast reported: L 0 pct / R 42 pct / Case No data\nLeft: Unavailable / Charging reported\nRight: Present / Not charging\nCase: Charging state unknown\nMic state: unknown / follow headset voice prompt');
  ctx.setListeningMode('ambient');
  const voiceValue = widget.match(/label: root\.tr\("voice\.english", "English"\),\s*value: "([^"]+)"/)[1];
  assert.equal(voiceValue, 'english', 'The UI locale en must not replace the headset protocol enum');
  ctx.setVoicePrompt(voiceValue);
  ctx.setVoicePrompt('chinese');
  ctx.setVoicePrompt('sound');
  ctx.setLighting('static');
  assert.deepEqual(sent, [['mode', 'ambient'], ['voice', 'english'], ['voice', 'chinese'], ['voice', 'sound'], ['lighting', 'static']]);
});
