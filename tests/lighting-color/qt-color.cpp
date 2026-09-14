// Exercise real QColor/variant argument passing without loading the plugin or a helper.
#include <QFile>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QRegularExpression>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQuickItem>
#include <iostream>
#include <memory>

static QString functions(const QString &file) {
  QFile source(file);
  if (!source.open(QIODevice::ReadOnly)) qFatal("Cannot open production source");
  const QString text = QString::fromUtf8(source.readAll());
  auto matches = QRegularExpression(R"(^  function \w+\([^)]*\) \{[\s\S]*?^  \})",
    QRegularExpression::MultilineOption).globalMatch(text);
  QString result;
  while (matches.hasNext()) result += matches.next().captured() + "\n";
  return result;
}

static QString extract(const QString &file, const QString &pattern) {
  QFile source(file);
  if (!source.open(QIODevice::ReadOnly)) qFatal("Cannot open production source");
  auto match = QRegularExpression(pattern, QRegularExpression::MultilineOption)
                 .match(QString::fromUtf8(source.readAll()));
  if (!match.hasMatch()) qFatal("Cannot extract production binding/function");
  return match.captured();
}

int main(int argc, char **argv) {
  QGuiApplication app(argc, argv);
  if (argc != 2) return 2;
  const QString dir = QString::fromLocal8Bit(argv[1]);
  const QString widget = dir + "/CetraViewModel.qml";
  const QString service = dir + "/CetraService.qml";
  QString qml = R"QML(import QtQuick
Item {
  id: root
  property color accent: Qt.rgba(0.2, 0.4, 0.6, 1)
  property bool useThemeColor: true
  property string lightingFeedback: ""
  property var lightingRgb: [17, 34, 51]
)QML";
  qml += extract(widget, R"(^  readonly property var selectedLightingColor: \{\n[\s\S]*?^  \})");
  qml += R"QML(
  property QtObject service: QtObject {
    property bool connected: true
    property string lighting: "unknown"
    property string sessionLightingEffect: ""
    property string lastLightingPayload: ""
    property QtObject themeColorDelay: QtObject { function stop() {} }
    property QtObject deviceWatchProc: QtObject {
      property bool running: true
      property var writes: []
      function write(text) { writes = writes.concat([text]) }
    }
)QML";
  qml += extract(service, R"(^  function setLighting\([^)]*\) \{[\s\S]*?^  \})");
  qml += "\n  }\n";
  qml += extract(widget, R"(^  function setLighting\([^)]*\) \{[\s\S]*?^  \})");
  qml += R"QML(
  function check(ok, reason) { if (!ok) throw new Error(reason) }
  function run() {
    check(typeof selectedLightingColor.r === "number", "QColor r is not numeric")
    check(typeof selectedLightingColor.g === "number", "QColor g is not numeric")
    check(typeof selectedLightingColor.b === "number", "QColor b is not numeric")
    for (var effect of ["static", "breathing", "strobing"]) {
      check(setLighting(effect) === true, "QColor rejected")
      check(service.deviceWatchProc.writes.slice(-1)[0] === "lighting " + effect + " 51 102 153\n", "QColor payload")
    }
    useThemeColor = false
    for (var channel = 0; channel <= 255; channel++) {
      lightingRgb = [channel, 255 - channel, channel]
      check(setLighting("static") === true, "Manual RGB rejected")
      check(service.deviceWatchProc.writes.slice(-1)[0] === "lighting static " + channel + " " + (255 - channel) + " " + channel + "\n", "Manual RGB payload")
    }
    lightingRgb = [null, 34, 51]
    check(setLighting("static") === false, "Invalid manual RGB accepted")
    useThemeColor = true
    accent = Qt.rgba(0, 0.5, 1, 1)
    check(setLighting("static") === true, "Changed QColor rejected")
    check(service.deviceWatchProc.writes.slice(-1)[0] === "lighting static 0 128 255\n", "Changed QColor payload")
    check(service.lighting === "unknown", "Optimistic hardware state")
    return true
  }
})QML";
  QQmlEngine engine;
  QQmlComponent component(&engine);
  component.setData(qml.toUtf8(), QUrl("file:///tmp/cetra-offline-color.qml"));
  std::unique_ptr<QObject> object(component.create());
  if (!object) { qCritical() << component.errors(); return 1; }
  QVariant result;
  if (!QMetaObject::invokeMethod(object.get(), "run", Q_RETURN_ARG(QVariant, result))
      || !result.toBool()) return 1;

  // Keep the host's derived-binding/signal ordering real: this reproduces a
  // lagging public snapshot in Qt, unlike the synchronous Node host fixtures.
  QString settingsQml = R"QML(import QtQuick
