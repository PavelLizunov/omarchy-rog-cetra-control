// Guard the actual module graph; a moved file must remain built and tested.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const {read, widgetFiles, serviceFiles} = require('./qml-source.js');
const root = path.resolve(__dirname, '..');
const entry = read('Cetra.qml');
assert.match(entry, /CetraViewModel\s*\{/);
assert.match(entry, /i18n: I18n/);
assert.match(read('CetraService.qml'), /CetraPreferences\s*\{/);
assert.match(read('CetraService.qml'), /CallDetector\s*\{.*root: serviceHost/);
for (const name of ['LanguageSection', 'BatterySection', 'NoiseSection', 'MicrophoneSection', 'LightingSection', 'VoiceSection']) {
  assert.ok(widgetFiles.includes(name + '.qml'));
  assert.match(entry, new RegExp(name + ' \\{[^}]*root: panelHost'));
  assert.match(read(name + '.qml'), /required property var root/);
}
assert.match(read('LightingSection.qml'), /LightingPalette\s*\{[\s\S]*root: section\.root/);
for (const name of widgetFiles) {
  const source = read(name);
  if (name !== 'ControlButton.qml' && name !== 'SettingToggle.qml') {
    for (const match of source.matchAll(/(?:ControlButton|SettingToggle)\s*\{([^{}]*)/g))
      assert.match(match[1], /panelRoot:/, `${name}: control must have explicit keyboard/theme owner`);
  }
}
const expected = new Set([...widgetFiles, ...serviceFiles, 'I18n.qml', 'CetraIcon.qml']);
assert.deepEqual(fs.readdirSync(root).filter(n => n.endsWith('.qml')).sort(), [...expected].sort(),
  'All production QML modules must have an explicit test/source owner');
const native = read('cetra-watch.c');
const included = [...native.matchAll(/^#include "daemon\/([a-z]+\.h)"/gm)].map(m => m[1]);
assert.deepEqual(included.sort(), fs.readdirSync(path.join(root, 'daemon')).filter(n => n.endsWith('.h')).sort());
assert.equal((native.match(/\bint main\(/g) || []).length, 1);
console.log('PASS module graph: explicit view dependencies, all QML owners and private C includes');
