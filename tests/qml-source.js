// Explicit production modules for offline tests; not a QML runtime simulation.
const fs = require('node:fs');
const path = require('node:path');
const repo = path.resolve(__dirname, '..');
const read = name => fs.readFileSync(path.join(repo, name), 'utf8');
const widgetFiles = ['CetraViewModel.qml', 'Cetra.qml', 'ControlButton.qml', 'SettingToggle.qml',
  'LanguageSection.qml', 'BatterySection.qml', 'NoiseSection.qml', 'MicrophoneSection.qml', 'LightingSection.qml', 'LightingPalette.qml', 'VoiceSection.qml'];
const serviceFiles = ['CetraPreferences.qml', 'CetraService.qml', 'CallDetector.qml'];
module.exports = { read, widgetFiles, serviceFiles,
  widget: widgetFiles.map(read).join('\n'), service: serviceFiles.map(read).join('\n') };