Item {
  id: test
  property var shellConfig: ({version: 1, bar: {layout: {right: [{id: "cetra", locale: "system", alwaysCallContext: false}]}}})
  readonly property var barConfig: shellConfig.bar
  property bool reject: false
  onShellConfigChanged: {
    api.barConfig = JSON.parse(JSON.stringify(barConfig))
    a.settings = barConfig.layout.right[0]
    b.settings = barConfig.layout.right[0]
  }
  QtObject {
    id: api
    property var barConfig: ({})
    function updateEntryInline(id, entry) {
      if (test.reject) return false
      test.shellConfig = {version: 1, bar: {layout: {right: [entry]}}}
      return true
    }
  }
  Item {
    id: owner
    property var shell: api
    property var manifest: ({id: "cetra"})
    property bool hostReady: true
    property var inlineSettings: null
    property var pendingPreferences: ({})
    property QtObject preferenceReadback: QtObject {
      function restart() {}
      function stop() {}
    }
)QML";
  for (const QString &name : {QString("settings"), QString("hostSettings")})
    settingsQml += extract(dir + "/CetraPreferences.qml", "^  readonly property var " + name + R"(: (?:\{\n[\s\S]*?^  \}|[^\n]+))") + "\n";
  settingsQml += functions(dir + "/CetraPreferences.qml") + functions(service) + "\n}\n";
  settingsQml += R"QML(
  component View: Item {
    id: root
    property var service: owner
    property var settings: ({})
    property string moduleName: "cetra"
    property var bar: ({shell: api})
)QML";
  settingsQml += functions(widget);
  settingsQml += R"QML(
  }
  View { id: a }
  View { id: b }
  function check(ok, reason) { if (!ok) throw new Error(reason) }
  function run() {
    api.barConfig = JSON.parse(JSON.stringify(barConfig))
    a.settings = barConfig.layout.right[0]
    b.settings = a.settings
    owner.applySavedConfig(JSON.stringify(shellConfig))
    a.setLocaleSetting("ru")
    check(api.barConfig.layout.right[0].locale === "system", "Must reproduce lagging host snapshot")
    b.setLightingSetting("useThemeColor", false)
    a.setLightingSetting("lightingRed", 17)
    check(barConfig.layout.right[0].locale === "ru", "Locale was rolled back")
    check(!b.preference("useThemeColor", true), "Color preference was rolled back")
    check(a.preference("lightingRed", 255) === 17, "RGB not shared")
    return true
  }
  function afterInjection() {
    owner.applySavedConfig(JSON.stringify({version: 1, bar: api.barConfig}))
    check(owner.settings.locale === "ru" && owner.settings.lightingRed === 17, "Deferred injection rolled back settings")
    owner.applySavedConfig(JSON.stringify(shellConfig))
    check(Object.keys(owner.pendingPreferences).length === 0, "Saved preferences not acknowledged")
    test.reject = true
    check(a.setLocaleSetting("de") === false, "Rejected write reported success")
    check(b.preference("locale", "system") === "ru", "Failed write changed selection")
    test.reject = false
    // An external host edit must supersede the plugin's shared preferences.
    shellConfig = {version: 1, bar: {layout: {right: [{id: "cetra", locale: "fr", alwaysCallContext: false}]}}}
    owner.applySavedConfig(JSON.stringify(shellConfig))
    return true
  }
  function afterExternalEdit() {
    check(a.preference("locale", "system") === "fr", "External locale not propagated")
    check(b.preference("useThemeColor", true), "External preference removal not propagated")
    check(a.preference("lightingRed", 255) === 255, "Removed preference survived external edit")
    return true
  }
  function selectLanguage(code) {
    a.setLocaleSetting(code)
    return true
  }
  function verifyLanguage(code) {
    check(a.preference("locale", "system") === code && b.preference("locale", "system") === code, "Language reverted after event delivery")
    owner.applySavedConfig(JSON.stringify({version: 1, bar: api.barConfig}))
    check(a.preference("locale", "system") === code, "Old disk snapshot reverted selection")
    owner.applySavedConfig(JSON.stringify(shellConfig))
    return true
  }
})QML";
  QQmlComponent settingsComponent(&engine);
  settingsComponent.setData(settingsQml.toUtf8(), QUrl("file:///tmp/cetra-offline-settings.qml"));
  std::unique_ptr<QObject> settingsObject(settingsComponent.create());
  if (!settingsObject) { qCritical() << settingsComponent.errors(); return 1; }
  for (const auto *step : {"run", "afterInjection", "afterExternalEdit"}) {
    if (!QMetaObject::invokeMethod(settingsObject.get(), step, Q_RETURN_ARG(QVariant, result)) || !result.toBool()) {
      std::cerr << "Settings check failed: " << step << '\n';
      return 1;
    }
    QCoreApplication::processEvents();
  }
  for (const auto *code : {"ru", "de", "en", "ja", "system", "ru"}) {
    const QVariant language = QString::fromLatin1(code);
    for (const auto *step : {"selectLanguage", "verifyLanguage"}) {
      if (!QMetaObject::invokeMethod(settingsObject.get(), step, Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, language)) || !result.toBool()) {
        std::cerr << "Language check failed: " << step << ' ' << code << '\n';
        return 1;
      }
      QCoreApplication::processEvents();
    }
  }

  // Exercise the production wrapping Text and height binding in real QtQuick.
  // Palette tokens are supplied locally; no Quickshell host or HID is started.
  QString labelQml = R"QML(import QtQuick
