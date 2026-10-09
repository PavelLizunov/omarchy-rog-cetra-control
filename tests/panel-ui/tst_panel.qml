import QtQuick
import QtTest
import qs.Commons
import "plugin" as Plugin
TestCase {
 id: test
 name: "CetraPanelRegression"
 when: windowShown
 visible: true
 width: 640; height: 860
 QtObject {
  id: svc
  property var settings: ({locale:"ru",showMicLevel:true,useThemeColor:true,autoThemeColor:false})
  property bool connected: true
  property bool receiver: true
  property string deviceStatus: "ok"
  property var leftLevel: 73
  property var rightLevel: 62
  property var caseLevel: 85
  property var leftPresent: true
  property var rightPresent: true
  property var leftCharging: false
  property var rightCharging: false
  property var caseCharging: false
  property bool presenceObserved: true
  property string listeningMode: "anc"
  property string pendingMode: ""
  property bool modeRequestTimedOut: false
  property var ancLevel: 2
  property var ancAdaptive: false
  property string voicePrompt: "english"
  property var pendingSettings: ({})
  property var timedOutSettings: ({})
  property bool settingsRequestTimedOut: false
  property string settingsStatusKey: ""
  property string lighting: "static"
  property bool callContextActive: false
  property var bluetoothAudioConnected: false
  property string bluetoothAvailability: "ready"
  property var bluetoothCandidates: []
  property string audioIdentity: "missing"
  property var bluetoothBattery: null
  property var audioStatus: ({output:"usb",capture:"usb",profile:"unknown",sink:true,microphone:true})
  property var bluetoothTelemetry: null
  property string bluetoothTelemetryState: "idle"
  property bool bluetoothTelemetryBusy: false
  property bool bluetoothTelemetryCanRefresh: true
  property string microphoneCaptureState: "active"
  property var microphoneLevel: 0.35
  property bool rejectSave: false
  property bool rejectSend: false
  property int writes: 0
  property int sends: 0
  function setBluetoothPanelOpen(view,open) {}
  function updateSetting(name,value,fallback) { if(rejectSave)return false; settings=Object.assign({},settings,{[name]:value});writes++;return true }
  function updateLightingColor(rgb,fallback) { if(rejectSave)return false;settings=Object.assign({},settings,{lightingRed:rgb[0],lightingGreen:rgb[1],lightingBlue:rgb[2],useThemeColor:false});writes++;return true }
  function setLighting(effect,color) {if(rejectSend)return false;sends++;return true}
  function setListeningMode(mode) {pendingMode=mode;sends++}
  function setAncLevel(level) {pendingSettings={anc_level:{desired:level}};settingsStatusKey="settings.pending";sends++}
  function setAncAdaptive(value) {pendingSettings={anc_adaptive:{desired:value}};settingsStatusKey="settings.pending";sends++}
  function setVoicePrompt(value) {pendingSettings={voice_prompt:{desired:value}};settingsStatusKey="settings.pending";sends++}
  function setAutoThemeColor(value) {return updateSetting("autoThemeColor",value,{})}
  function selectBluetoothAudio(address) {audioIdentity=address?"selected":"missing";return true}
  function refreshBluetoothTelemetry() {bluetoothTelemetryState="loading"}
 }
 QtObject {
  id: hostApi
  function serviceFor(id) {return svc}
  function updateEntryInline(id,entry) {if(svc.rejectSave)return false;svc.settings=entry;return true}
 }
 QtObject {
  id: bar
  property QtObject shell: hostApi
  property color foreground: Color.foreground
  property color barForeground: Color.foreground
  property color background: Color.background
  property color accent: Color.accent
  property color urgent: Color.urgent
  property string fontFamily: Style.font.family
 }
 Plugin.Cetra {id: panel;bar:bar}
 function obj(name) {var x=findChild(panel,name);verify(x!==null,name);return x}
 function controls() {var list=[];panel.collectControls(obj("column"),list);return list}
 function labelled(label) {var list=controls();for(var i=0;i<list.length;i++)if(list[i].label===label)return list[i];fail("No control: "+label)}
 function clickItem(item) {verify(item.visible&&item.enabled,"click "+item.objectName+" "+item.label+" visible="+item.visible+" enabled="+item.enabled);panel.focusControl(item);wait(30);mouseClick(item,item.width/2,item.height/2);wait(70)}
 function clickName(name) {clickItem(obj(name))}
 function verifyMessageVisible() {
  var message=obj("lightingMessage"), viewport=obj("viewport"), point=message.mapToItem(viewport,0,0)
  verify(message.visible && point.y>=-1 && point.y+message.height<=viewport.height+1,"error message scrolled into view")
  verify(obj("chooseColorButton").activeFocus,"Apply retains focus while error is revealed")
 }
 property var geometryReferences: ({})
 property var wheelSizes: ({})
 function checkGeometry(name) {
  var card=obj("panel"), nav=obj("pageTabs"), viewport=obj("viewport")
  var navPoint=nav.mapToItem(card,0,0), bodyPoint=viewport.mapToItem(card,0,0)
  var values={x:card.x,y:card.y,width:card.width,height:card.height,
   requestedWidth:card.preferredWidth,requestedHeight:card.preferredHeight,
   navX:navPoint.x,navY:navPoint.y,navWidth:nav.width,navHeight:nav.height,
   bodyX:bodyPoint.x,bodyY:bodyPoint.y,bodyWidth:viewport.width,bodyHeight:viewport.height}
  var group=JSON.stringify([card.screen,Style.fontBaseSize])
  var before=geometryReferences[group]
  if (before) for (var key in values) verify(Math.abs(values[key]-before[key])<=1,name+" stable "+key+": "+values[key]+" vs "+before[key])
  else geometryReferences[group]=values
  verify(viewport.height>0,"usable page viewport")
  verify(card.x>=0 && card.y>=0 && card.x+card.width<=card.screen.width && card.y+card.height<=card.screen.height,"fitted card bounds")
  if (panel.lightingColorExpanded) {
   var wheel=obj("wheel"), prior=wheelSizes[group]
   if (prior) {compare(wheel.width,prior[0]);compare(wheel.height,prior[1])}
   else wheelSizes[group]=[wheel.width,wheel.height]
  }
  console.log("GEOMETRY",name,JSON.stringify(values))
 }
 function save(name) {verify(panel.opened,"panel must be open for capture");mouseMove(test,width-2,height-2);waitForRendering(obj("panel"));wait(180);checkGeometry(name);grabImage(obj("panel")).save("__OUTPUT__/panel-"+name+".png");console.log("CAPTURE",name,obj("panel").width,obj("panel").height)}
 function test_walkthrough() {
  panel.open();wait(150);console.log("GRAPHICS API",panel.GraphicsInfo.api);compare(panel.GraphicsInfo.api,GraphicsInfo.OpenGL,"Panel regression requires actual OpenGL");compare(panel.i18n.effectiveLocale,"ru");save("sound-ru")
  var focusItems=controls();focusItems[0].forceActiveFocus()
  for(var k=1;k<focusItems.length;k++){keyClick(Qt.Key_Tab);tryCompare(focusItems[k],"activeFocus",true)}
  for(var k=focusItems.length-2;k>=0;k--){keyClick(Qt.Key_Backtab);tryCompare(focusItems[k],"activeFocus",true)}
  console.log("PASS real Tab/Backtab",focusItems.length,"Sound controls")
  clickItem(labelled("Высокий"));compare(svc.settingsStatusKey,"settings.pending");save("anc-pending-ru")
  svc.pendingSettings={};svc.timedOutSettings={anc_level:3};svc.settingsStatusKey="settings.notConfirmed";verify(obj("ancFeedback").visible);compare(panel.settingsFeedback(["voice_prompt"]),"");save("anc-timeout-ru")
  clickName("deviceTab");compare(panel.panelPage,"device");save("device-error-ru")
  svc.settingsStatusKey="";svc.timedOutSettings={};clickItem(labelled("Подробности  +"));save("device-details-ru")
  clickName("colorTab");compare(panel.devicePage,"color");save("color-ru")
  clickName("lightingPaletteToggle");verify(panel.lightingColorExpanded);tryCompare(obj("wheel"),"activeFocus",true);save("editor-ru")
  keyClick(Qt.Key_Right);keyClick(Qt.Key_Up);save("editor-keyboard-ru")
  clickItem(labelled("Точный цвет  +"));clickName("hexInput");keyClick(Qt.Key_A,Qt.ControlModifier);keyClick(Qt.Key_Z);save("invalid-hex-ru");verify(!obj("chooseColorButton").enabled)
  var sendsBefore=svc.sends;mouseClick(obj("chooseColorButton"),20,10);compare(svc.sends,sendsBefore);verify(!obj("chooseColorButton").selected);compare(obj("chooseColorButton").opacity,0.5);console.log("PASS disabled Apply does not send and has distinct styling")
  keyClick(Qt.Key_A,Qt.ControlModifier);keySequence("1");keySequence("2");keySequence("3");keySequence("4");keySequence("5");keySequence("6");verify(obj("chooseColorButton").enabled)
  svc.rejectSave=true;clickName("chooseColorButton");verify(panel.lightingDraftError);save("save-error-ru");verifyMessageVisible()
  svc.rejectSave=false;svc.rejectSend=true;clickName("chooseColorButton");compare(panel.lightingFeedback,"saved-not-sent");verify(!panel.lightingDraftError);save("send-error-ru");verifyMessageVisible()
  var saved=JSON.stringify(svc.settings);clickItem(labelled("Закрыть"));verify(!panel.lightingColorExpanded);compare(JSON.stringify(svc.settings),saved);save("cancel-after-error-ru")
  svc.rejectSend=false;clickName("lightingPaletteToggle");clickName("chooseColorButton");verify(!panel.lightingColorExpanded);compare(panel.lightingFeedback,"sent");save("applied-ru")
  panel.focusControl(obj("headerLanguageButton"));keyClick(Qt.Key_Return);wait(80);verify(panel.languageExpanded);save("languages-ru")
  svc.rejectSave=true;clickItem(labelled("Deutsch"));verify(panel.languageExpanded);verify(panel.languageSaveFailed);verify(obj("languageError").visible);compare(panel.i18n.effectiveLocale,"ru");save("language-save-rejected-ru")
  svc.rejectSave=false;clickItem(labelled("Deutsch"));compare(panel.i18n.effectiveLocale,"de");save("color-de")
  clickName("lightingPaletteToggle");save("editor-de");obj("wheel").forceActiveFocus();keyClick(Qt.Key_Escape);wait(30);verify(panel.opened);verify(!panel.lightingColorExpanded);tryCompare(obj("lightingPaletteToggle"),"activeFocus",true);save("wheel-escape-de")
  wait(80);clickName("lightingPaletteToggle");clickItem(labelled(panel.tr("lighting.exactColorExpand","Exact color  +")));clickName("hexInput");keyClick(Qt.Key_Escape);wait(40);verify(panel.opened);verify(!panel.lightingColorExpanded);save("hex-escape-de")
  for (var target of ["brightnessSlider", "chooseColorButton", "rgbSlider"]) {
    clickName("lightingPaletteToggle")
    if (target === "rgbSlider") clickItem(labelled(panel.tr("lighting.exactColorExpand","Exact color  +")))
    obj(target).forceActiveFocus();keyClick(Qt.Key_Escape);wait(40)
    verify(panel.opened, target+" keeps panel open");verify(!panel.lightingColorExpanded, target+" closes editor")
    tryCompare(obj("lightingPaletteToggle"),"activeFocus",true)
  }
  keyClick(Qt.Key_Escape);verify(!panel.opened);panel.open();wait(80)
  svc.updateSetting("locale","ru",{});clickName("soundTab");svc.bluetoothAudioConnected=true;svc.connected=false;svc.receiver=false;svc.leftPresent=null;svc.rightPresent=null;svc.deviceStatus="receiver-missing";svc.bluetoothTelemetryState="loading";save("bluetooth-loading-ru")
  svc.bluetoothTelemetryState="unavailable";save("bluetooth-error-ru")
  svc.bluetoothTelemetry={left:44,right:39,case:null,mode:"ambient"};svc.bluetoothTelemetryState="ready";save("bluetooth-ready-ru")
  clickName("deviceTab");clickName("colorTab");save("bluetooth-color-ru")
  svc.deviceStatus="permission-denied";svc.bluetoothAudioConnected=false;svc.bluetoothTelemetry=null;clickName("soundTab");save("permission-ru")
  svc.deviceStatus="starting";save("starting-ru")
  svc.updateSetting("hideWhenDisconnected",false,{});svc.deviceStatus="receiver-missing";save("disconnected-ru")
  svc.bluetoothCandidates=[{name:"ROG Cetra SpeedNova — очень длинное имя тестового устройства",address:"00:11:22:33:44:55"}];clickItem(labelled("Выбрать Bluetooth-наушники"));compare(panel.devicePage,"settings");verify(labelled(panel.bluetoothDeviceText(svc.bluetoothCandidates[0])).visible);console.log("PASS Bluetooth selection opens Settings after visiting Color");save("bluetooth-picker-ru")
  clickName("deviceSettingsTab");save("bluetooth-picker-visible-ru");clickItem(labelled(panel.bluetoothDeviceText(svc.bluetoothCandidates[0])));compare(svc.audioIdentity,"selected");svc.bluetoothCandidates=[]
  svc.connected=true;svc.receiver=true;svc.leftPresent=true;svc.rightPresent=true;svc.deviceStatus="ok";
  svc.updateSetting("locale","fi",{});clickName("soundTab");compare(panel.i18n.effectiveLocale,"en");compare(panel.displayLocaleCode,"EN");save("unsupported-locale-fi")
  for(var locale of ["en","de","fr","es","it","pt","zh","ja","ko"]) {svc.updateSetting("locale",locale,{});clickName("soundTab");save("sound-"+locale)}
  svc.updateSetting("locale","ru",{});clickName("deviceTab");clickName("colorTab");clickName("lightingPaletteToggle");clickItem(labelled("Точный цвет  +"));Style.fontBaseSize=18;save("editor-scale150-ru")
  panel.focusControl(obj("chooseColorButton"));wait(80);verify(obj("chooseColorButton").activeFocus);verify(obj("viewport").contentY>0);save("editor-scale150-bottom-ru");Style.fontBaseSize=12
  svc.connected=false;wait(80);verify(!panel.lightingColorExpanded);verify(obj("deviceTab").activeFocus);save("disconnect-editor-ru")
  svc.connected=true;svc.receiver=true;svc.deviceStatus="ok";svc.updateSetting("locale","de",{})
  obj("panel").screen={width:320,height:420};obj("panel").anchorWindow={width:320,height:24};wait(80)
  for (var scale of [12,18]) {
   Style.fontBaseSize=scale;wait(80)
   clickName("soundTab");save("small-sound-"+scale)
   clickName("deviceTab");clickName("deviceSettingsTab");save("small-settings-"+scale)
   clickName("colorTab");save("small-color-"+scale)
   clickName("lightingPaletteToggle");save("small-editor-"+scale)
   clickItem(labelled(panel.tr("lighting.exactColorExpand","Exact color  +")))
   panel.focusControl(obj("chooseColorButton"));wait(80)
   var pos=obj("chooseColorButton").mapToItem(obj("viewport"),0,0)
   verify(pos.y>=-1 && pos.y+obj("chooseColorButton").height<=obj("viewport").height+1,"Apply visible on small screen")
   save("small-exact-bottom-"+scale)
   var scroll=obj("viewport").contentY
   panel.focusControl(obj("deviceTab"));compare(obj("viewport").contentY,scroll,"fixed navigation does not scroll page")
   obj("chooseColorButton").forceActiveFocus();keyClick(Qt.Key_Escape);wait(80);verify(!panel.lightingColorExpanded)
   save("small-editor-closed-"+scale)
   clickName("soundTab");save("small-sound-return-"+scale)
  }
  panel.close();verify(!panel.opened)
 }
}