Item {
  id: controlButton
  property string label: ""
  property real horizontalPadding: 9
  property real verticalPadding: 5
  property bool selected: false
  property bool leftAlign: false
  property color foreground: "transparent"
  property color accent: "transparent"
  property string fontFamily: "monospace"
  property real fontSize: 9
  property QtObject style: QtObject {
    property int normalBorderWidth: 1
    function selectedStateColor(foreground, accent) { return foreground }
  }
)QML";
  QString height = extract(dir + "/ControlButton.qml", R"(^  implicitHeight: buttonLabel[^\n]+)");
  QString label = extract(dir + "/ControlButton.qml", R"(^  Text \{\n    textFormat: Text.PlainText\n    id: buttonLabel[\s\S]*?^  \})");
  // Only replace the environment-owned singleton reference, never text/geometry.
  labelQml += height.replace("Style.", "style.") + "\n";
  labelQml += label.replace("Style.", "style.") + "\n}";
  QQmlComponent labelComponent(&engine);
  labelComponent.setData(labelQml.toUtf8(), QUrl("file:///tmp/cetra-offline-label.qml"));
  std::unique_ptr<QObject> labelObject(labelComponent.create());
  if (!labelObject) { qCritical() << labelComponent.errors(); return 1; }
  auto *button = qobject_cast<QQuickItem *>(labelObject.get());
  auto *text = button->childItems().at(0);
  QFile indexFile(dir + "/locales/index.json");
  if (!indexFile.open(QIODevice::ReadOnly)) return 1;
  const auto registry = QJsonDocument::fromJson(indexFile.readAll()).object();
  int labelsChecked = 0;
  for (auto it = registry.begin(); it != registry.end(); ++it) {
    QFile file(dir + "/locales/" + it.value().toObject()["file"].toString());
    if (!file.open(QIODevice::ReadOnly)) return 1;
    const auto catalog = QJsonDocument::fromJson(file.readAll()).object();
    for (const auto *key : {"voice.english", "voice.chinese", "voice.beeps", "lighting.static",
                           "lighting.breathing", "lighting.strobing", "language.system"}) {
      for (const int width : {69, 102}) for (const int fontSize : {9, 14, 20}) {
        button->setWidth(width);
        button->setProperty("fontSize", fontSize);
        button->setProperty("label", catalog[key].toString());
        QCoreApplication::processEvents();
        if (text->property("contentWidth").toDouble() > text->width() + 1
            || button->height() < text->implicitHeight() + 10) {
          std::cerr << "Label overflow: " << it.key().toStdString() << ' ' << key << '\n';
          return 1;
        }
        labelsChecked++;
      }
    }
  }
  std::cout << "PASS Qt settings: lagging host, two views, rejected write, external edit; "
            << labelsChecked << " production label layouts\n";
  qInfo("PASS Qt QColor/variant: numeric channels, theme changes, all manual bytes and exact mock payloads (no HID)");
  return 0;
}
