#include "web_server.h"
#include "ble_keyboard.h"
#include "auto_mode.h"
#include "config_manager.h"

WebController::WebController(BleKeyboard* keyboard, AutoMode* autoMode, ConfigManager* configMgr)
  : _server(nullptr), _keyboard(keyboard), _autoMode(autoMode), _configMgr(configMgr) {
}

void WebController::begin() {
  _server = new WebServer(WEB_SERVER_PORT);
  _server->on("/", [this]() { handleRoot(); });
  _server->on("/api/press", HTTP_GET, [this]() { handleKeyPress(); });
  _server->on("/api/release", HTTP_GET, [this]() { handleKeyRelease(); });
  _server->on("/api/config", HTTP_ANY, [this]() { handleConfig(); });
  _server->on("/api/status", HTTP_GET, [this]() { handleStatus(); });
  _server->on("/api/events", HTTP_GET, [this]() { handleEvents(); });
  _server->on("/api/stats", HTTP_GET, [this]() { handleStats(); });
  _server->on("/api/ble/reboot", HTTP_POST, [this]() { handleBleReboot(); });
  // 配置槽位管理
  _server->on("/api/slots", HTTP_GET, [this]() { handleSlots(); });
  _server->on("/api/slot/save", HTTP_POST, [this]() { handleSlotSave(); });
  _server->on("/api/slot/load", HTTP_POST, [this]() { handleSlotLoad(); });
  _server->on("/api/slot/delete", HTTP_POST, [this]() { handleSlotDelete(); });
  _server->on("/api/slot/export", HTTP_POST, [this]() { handleSlotExport(); });
  _server->on("/api/slot/import", HTTP_POST, [this]() { handleSlotImport(); });
  _server->on("/api/config/export", HTTP_GET, [this]() { handleConfigExport(); });
  _server->on("/api/config/import", HTTP_POST, [this]() { handleConfigImport(); });
  _server->begin();
  Serial.print("[Web] 服务器已启动: http://");
  Serial.println(getLocalIP());
}

void WebController::handleClient() { _server->handleClient(); }
String WebController::getLocalIP() { return WiFi.localIP().toString(); }

void WebController::handleRoot() {
  _server->setContentLength(CONTENT_LENGTH_UNKNOWN);
  _server->send(200, "text/html", "");
  _server->sendContent("<!DOCTYPE html><html><head><meta charset='UTF-8'>");
  _server->sendContent("<meta name='viewport' content='width=device-width,initial-scale=1'>");
_server->sendContent("<title>ESP Virtual Keyboard</title>");
  _server->sendContent("<style>" + generateCSS() + "</style>");
  _server->sendContent("</head><body>" + generateHTML());
  _server->sendContent("<script>" + generateJS() + "</script>");
  _server->sendContent("</body></html>");
}

void WebController::handleKeyPress() {
  if (!_server->hasArg("key")) { _server->send(400, "application/json", "{\"error\":\"no key\"}"); return; }
  uint8_t k = mapWebKeyToHid(_server->arg("key"));
  if (k == 0xFF) { _server->send(400, "application/json", "{\"error\":\"unknown\"}"); return; }
  _keyboard->press(k);
  _server->send(200, "application/json", "{\"ok\":true}");
}

void WebController::handleKeyRelease() {
  if (!_server->hasArg("key")) { _server->send(400, "application/json", "{\"error\":\"no key\"}"); return; }
  uint8_t k = mapWebKeyToHid(_server->arg("key"));
  if (k == 0xFF) { _server->send(400, "application/json", "{\"error\":\"unknown\"}"); return; }
  _keyboard->release(k);
  _server->send(200, "application/json", "{\"ok\":true}");
}

void WebController::handleConfig() {
  if (_server->method() == HTTP_GET) {
    AutoModeConfig c = _autoMode->getConfig();
    String j = "{\"enabled\":" + String(c.enabled?"true":"false");
    j += ",\"minInterval\":" + String(c.minIntervalMs);
    j += ",\"maxInterval\":" + String(c.maxIntervalMs);
    j += ",\"minHold\":" + String(c.minHoldMs);
    j += ",\"maxHold\":" + String(c.maxHoldMs);
    j += ",\"weightW\":" + String(c.moveForwardWeight,2);
    j += ",\"weightS\":" + String(c.moveBackWeight,2);
    j += ",\"weightA\":" + String(c.moveLeftWeight,2);
    j += ",\"weightD\":" + String(c.moveRightWeight,2);
    j += ",\"weightTL\":" + String(c.turnLeftWeight,2);
    j += ",\"weightTR\":" + String(c.turnRightWeight,2);
    j += ",\"weightSP\":" + String(c.jumpWeight,2);
    j += ",\"weightC\":" + String(c.weightC,2);
    j += ",\"weightZ\":" + String(c.weightZ,2);
    j += ",\"weightIdle\":" + String(c.idleWeight,2) + "}";
    _server->send(200, "application/json", j);
  } else {
    AutoModeConfig c = _autoMode->getConfig();
    if (_server->hasArg("enabled")) c.enabled = _server->arg("enabled")=="true";
    if (_server->hasArg("minInterval")) c.minIntervalMs = _server->arg("minInterval").toInt();
    if (_server->hasArg("maxInterval")) c.maxIntervalMs = _server->arg("maxInterval").toInt();
    if (_server->hasArg("minHold")) c.minHoldMs = _server->arg("minHold").toInt();
    if (_server->hasArg("maxHold")) c.maxHoldMs = _server->arg("maxHold").toInt();
    if (_server->hasArg("weightW")) c.moveForwardWeight = _server->arg("weightW").toFloat();
    if (_server->hasArg("weightS")) c.moveBackWeight = _server->arg("weightS").toFloat();
    if (_server->hasArg("weightA")) c.moveLeftWeight = _server->arg("weightA").toFloat();
    if (_server->hasArg("weightD")) c.moveRightWeight = _server->arg("weightD").toFloat();
    if (_server->hasArg("weightTL")) c.turnLeftWeight = _server->arg("weightTL").toFloat();
    if (_server->hasArg("weightTR")) c.turnRightWeight = _server->arg("weightTR").toFloat();
    if (_server->hasArg("weightSP")) c.jumpWeight = _server->arg("weightSP").toFloat();
    if (_server->hasArg("weightC")) c.weightC = _server->arg("weightC").toFloat();
    if (_server->hasArg("weightZ")) c.weightZ = _server->arg("weightZ").toFloat();
    if (_server->hasArg("weightIdle")) c.idleWeight = _server->arg("weightIdle").toFloat();
    _autoMode->setConfig(c);
    _server->send(200, "application/json", "{\"ok\":true}");
  }
}

void WebController::handleStatus() {
  BleState s = _keyboard->getState();
  String ss = s==BLE_STATE_CONNECTED?"connected":s==BLE_STATE_ADVERTISING?"advertising":"stopped";
  String ck = _autoMode->getCurrentKeyName();
  time_t now_t;
  time(&now_t);
  _server->send(200, "application/json",
    "{\"bleState\":\""+ss+"\",\"bleConnected\":"+String(_keyboard->isConnected()?"true":"false")+
    ",\"autoMode\":"+String(_autoMode->isEnabled()?"true":"false")+
    ",\"currentKey\":\""+ck+"\""+
    ",\"ip\":\""+getLocalIP()+"\",\"uptime\":"+String(millis()/1000)+
    ",\"epoch\":" + String((uint32_t)now_t) + "}");
}

void WebController::handleBleReboot() {
  _keyboard->disconnectAndReboot();
  _server->send(200, "application/json", "{\"ok\":true,\"msg\":\"已断开并重新广播\"}");
}

void WebController::handleStats() {
  String keys[10] = {"W","S","A","D","TL","TR","SP","C","Z","Idle"};
  uint32_t total = _autoMode->getTotalCount();
  String j = "{\"total\":" + String(total);
  j += ",\"keys\":{";
  for (int i = 0; i < 10; i++) {
    if (i > 0) j += ",";
    j += "\"" + keys[i] + "\":" + String(_autoMode->getKeyCount(i));
  }
  j += "}}";
  _server->send(200, "application/json", j);
}

void WebController::handleEvents() {
  uint32_t since = 0;
  if (_server->hasArg("since")) {
    since = _server->arg("since").toInt();
  }
  
  String j = "{\"events\":[";
  int count = _autoMode->getEventCount();
  bool first = true;
  for (int i = 0; i < count; i++) {
    KeyEvent ev = _autoMode->getEvent(i);
    if (ev.eventId > since) {
      if (!first) j += ",";
      j += "{\"id\":" + String(ev.eventId);
      j += ",\"key\":\"" + ev.keyName + "\"";
      j += ",\"down\":" + String(ev.pressed ? "true" : "false");
      j += ",\"t\":" + String(ev.timestamp);
      j += "}";
      first = false;
    }
  }
  j += "],\"lastId\":" + String(_autoMode->getLastEventId()) + "}";
  _server->send(200, "application/json", j);
}

uint8_t WebController::mapWebKeyToHid(const String& key) {
  if (key=="a")return HID_KEY_A; if (key=="b")return HID_KEY_B; if (key=="c")return HID_KEY_C;
  if (key=="d")return HID_KEY_D; if (key=="e")return HID_KEY_E; if (key=="f")return HID_KEY_F;
  if (key=="g")return HID_KEY_G; if (key=="h")return HID_KEY_H; if (key=="i")return HID_KEY_I;
  if (key=="j")return HID_KEY_J; if (key=="k")return HID_KEY_K; if (key=="l")return HID_KEY_L;
  if (key=="m")return HID_KEY_M; if (key=="n")return HID_KEY_N; if (key=="o")return HID_KEY_O;
  if (key=="p")return HID_KEY_P; if (key=="q")return HID_KEY_Q; if (key=="r")return HID_KEY_R;
  if (key=="s")return HID_KEY_S; if (key=="t")return HID_KEY_T; if (key=="u")return HID_KEY_U;
  if (key=="v")return HID_KEY_V; if (key=="w")return HID_KEY_W; if (key=="x")return HID_KEY_X;
  if (key=="y")return HID_KEY_Y; if (key=="z")return HID_KEY_Z;
  if (key=="1")return HID_KEY_1; if (key=="2")return HID_KEY_2; if (key=="3")return HID_KEY_3;
  if (key=="4")return HID_KEY_4; if (key=="5")return HID_KEY_5; if (key=="6")return HID_KEY_6;
  if (key=="7")return HID_KEY_7; if (key=="8")return HID_KEY_8; if (key=="9")return HID_KEY_9;
  if (key=="0")return HID_KEY_0;
  if (key=="enter")return HID_KEY_ENTER; if (key=="esc")return HID_KEY_ESC;
  if (key=="backspace")return HID_KEY_BACKSPACE; if (key=="tab")return HID_KEY_TAB;
  if (key=="space")return HID_KEY_SPACE; if (key=="minus")return HID_KEY_MINUS;
  if (key=="equal")return HID_KEY_EQUAL; if (key=="lbracket")return HID_KEY_LBRACKET;
  if (key=="rbracket")return HID_KEY_RBRACKET; if (key=="backslash")return HID_KEY_BACKSLASH;
  if (key=="semicolon")return HID_KEY_SEMICOLON; if (key=="apostrophe")return HID_KEY_APOSTROPHE;
  if (key=="grave")return HID_KEY_GRAVE; if (key=="comma")return HID_KEY_COMMA;
  if (key=="period")return HID_KEY_PERIOD; if (key=="slash")return HID_KEY_SLASH;
  if (key=="capslock")return HID_KEY_CAPSLOCK;
  if (key=="f1")return HID_KEY_F1; if (key=="f2")return HID_KEY_F2; if (key=="f3")return HID_KEY_F3;
  if (key=="f4")return HID_KEY_F4; if (key=="f5")return HID_KEY_F5; if (key=="f6")return HID_KEY_F6;
  if (key=="f7")return HID_KEY_F7; if (key=="f8")return HID_KEY_F8; if (key=="f9")return HID_KEY_F9;
  if (key=="f10")return HID_KEY_F10; if (key=="f11")return HID_KEY_F11; if (key=="f12")return HID_KEY_F12;
  if (key=="up")return HID_KEY_UP_ARROW; if (key=="down")return HID_KEY_DOWN_ARROW;
  if (key=="left")return HID_KEY_LEFT_ARROW; if (key=="right")return HID_KEY_RIGHT_ARROW;
  if (key=="insert")return HID_KEY_INSERT; if (key=="home")return HID_KEY_HOME;
  if (key=="pageup")return HID_KEY_PAGEUP; if (key=="delete")return HID_KEY_DELETE;
  if (key=="end")return HID_KEY_END; if (key=="pagedown")return HID_KEY_PAGEDOWN;
  // 小键盘
  if (key=="numpad0")return HID_KEY_NUMPAD_0; if (key=="numpad1")return HID_KEY_NUMPAD_1;
  if (key=="numpad2")return HID_KEY_NUMPAD_2; if (key=="numpad3")return HID_KEY_NUMPAD_3;
  if (key=="numpad4")return HID_KEY_NUMPAD_4; if (key=="numpad5")return HID_KEY_NUMPAD_5;
  if (key=="numpad6")return HID_KEY_NUMPAD_6; if (key=="numpad7")return HID_KEY_NUMPAD_7;
  if (key=="numpad8")return HID_KEY_NUMPAD_8; if (key=="numpad9")return HID_KEY_NUMPAD_9;
  if (key=="numpadadd")return HID_KEY_NUMPAD_ADD; if (key=="numpadsub")return HID_KEY_NUMPAD_SUB;
  if (key=="numpadmul")return HID_KEY_NUMPAD_MUL; if (key=="numpaddiv")return HID_KEY_NUMPAD_DIV;
  if (key=="numpaddot")return HID_KEY_NUMPAD_DOT; if (key=="numpadenter")return HID_KEY_NUMPAD_ENTER;
  if (key=="numlock")return HID_KEY_NUMLOCK;
  return 0xFF;
}

// ========== 配置槽位管理 API ==========

void WebController::handleSlots() {
  if (!_configMgr) { _server->send(500, "application/json", "{\"error\":\"config manager not available\"}"); return; }
  String j = "{\"slots\":[";
  for (int i = 0; i < SLOT_COUNT; i++) {
    if (i > 0) j += ",";
    SlotSummary s = _configMgr->getSlotSummary(i);
    j += "{\"index\":" + String(i);
    j += ",\"used\":" + String(s.used ? "true" : "false");
    j += ",\"name\":\"" + s.name + "\"}";
  }
  j += "],\"active\":" + String(_configMgr->getActiveSlot()) + "}";
  _server->send(200, "application/json", j);
}

void WebController::handleSlotSave() {
  if (!_configMgr) { _server->send(500, "application/json", "{\"error\":\"config manager not available\"}"); return; }
  if (!_server->hasArg("slot") || !_server->hasArg("name")) {
    _server->send(400, "application/json", "{\"error\":\"missing slot or name\"}");
    return;
  }
  int slot = _server->arg("slot").toInt();
  String name = _server->arg("name");
  if (slot < 0 || slot >= SLOT_COUNT) {
    _server->send(400, "application/json", "{\"error\":\"invalid slot\"}");
    return;
  }
  AutoModeConfig c = _autoMode->getConfig();
  bool ok = _configMgr->saveSlot(slot, name, c);
  if (ok) {
    _configMgr->setActiveSlot(slot);
  }
  _server->send(200, "application/json", ok ? "{\"ok\":true}" : "{\"error\":\"save failed\"}");
}

void WebController::handleSlotLoad() {
  if (!_configMgr) { _server->send(500, "application/json", "{\"error\":\"config manager not available\"}"); return; }
  if (!_server->hasArg("slot")) {
    _server->send(400, "application/json", "{\"error\":\"missing slot\"}");
    return;
  }
  int slot = _server->arg("slot").toInt();
  AutoModeConfig c;
  String name;
  if (!_configMgr->loadSlot(slot, c, name)) {
    _server->send(404, "application/json", "{\"error\":\"slot empty or invalid\"}");
    return;
  }
  _autoMode->setConfig(c);
  _configMgr->setActiveSlot(slot);
  _server->send(200, "application/json", "{\"ok\":true,\"name\":\"" + name + "\"}");
}

void WebController::handleSlotDelete() {
  if (!_configMgr) { _server->send(500, "application/json", "{\"error\":\"config manager not available\"}"); return; }
  if (!_server->hasArg("slot")) {
    _server->send(400, "application/json", "{\"error\":\"missing slot\"}");
    return;
  }
  int slot = _server->arg("slot").toInt();
  bool ok = _configMgr->deleteSlot(slot);
  _server->send(200, "application/json", ok ? "{\"ok\":true}" : "{\"error\":\"delete failed\"}");
}

void WebController::handleSlotExport() {
  if (!_configMgr) { _server->send(500, "application/json", "{\"error\":\"config manager not available\"}"); return; }
  if (!_server->hasArg("slot")) {
    _server->send(400, "application/json", "{\"error\":\"missing slot\"}");
    return;
  }
  int slot = _server->arg("slot").toInt();
  String json = _configMgr->exportSlot(slot);
  if (json == "{}") {
    _server->send(404, "application/json", "{\"error\":\"slot empty\"}");
    return;
  }
  String slotName = _configMgr->getSlotName(slot);
  String safeName = slotName.length() > 0 ? slotName : "slot" + String(slot);
  _server->sendHeader("Content-Disposition", "attachment; filename=\"" + safeName + ".json\"");
  _server->send(200, "application/json", json);
}

void WebController::handleSlotImport() {
  if (!_configMgr) { _server->send(500, "application/json", "{\"error\":\"config manager not available\"}"); return; }
  if (!_server->hasArg("slot") || !_server->hasArg("json")) {
    _server->send(400, "application/json", "{\"error\":\"missing slot or json\"}");
    return;
  }
  int slot = _server->arg("slot").toInt();
  String json = _server->arg("json");
  if (slot < 0 || slot >= SLOT_COUNT) {
    _server->send(400, "application/json", "{\"error\":\"invalid slot\"}");
    return;
  }
  bool ok = _configMgr->importToSlot(slot, json);
  _server->send(200, "application/json", ok ? "{\"ok\":true}" : "{\"error\":\"import failed: invalid JSON\"}");
}

void WebController::handleConfigExport() {
  AutoModeConfig c = _autoMode->getConfig();
  String name = "当前配置";
  if (_configMgr) {
    int active = _configMgr->getActiveSlot();
    if (active >= 0) {
      String sn = _configMgr->getSlotName(active);
      if (sn.length() > 0) name = sn;
    }
  }
  String json;
  if (_configMgr) {
    json = _configMgr->exportConfig(c, name);
  } else {
    json = "{\"version\":1,\"name\":\"" + name + "\",\"enabled\":true}";
  }
  _server->sendHeader("Content-Disposition", "attachment; filename=\"" + name + ".json\"");
  _server->send(200, "application/json", json);
}

void WebController::handleConfigImport() {
  if (!_configMgr) { _server->send(500, "application/json", "{\"error\":\"config manager not available\"}"); return; }
  if (!_server->hasArg("json")) {
    _server->send(400, "application/json", "{\"error\":\"missing json\"}");
    return;
  }
  String json = _server->arg("json");
  AutoModeConfig c;
  if (!_configMgr->importToCurrentConfig(json, c)) {
    _server->send(400, "application/json", "{\"error\":\"invalid config JSON\"}");
    return;
  }
  _autoMode->setConfig(c);
  _configMgr->setActiveSlot(-1);
  _server->send(200, "application/json", "{\"ok\":true}");
}

// ========== HTML ==========

String WebController::generateHTML() {
  return R"rawliteral(
<div class="top-bar">
  <div class="brand">ESP Virtual Keyboard</div>
    <div class="stats">
    <span id="bleSt" class="tag tag-off">BLE: --</span>
    <span id="autoSt" class="tag tag-off">AUTO: OFF</span>
    <span id="upSt" class="tag">00:00:00</span>
    <span id="clockSt" class="tag">--:--:--</span>
    <button class="top-btn top-btn-warn" onclick="bleReboot()" id="btnPair">配对</button>
    <button class="theme-btn" onclick="toggleTheme()" id="themeBtn">明暗</button>
    <button class="top-btn" onclick="toggleLang()" id="btnLang">En</button>
    <button class="top-btn" onclick="showAbout()" id="btnAbout">关于</button>
  </div>
</div>

<div class="main">
  <!-- 控制面板 -->
  <div class="ctrl-grid">
    <div class="ctrl-card">
      <div class="ctrl-title" onclick="togglePanel('autoPanel')"><span id="lblAutoMode">⚙️ 自动模式</span> <span class="arrow">▾</span></div>
      <div id="autoPanel" class="ctrl-body">
        <button id="autoBtn" class="btn btn-green" onclick="toggleAuto()">▶ 开启</button>
        <div class="sld-group">
          <div class="sld-label"><span id="lblInterval">间隔</span> <span id="ivD">800-4000</span>ms</div>
          <div class="sld-row"><input type="range" id="iv1" min="200" max="10000" value="800" oninput="updSld()"><input type="range" id="iv2" min="200" max="10000" value="4000" oninput="updSld()"></div>
        </div>
        <div class="sld-group">
          <div class="sld-label"><span id="lblHold">持续</span> <span id="hdD">80-600</span>ms</div>
          <div class="sld-row"><input type="range" id="hd1" min="20" max="3000" value="80" oninput="updSld()"><input type="range" id="hd2" min="20" max="3000" value="600" oninput="updSld()"></div>
        </div>
        <div class="wt-grid">
          <div class="wt-item"><span id="lblWF">W 前进</span><input type="range" id="wW" min="0" max="100" value="30" oninput="updWt()"><span class="wt-v" id="wWv">0.30</span></div>
          <div class="wt-item"><span id="lblWS">S 后退</span><input type="range" id="wS" min="0" max="100" value="8" oninput="updWt()"><span class="wt-v" id="wSv">0.08</span></div>
          <div class="wt-item"><span id="lblWA">A 左移</span><input type="range" id="wA" min="0" max="100" value="12" oninput="updWt()"><span class="wt-v" id="wAv">0.12</span></div>
          <div class="wt-item"><span id="lblWD">D 右移</span><input type="range" id="wD" min="0" max="100" value="12" oninput="updWt()"><span class="wt-v" id="wDv">0.12</span></div>
          <div class="wt-item"><span id="lblWTL">← 左转</span><input type="range" id="wTL" min="0" max="100" value="10" oninput="updWt()"><span class="wt-v" id="wTLv">0.10</span></div>
          <div class="wt-item"><span id="lblWTR">→ 右转</span><input type="range" id="wTR" min="0" max="100" value="10" oninput="updWt()"><span class="wt-v" id="wTRv">0.10</span></div>
          <div class="wt-item"><span id="lblWSP">Space 跳</span><input type="range" id="wSP" min="0" max="100" value="8" oninput="updWt()"><span class="wt-v" id="wSPv">0.08</span></div>
          <div class="wt-item"><span id="lblWC">C 蹲下</span><input type="range" id="wC" min="0" max="100" value="5" oninput="updWt()"><span class="wt-v" id="wCv">0.05</span></div>
          <div class="wt-item"><span id="lblWZ">Z 趴下</span><input type="range" id="wZ" min="0" max="100" value="5" oninput="updWt()"><span class="wt-v" id="wZv">0.05</span></div>
          <div class="wt-item"><span id="lblWId">Idle 空闲</span><input type="range" id="wId" min="0" max="100" value="8" oninput="updWt()"><span class="wt-v" id="wIdv">0.08</span></div>
        </div>
        <div class="wt-total"><span id="lblTotal">总计</span>: <span id="wtSum">0.88</span></div>
        <div class="btn-row">
          <button class="btn btn-blue btn-half" onclick="applyCfg()" id="btnApply">✅ 应用</button>
          <button class="btn btn-green btn-half" onclick="saveToSlot()" id="btnSaveTo">💾 保存到配置</button>
        </div>
      </div>
    </div>

    <!-- 配置管理 -->
    <div class="ctrl-card slot-card" id="slotCard">
    <div class="slot-title"><span id="lblConfigMgr">💾 配置管理</span>
      <div class="slot-actions">
        <button class="btn btn-blue btn-sm" onclick="exportCurrent()" id="btnExportCurr">📤 导出当前</button>
        <label class="btn btn-green btn-sm import-label" id="lblImportCurr"><span id="lblImportCurrText">📥 导入当前</span><input type="file" accept=".json" onchange="importCurrent(this)" style="display:none"></label>
      </div>
    </div>
      <div id="slotList" class="slot-list">
        <div class="slot-empty">加载中...</div>
      </div>
    </div>
  </div>

  <!-- 键盘 -->
  <div class="kb-wrap">
    <div class="kb">
      <!-- F行 Esc + F1-F12 + PrtSc/ScrLk/Pause -->
      <div class="kb-row">
        <div class="k w1" data-k="esc">Esc</div>
        <div class="k-gap"></div>
        <div class="k w1" data-k="f1">F1</div>
        <div class="k w1" data-k="f2">F2</div>
        <div class="k w1" data-k="f3">F3</div>
        <div class="k w1" data-k="f4">F4</div>
        <div class="k-gap"></div>
        <div class="k w1" data-k="f5">F5</div>
        <div class="k w1" data-k="f6">F6</div>
        <div class="k w1" data-k="f7">F7</div>
        <div class="k w1" data-k="f8">F8</div>
        <div class="k-gap"></div>
        <div class="k w1" data-k="f9">F9</div>
        <div class="k w1" data-k="f10">F10</div>
        <div class="k w1" data-k="f11">F11</div>
        <div class="k w1" data-k="f12">F12</div>
      </div>
      <!-- 数字行 -->
      <div class="kb-row">
        <div class="k w1" data-k="grave">`~</div>
        <div class="k w1" data-k="1">1 !</div>
        <div class="k w1" data-k="2">2 @</div>
        <div class="k w1" data-k="3">3 #</div>
        <div class="k w1" data-k="4">4 $</div>
        <div class="k w1" data-k="5">5 %</div>
        <div class="k w1" data-k="6">6 ^</div>
        <div class="k w1" data-k="7">7 &</div>
        <div class="k w1" data-k="8">8 *</div>
        <div class="k w1" data-k="9">9 (</div>
        <div class="k w1" data-k="0">0 )</div>
        <div class="k w1" data-k="minus">-_</div>
        <div class="k w1" data-k="equal">=+</div>
        <div class="k w2" data-k="backspace">← Bksp</div>
      </div>
      <!-- QWERTY行 -->
      <div class="kb-row">
        <div class="k w1-5" data-k="tab">Tab ⇆</div>
        <div class="k w1" data-k="q">Q</div>
        <div class="k w1" data-k="w">W</div>
        <div class="k w1" data-k="e">E</div>
        <div class="k w1" data-k="r">R</div>
        <div class="k w1" data-k="t">T</div>
        <div class="k w1" data-k="y">Y</div>
        <div class="k w1" data-k="u">U</div>
        <div class="k w1" data-k="i">I</div>
        <div class="k w1" data-k="o">O</div>
        <div class="k w1" data-k="p">P</div>
        <div class="k w1" data-k="lbracket">[{</div>
        <div class="k w1" data-k="rbracket">]}</div>
        <div class="k w1-5" data-k="backslash">\|</div>
      </div>
      <!-- ASDF行 -->
      <div class="kb-row">
        <div class="k w1-75" data-k="capslock">Caps ⇪</div>
        <div class="k w1" data-k="a">A</div>
        <div class="k w1" data-k="s">S</div>
        <div class="k w1" data-k="d">D</div>
        <div class="k w1" data-k="f">F</div>
        <div class="k w1" data-k="g">G</div>
        <div class="k w1" data-k="h">H</div>
        <div class="k w1" data-k="j">J</div>
        <div class="k w1" data-k="k">K</div>
        <div class="k w1" data-k="l">L</div>
        <div class="k w1" data-k="semicolon">;:</div>
        <div class="k w1" data-k="apostrophe">'"</div>
        <div class="k w2-25" data-k="enter">Enter ↵</div>
      </div>
      <!-- ZXCV行 -->
      <div class="kb-row">
        <div class="k w2-25" data-k="lshift">Shift ⇧</div>
        <div class="k w1" data-k="z">Z</div>
        <div class="k w1" data-k="x">X</div>
        <div class="k w1" data-k="c">C</div>
        <div class="k w1" data-k="v">V</div>
        <div class="k w1" data-k="b">B</div>
        <div class="k w1" data-k="n">N</div>
        <div class="k w1" data-k="m">M</div>
        <div class="k w1" data-k="comma">,<</div>
        <div class="k w1" data-k="period">.></div>
        <div class="k w1" data-k="slash">/?</div>
        <div class="k w2-75" data-k="rshift">Shift ⇧</div>
      </div>
      <!-- 底部修饰键行 -->
      <div class="kb-row">
        <div class="k w1-25" data-k="lctrl">Ctrl</div>
        <div class="k w1-25">❖</div>
        <div class="k w1-25" data-k="lalt">Alt</div>
        <div class="k w6-25" data-k="space"></div>
        <div class="k w1-25" data-k="ralt">Alt</div>
        <div class="k w1-25">❖</div>
        <div class="k w1-25">☰</div>
        <div class="k w1-25" data-k="rctrl">Ctrl</div>
      </div>
    </div>

      <!-- 编辑键区和方向键 (右侧分离区域) -->
      <div class="kb-right">
        <div class="kb-row">
          <div class="k w1 sm" data-k="insert">Ins</div>
          <div class="k w1 sm" data-k="home">Home</div>
          <div class="k w1 sm" data-k="pageup">PgUp</div>
        </div>
        <div class="kb-row">
          <div class="k w1 sm" data-k="delete">Del</div>
          <div class="k w1 sm" data-k="end">End</div>
          <div class="k w1 sm" data-k="pagedown">PgDn</div>
        </div>
        <div class="kb-row">
          <div class="k-gap-fill"></div>
          <div class="k-gap-fill"></div>
          <div class="k-gap-fill"></div>
        </div>
        <div class="kb-row">
          <div class="k-gap-fill"></div>
          <div class="k w1 sm" data-k="up">▲</div>
          <div class="k-gap-fill"></div>
        </div>
        <div class="kb-row">
          <div class="k w1 sm" data-k="left">◀</div>
          <div class="k w1 sm" data-k="down">▼</div>
          <div class="k w1 sm" data-k="right">▶</div>
        </div>
      </div>

      <!-- 数字小键盘 -->
      <div class="kb-numpad">
        <div class="kb-row">
          <div class="k w1 sm" data-k="numlock">Num</div>
          <div class="k w1 sm" data-k="numpaddiv">÷</div>
          <div class="k w1 sm" data-k="numpadmul">×</div>
          <div class="k w1 sm" data-k="numpadsub">−</div>
        </div>
        <div class="kb-row">
          <div class="k w1 sm" data-k="numpad7">7</div>
          <div class="k w1 sm" data-k="numpad8">8</div>
          <div class="k w1 sm" data-k="numpad9">9</div>
          <div class="k w1 sm np-add" data-k="numpadadd">+</div>
        </div>
        <div class="kb-row">
          <div class="k w1 sm" data-k="numpad4">4</div>
          <div class="k w1 sm" data-k="numpad5">5</div>
          <div class="k w1 sm" data-k="numpad6">6</div>
          <div class="k-gap-fill"></div>
        </div>
        <div class="kb-row">
          <div class="k w1 sm" data-k="numpad1">1</div>
          <div class="k w1 sm" data-k="numpad2">2</div>
          <div class="k w1 sm" data-k="numpad3">3</div>
          <div class="k w1 sm np-enter" data-k="numpadenter">⏎</div>
        </div>
        <div class="kb-row">
          <div class="k w2 sm np-zero" data-k="numpad0">0</div>
          <div class="k w1 sm" data-k="numpaddot">.</div>
          <div class="k-gap-fill"></div>
        </div>
      </div>
  </div>

  <!-- 按键日志 & 统计 -->
  <div class="log-stats-grid">
  <div class="log-card">
    <div class="log-title"><span id="lblKeyLog">📋 按键日志</span> <span id="logRunTime" style="font-size:0.8em;color:var(--dim);margin-left:auto;margin-right:8px">00:00:00</span> <button class="log-clear-btn" onclick="clearLog()" id="btnClearLog">清空</button></div>
    <div id="logBox" class="log-box"><div class="log-empty" id="lblWaitLog">等待自动模式按键...</div></div>
  </div>

  <!-- 按键统计 -->
  <div class="stats-card">
    <div class="stats-title"><span id="lblKeyStats">📊 按键统计</span> <button class="log-clear-btn" onclick="clearStats()" id="btnResetStats">重置</button></div>
    <div id="statsBox" class="stats-body">
      <div class="stats-row stats-hdr"><span class="sk" id="lblKeyCol">按键</span><span class="sc" id="lblCountCol">次数</span><span class="sp" id="lblPctCol">比例</span></div>
      <div id="statsRows"></div>
      <div class="stats-row stats-total"><span class="sk" id="lblTotalRow">总计</span><span class="sc" id="statsTotal">0</span><span class="sp"></span></div>
    </div>
  </div>
  </div>
<!-- 配置槽位选择弹窗 -->
<div id="slotModal" class="modal-overlay" style="display:none">
  <div class="modal-card">
    <div class="modal-title" id="slotModalTitle">💾 保存到配置</div>
    <div class="modal-subtitle" id="slotModalSub">选择目标槽位：</div>
    <div id="modalSlots" class="modal-slots"></div>
    <button class="btn btn-red modal-cancel" onclick="closeSlotModal()" id="btnSlotCancel">取消</button>
  </div>
</div>

<!-- 关于弹窗 -->
<div id="aboutModal" class="modal-overlay" style="display:none">
  <div class="modal-card">
    <div class="modal-title" id="aboutTitle">关于</div>
    <div class="about-content">
      <div class="about-name">ESP Virtual Keyboard</div>
      <div class="about-ver">v1.0</div>
      <div class="about-row"><span class="about-label" id="aboutAuthor">作者</span><span>IceyVanci</span></div>
      <div class="about-row"><span class="about-label" id="aboutLicense">许可证</span><span>MIT License</span></div>
      <div class="about-row"><span class="about-label" id="aboutDev">开发</span><span id="aboutDevText">在 MimoV2.5Pro 协助下开发</span></div>
      <div class="about-row"><span class="about-label">GitHub</span><a href="https://github.com/IceyVanci/ESPVirtualKeyboard" target="_blank" style="color:var(--accent);word-break:break-all">IceyVanci/ESPVirtualKeyboard</a></div>
    </div>
    <button class="btn btn-blue" onclick="closeAbout()" id="aboutClose">关闭</button>
  </div>
</div>

</div>
)rawliteral";
}

// ========== CSS ==========

String WebController::generateCSS() {
  return R"rawliteral(
:root{--bg:#0d1117;--card:#161b22;--border:#30363d;--text:#c9d1d9;--dim:#8b949e;--accent:#58a6ff;--green:#3fb950;--red:#f85149;--orange:#d29922;--key-bg:#21262d;--key-border:#30363d;--key-h:#30363d;--key-active:#1f6feb}
[data-theme="light"]{--bg:#f6f8fa;--card:#ffffff;--border:#d0d7de;--text:#24292f;--dim:#57606a;--accent:#0969da;--green:#1a7f37;--red:#cf222e;--orange:#bf8700;--key-bg:#f6f8fa;--key-border:#d0d7de;--key-h:#eaeef2;--key-active:#0969da}
*{margin:0;padding:0;box-sizing:border-box}
html{scroll-behavior:smooth}
body{font-family:'Segoe UI',system-ui,sans-serif;background:var(--bg);color:var(--text);min-height:100vh;font-size:14px;line-height:1.5}
.top-bar{display:flex;align-items:center;justify-content:space-between;padding:12px 20px;background:linear-gradient(135deg,var(--card),rgba(22,27,34,0.95));border-bottom:1px solid var(--border);position:sticky;top:0;z-index:10}
.brand{font-weight:700;font-size:1.15em;background:linear-gradient(135deg,var(--accent),#a78bfa);-webkit-background-clip:text;-webkit-text-fill-color:transparent;background-clip:text}
.stats{display:flex;gap:6px;align-items:center}
.tag{padding:3px 10px;border-radius:12px;font-size:0.75em;font-weight:600;background:#21262d;color:var(--dim)}
.tag-ok{background:#0d2818;color:var(--green)}
.tag-warn{background:#2d1800;color:var(--orange)}
.tag-off{background:#2d1010;color:var(--red)}
.main{max-width:1200px;margin:0 auto;padding:14px 12px}
.ctrl-grid{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-bottom:12px}
.ctrl-card{background:var(--card);border:1px solid var(--border);border-radius:10px;overflow:hidden;transition:border-color .2s}
.ctrl-title{padding:10px 14px;cursor:pointer;font-weight:600;font-size:0.9em;display:flex;justify-content:space-between;align-items:center;user-select:none;border-left:3px solid var(--accent)}
.ctrl-title:hover{background:var(--key-bg)}
.arrow{font-size:0.8em;transition:transform .2s}
.arrow.up{transform:rotate(180deg)}
.ctrl-body{padding:0 14px 12px;display:block}
.ctrl-body.hidden{display:none}
.btn{border:none;border-radius:8px;padding:9px 16px;font-size:0.82em;cursor:pointer;font-weight:600;transition:all .15s;width:100%;margin-top:8px}
.btn:hover{filter:brightness(1.15);transform:translateY(-1px)}
.btn:active{transform:scale(0.97)}
.btn-green{background:var(--green);color:#000}
.btn-red{background:var(--red);color:#fff}
.btn-blue{background:var(--accent);color:#000}
.btn-warn{background:var(--orange);color:#000}
.btn-row{display:flex;gap:8px;margin-top:8px}
.btn-half{flex:1;margin-top:0}
.sld-group{margin:8px 0}
.sld-label{font-size:0.78em;color:var(--dim);margin-bottom:3px}
.sld-row{display:flex;gap:6px}
.sld-row input{flex:1;accent-color:var(--accent)}
.wt-grid{display:grid;grid-template-columns:1fr 1fr;gap:2px 10px;margin:6px 0}
.wt-item{display:flex;align-items:center;gap:4px;font-size:0.75em}
.wt-item span:first-child{width:50px;text-align:right;color:var(--dim);white-space:nowrap}
.wt-item input{flex:1;accent-color:var(--accent);min-width:0}
.wt-v{width:28px;color:var(--accent);text-align:right;font-size:0.9em}
.wt-total{margin-top:6px;padding:4px 0;font-size:0.82em;text-align:right;color:var(--dim)}
.wt-total .ok{color:var(--green);font-weight:600}
.wt-total .over{color:var(--red);font-weight:600}
.kb-wrap{background:var(--card);border:1px solid var(--border);border-radius:12px;padding:16px;display:flex;justify-content:center;align-items:flex-start;gap:14px;overflow-x:auto}
.kb{display:inline-flex;flex-direction:column;gap:4px}
.kb-row{display:flex;gap:3px}
.kb-right{display:flex;flex-direction:column;gap:3px}
.k{background:var(--key-bg);border:1px solid var(--key-border);border-radius:7px;display:flex;align-items:center;justify-content:center;font-size:0.72em;cursor:pointer;user-select:none;transition:all .08s;min-height:34px;padding:2px 4px;text-align:center;line-height:1.2;box-shadow:0 1px 2px rgba(0,0,0,0.1)}
.k:hover{background:var(--key-h);border-color:var(--dim);transform:translateY(-1px)}
.k:active,.k.on{background:var(--key-active);color:#fff;border-color:var(--key-active);transform:translateY(1px);box-shadow:inset 0 1px 3px rgba(0,0,0,0.2)}
.k.auto-on{background:#3fb950;color:#000;border-color:#3fb950;box-shadow:0 0 8px rgba(63,185,80,0.4)}
.k.sm{font-size:0.65em;min-height:30px}
.k-gap{width:6px;flex-shrink:0}
.k-gap-fill{width:42px;height:30px}
.kb-numpad{display:flex;flex-direction:column;gap:3px;padding-left:14px;border-left:2px solid var(--border)}
.np-add{min-height:30px;height:auto}
.np-enter{height:auto;position:relative}
.np-zero{width:42px}
.theme-btn,.top-btn{background:transparent;border:1px solid var(--border);border-radius:6px;padding:4px 10px;font-size:1.1em;cursor:pointer;transition:all .15s;color:var(--dim)}
.theme-btn:hover,.top-btn:hover{background:var(--key-bg);color:var(--text)}
.top-btn-warn{color:var(--orange)}
.top-btn-warn:hover{background:#2d1800}
.w1{width:42px}
.w1-25{width:52px}
.w1-5{width:62px}
.w1-75{width:72px}
.w2{width:84px}
.w2-25{width:94px}
.w2-75{width:114px}
.w6-25{width:264px}
.log-stats-grid{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-top:12px}
.log-card{background:var(--card);border:1px solid var(--border);border-radius:12px;overflow:hidden}
.log-title{padding:10px 14px;font-weight:600;font-size:0.9em;display:flex;justify-content:space-between;align-items:center;border-bottom:1px solid var(--border);border-left:3px solid var(--green)}
.log-clear-btn{background:transparent;border:1px solid var(--border);border-radius:6px;padding:4px 12px;font-size:0.75em;cursor:pointer;color:var(--dim);transition:all .15s}
.log-clear-btn:hover{background:var(--key-bg);color:var(--text)}
.log-box{padding:10px 14px;height:240px;overflow-y:auto;font-family:'Consolas','Monaco',monospace;font-size:0.8em;line-height:1.8}
.log-empty{color:var(--dim);text-align:center;padding:40px 0;font-style:italic}
.log-line{border-bottom:1px solid rgba(48,54,61,0.5);padding:3px 4px;display:flex;gap:8px;border-radius:4px;transition:background .15s}
.log-line:last-child{border-bottom:none}
.log-line:hover{background:rgba(88,166,255,0.06)}
.log-time{color:var(--dim);min-width:60px;font-size:0.9em}
.log-icon{min-width:20px;text-align:center}
.log-icon.down{color:var(--green)}
.log-icon.up{color:var(--orange)}
.log-key{color:var(--accent);font-weight:600;min-width:60px}
.log-key.space-key{letter-spacing:1px}
.stats-card{background:var(--card);border:1px solid var(--border);border-radius:12px;overflow:hidden}
.stats-title{padding:10px 14px;font-weight:600;font-size:0.9em;display:flex;justify-content:space-between;align-items:center;border-bottom:1px solid var(--border);border-left:3px solid var(--accent)}
.stats-body{padding:10px 14px}
.stats-row{display:flex;align-items:center;padding:4px 4px;font-size:0.82em;border-bottom:1px solid rgba(48,54,61,0.3);border-radius:4px;transition:background .15s}
.stats-row:last-child{border-bottom:none}
.stats-row:hover{background:rgba(88,166,255,0.06)}
.stats-hdr{color:var(--dim);font-size:0.75em;font-weight:600}
.stats-total{font-weight:600;color:var(--accent);border-top:1px solid var(--border);margin-top:4px;padding-top:6px}
.sk{width:60px;text-align:right;padding-right:10px;white-space:nowrap}
.sc{width:50px;text-align:right;padding-right:10px;font-family:'Consolas','Monaco',monospace}
.sp{flex:1;display:flex;align-items:center;gap:6px}
.sp-bar{height:8px;border-radius:4px;background:linear-gradient(90deg,var(--accent),#a78bfa);min-width:2px;transition:width .3s}
.sp-pct{font-size:0.85em;color:var(--dim);width:36px;text-align:right}
.slot-card{overflow:hidden}
.slot-title{padding:10px 14px;font-weight:600;font-size:0.9em;display:flex;justify-content:space-between;align-items:center;border-bottom:1px solid var(--border);border-left:3px solid var(--orange)}
.slot-actions{display:flex;gap:6px}
.btn-sm{width:auto;padding:5px 12px;font-size:0.75em;margin:0}
.import-label{cursor:pointer;display:inline-flex;align-items:center}
.slot-list{padding:10px 14px}
.slot-empty{color:var(--dim);text-align:center;padding:20px 0;font-style:italic;font-size:0.85em}
.slot-item{display:flex;align-items:center;padding:8px 10px;border:1px solid var(--border);border-radius:8px;margin-bottom:6px;transition:background .15s;gap:8px}
.slot-item:hover{background:var(--key-bg)}
.slot-item.active{border-color:var(--accent);box-shadow:0 0 0 1px var(--accent)}
.slot-num{font-weight:700;font-size:0.85em;color:var(--dim);min-width:24px}
.slot-name{flex:1;font-size:0.85em;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.slot-name.empty{color:var(--dim);font-style:italic}
.slot-badge{font-size:0.65em;padding:2px 6px;border-radius:8px;background:var(--accent);color:#000;font-weight:600}
.slot-btns{display:flex;gap:4px;flex-wrap:wrap;justify-content:flex-end}
.slot-btn{border:none;border-radius:6px;padding:4px 8px;font-size:0.7em;cursor:pointer;font-weight:600;transition:all .15s}
.slot-btn:hover{filter:brightness(1.2)}
.slot-btn.load{background:var(--green);color:#000}
.slot-btn.save{background:var(--accent);color:#000}
.slot-btn.del{background:var(--red);color:#fff}
.slot-btn.exp{background:var(--orange);color:#000}
.modal-overlay{position:fixed;top:0;left:0;width:100%;height:100%;background:rgba(0,0,0,0.6);z-index:100;display:flex;align-items:center;justify-content:center}
.modal-card{background:var(--card);border:1px solid var(--border);border-radius:12px;padding:20px;width:320px;max-width:90vw}
.modal-title{font-weight:700;font-size:1.1em;margin-bottom:6px}
.modal-subtitle{font-size:0.85em;color:var(--dim);margin-bottom:12px}
.modal-slots{display:flex;flex-direction:column;gap:6px;margin-bottom:12px}
.modal-slot{display:flex;align-items:center;padding:10px 12px;border:1px solid var(--border);border-radius:8px;cursor:pointer;transition:all .15s;gap:10px}
.modal-slot:hover{background:var(--key-bg);border-color:var(--accent)}
.modal-slot.active{border-color:var(--accent);box-shadow:0 0 0 1px var(--accent)}
.modal-slot .slot-num{font-weight:700;font-size:0.9em;color:var(--dim);min-width:24px}
.modal-slot .slot-name{flex:1;font-size:0.9em}
.modal-slot .slot-name.empty{color:var(--dim);font-style:italic}
.modal-cancel{margin-top:0}
.about-content{margin-bottom:12px}
.about-name{font-size:1.2em;font-weight:700;color:var(--accent);margin-bottom:2px}
.about-ver{font-size:0.85em;color:var(--dim);margin-bottom:12px}
.about-row{display:flex;gap:10px;padding:4px 0;font-size:0.88em;border-bottom:1px solid rgba(48,54,61,0.3)}
.about-label{color:var(--dim);min-width:48px;font-weight:600}
@media(max-width:768px){
  .ctrl-grid,.log-stats-grid{grid-template-columns:1fr}
  .kb-wrap{padding:8px}
  .k{min-height:28px;font-size:0.62em}
  .k-sm{min-height:24px}
  .w1{width:30px}.w1-25{width:37px}.w1-5{width:44px}.w1-75{width:52px}
  .w2{width:60px}.w2-25{width:68px}.w2-75{width:82px}
  .w6-25{width:190px}
  .k-gap-fill{width:30px;height:24px}
  .main{padding:6px}
  .log-box{height:180px}
}
)rawliteral";
}

// ========== JavaScript ==========

String WebController::generateJS() {
  return R"rawliteral(
var autoOn=false;

document.querySelectorAll('.k[data-k]').forEach(function(k){
  var key=k.dataset.k;
  k.addEventListener('mousedown',function(e){e.preventDefault();k.classList.add('on');fetch('/api/press?key='+key)});
  k.addEventListener('mouseup',function(e){e.preventDefault();k.classList.remove('on');fetch('/api/release?key='+key)});
  k.addEventListener('mouseleave',function(e){k.classList.remove('on');fetch('/api/release?key='+key)});
  k.addEventListener('touchstart',function(e){e.preventDefault();k.classList.add('on');fetch('/api/press?key='+key)},{passive:false});
  k.addEventListener('touchend',function(e){e.preventDefault();k.classList.remove('on');fetch('/api/release?key='+key)},{passive:false});
});

function togglePanel(id){
  var el=document.getElementById(id);
  el.classList.toggle('hidden');
  var arrow=el.previousElementSibling.querySelector('.arrow');
  if(arrow)arrow.classList.toggle('up');
}

function toggleAuto(){
  autoOn=!autoOn;
  fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'enabled='+(autoOn?'true':'false')}).then(function(){updStatus()});
}

function bleReboot(){
  if(!confirm('确定断开当前连接并重新配对？'))return;
  fetch('/api/ble/reboot',{method:'POST'}).then(function(r){return r.json()}).then(function(d){alert(d.msg);updStatus()});
}

function updSld(){
  document.getElementById('ivD').textContent=document.getElementById('iv1').value+'-'+document.getElementById('iv2').value;
  document.getElementById('hdD').textContent=document.getElementById('hd1').value+'-'+document.getElementById('hd2').value;
}

var wtIds=['W','S','A','D','TL','TR','SP','C','Z','Id'];

function updWt(){
  var total=0;
  wtIds.forEach(function(id){
    var v=parseInt(document.getElementById('w'+id).value);
    total+=v;
    document.getElementById('w'+id+'v').textContent=(v/100).toFixed(2);
  });
  var sumEl=document.getElementById('wtSum');
  sumEl.textContent=(total/100).toFixed(2);
  sumEl.className=(total>100?'over':'ok');
}

function getCfgBody(){
  return ['minInterval='+document.getElementById('iv1').value,'maxInterval='+document.getElementById('iv2').value,'minHold='+document.getElementById('hd1').value,'maxHold='+document.getElementById('hd2').value,'weightW='+(parseInt(document.getElementById('wW').value)/100).toFixed(2),'weightS='+(parseInt(document.getElementById('wS').value)/100).toFixed(2),'weightA='+(parseInt(document.getElementById('wA').value)/100).toFixed(2),'weightD='+(parseInt(document.getElementById('wD').value)/100).toFixed(2),'weightTL='+(parseInt(document.getElementById('wTL').value)/100).toFixed(2),'weightTR='+(parseInt(document.getElementById('wTR').value)/100).toFixed(2),'weightSP='+(parseInt(document.getElementById('wSP').value)/100).toFixed(2),'weightC='+(parseInt(document.getElementById('wC').value)/100).toFixed(2),'weightZ='+(parseInt(document.getElementById('wZ').value)/100).toFixed(2),'weightIdle='+(parseInt(document.getElementById('wId').value)/100).toFixed(2)].join('&');
}
function applyCfg(){
  fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:getCfgBody()}).then(function(r){return r.json()}).then(function(d){if(d.ok)alert('已应用！')});
}
function saveToSlot(){
  fetch('/api/slots').then(function(r){return r.json()}).then(function(d){
    window._slotModalData=d;
    var slots=d.slots||[];var active=d.active;
    var html='';
    if(active>=0){
      var sn=slots[active]?slots[active].name:'槽位'+(active+1);
      html+='<div class="modal-slot active" onclick="confirmSlotSave('+active+',true)"><span class="slot-num">★</span><span class="slot-name">覆盖当前: '+sn+'</span><span class="slot-badge">当前</span></div>';
    }
    for(var i=0;i<slots.length;i++){
      var s=slots[i];
      var nm=s.used?s.name:'空槽位';
      var cls='modal-slot'+(s.index===active?' active':'');
      html+='<div class="'+cls+'" onclick="confirmSlotSave('+s.index+',false)"><span class="slot-num">'+(s.index+1)+'</span><span class="slot-name'+(s.used?'':' empty')+'">'+nm+'</span></div>';
    }
    document.getElementById('modalSlots').innerHTML=html;
    document.getElementById('slotModal').style.display='flex';
  });
}
function closeSlotModal(){document.getElementById('slotModal').style.display='none';}
function confirmSlotSave(idx,isOverride){
  closeSlotModal();
  var slots=window._slotModalData?window._slotModalData.slots:[];
  var defaultName=slots[idx]&&slots[idx].used?slots[idx].name:'配置'+(idx+1);
  var name=isOverride?defaultName:prompt('输入配置名称：',defaultName);
  if(name===null)return;
  if(!name.trim())name=defaultName;
  fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:getCfgBody()}).then(function(){return fetch('/api/slot/save',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'slot='+idx+'&name='+encodeURIComponent(name.trim())})}).then(function(r){return r.json()}).then(function(d){
    if(d.ok){loadSlots();alert('已保存到槽位'+(idx+1));}else{alert(d.error||'保存失败');}
  });
}

function updStatus(){
  fetch('/api/status').then(function(r){return r.json()}).then(function(d){
    var bs=document.getElementById('bleSt');
    if(d.bleState=='connected'){bs.className='tag tag-ok';bs.textContent='BLE: ✓已连接'}
    else if(d.bleState=='advertising'){bs.className='tag tag-warn';bs.textContent='BLE: 广播中'}
    else{bs.className='tag tag-off';bs.textContent='BLE: ✕已停止'}
    var as=document.getElementById('autoSt');
    autoOn=d.autoMode;
    if(d.autoMode){as.className='tag tag-ok';as.textContent='AUTO: ON'}
    else{as.className='tag tag-off';as.textContent='AUTO: OFF'}
    var ab=document.getElementById('autoBtn');
    if(d.autoMode){ab.className='btn btn-red';ab.innerHTML='⏹ 关闭'}else{ab.className='btn btn-green';ab.innerHTML='▶ 开启'}
    document.getElementById('upSt').textContent=fmtHMS(d.uptime);
    if(d.epoch>100000){window._espEpoch=d.epoch;window._espUpMs=d.uptime*1000;var d2=new Date(d.epoch*1000);document.getElementById('clockSt').textContent=pad2(d2.getHours())+':'+pad2(d2.getMinutes())+':'+pad2(d2.getSeconds());}
    document.getElementById('logRunTime').textContent=fmtHMS(d.uptime);
    // 键盘高亮同步
    document.querySelectorAll('.k.auto-on').forEach(function(el){el.classList.remove('auto-on')});
    if(d.autoMode&&d.currentKey){
      var el=document.querySelector('.k[data-k="'+d.currentKey+'"]');
      if(el)el.classList.add('auto-on');
    }
  });
}

function loadCfg(){
  fetch('/api/config').then(function(r){return r.json()}).then(function(d){
    autoOn=d.enabled;
    document.getElementById('iv1').value=d.minInterval;
    document.getElementById('iv2').value=d.maxInterval;
    document.getElementById('hd1').value=d.minHold;
    document.getElementById('hd2').value=d.maxHold;
    document.getElementById('wW').value=Math.round(d.weightW*100);
    document.getElementById('wS').value=Math.round(d.weightS*100);
    document.getElementById('wA').value=Math.round(d.weightA*100);
    document.getElementById('wD').value=Math.round(d.weightD*100);
    document.getElementById('wTL').value=Math.round(d.weightTL*100);
    document.getElementById('wTR').value=Math.round(d.weightTR*100);
    document.getElementById('wSP').value=Math.round(d.weightSP*100);
    document.getElementById('wC').value=Math.round(d.weightC*100);
    document.getElementById('wZ').value=Math.round(d.weightZ*100);
    document.getElementById('wId').value=Math.round(d.weightIdle*100);
    updSld();updWt();
  });
}

// ---- 按键日志 ----
var logStartT=0;
var lastEvtId=0;
var logEntries=[];
var logBox=null;
var logEmpty=true;

function pad2(n){return n<10?'0'+n:''+n;}
function fmtHMS(sec){var h=Math.floor(sec/3600);var m=Math.floor((sec%3600)/60);var s=sec%60;return pad2(h)+':'+pad2(m)+':'+pad2(s);}
function fmtTime(ms){
  var s=Math.floor(ms/1000);
  var m=Math.floor(s/60);s=s%60;
  return (m<10?'0':'')+m+':'+(s<10?'0':'')+s;
}

function addLogLine(key,down,ts){
  if(!logBox)logBox=document.getElementById('logBox');
  if(logEmpty){logBox.innerHTML='';logEmpty=false;}
  var div=document.createElement('div');
  div.className='log-line';
  var t=fmtTime(ts-logStartT);
  var clockStr='';
  if(window._espEpoch&&window._espEpoch>100000){var evtS=Math.floor(ts/1000);var ep=Math.floor((window._espEpoch*1000+ts-window._espUpMs)/1000);var d3=new Date(ep*1000);clockStr=pad2(d3.getHours())+':'+pad2(d3.getMinutes())+':'+pad2(d3.getSeconds());}
  var icon=down?'⬇':'⬆';
  var cls=down?'down':'up';
  var displayName=key;
  if(key==='space')displayName='Space';
  else if(key==='left')displayName='←';
  else if(key==='right')displayName='→';
  else displayName=key.toUpperCase();
  var kc=key==='space'?'log-key space-key':'log-key';
  div.innerHTML=(clockStr?'<span class="log-time" style="min-width:56px;color:var(--accent)">'+clockStr+'</span>':'')+'<span class="log-time">'+t+'</span><span class="log-icon '+cls+'">'+icon+'</span><span class="'+kc+'">'+displayName+'</span>';
  logBox.appendChild(div);
  logEntries.push(div);
  logBox.scrollTop=logBox.scrollHeight;
}

function clearLog(){
  if(logBox)logBox.innerHTML='<div class="log-empty">等待自动模式按键...</div>';
  logEntries=[];
  logEmpty=true;
}

function pollEvents(){
  fetch('/api/events?since='+lastEvtId).then(function(r){return r.json()}).then(function(d){
    if(d.events&&d.events.length>0){
      if(logStartT===0)logStartT=d.events[0].t;
      d.events.forEach(function(ev){
        addLogLine(ev.key,ev.down,ev.t);
      });
    }
    if(d.lastId>lastEvtId)lastEvtId=d.lastId;
  }).catch(function(){});
}

// ---- 按键统计 ----
var statsKeyNames=['W','S','A','D','TL','TR','SP','C','Z','Idle'];
var statsKeyLabels=['W 前进','S 后退','A 左移','D 右移','← 左转','→ 右转','Space 跳','C 蹲下','Z 趴下','Idle 空闲'];
var statsData={total:0,keys:{}};
var statsBaseline={total:0,keys:{}};

function updStats(){
  fetch('/api/stats').then(function(r){return r.json()}).then(function(d){
    statsData=d;
    renderStats();
  }).catch(function(){});
}

function renderStats(){
  var rawTotal=statsData.total||0;
  var adjTotal=rawTotal-statsBaseline.total;
  if(adjTotal<0)adjTotal=0;
  var rows=document.getElementById('statsRows');
  if(!rows)return;
  var html='';
  statsKeyNames.forEach(function(k,i){
    var rawCnt=statsData.keys?statsData.keys[k]||0:0;
    var baseCnt=statsBaseline.keys?statsBaseline.keys[k]||0:0;
    var cnt=rawCnt-baseCnt;
    if(cnt<0)cnt=0;
    var pct=adjTotal>0?(cnt/adjTotal*100).toFixed(1):'0.0';
    var barW=adjTotal>0?(cnt/adjTotal*100):0;
    html+='<div class="stats-row"><span class="sk">'+statsKeyLabels[i]+'</span><span class="sc">'+cnt+'</span><span class="sp"><span class="sp-bar" style="width:'+barW+'%"></span><span class="sp-pct">'+pct+'%</span></span></div>';
  });
  rows.innerHTML=html;
  document.getElementById('statsTotal').textContent=adjTotal;
}

function clearStats(){
  statsBaseline={total:statsData.total||0,keys:{}};
  statsKeyNames.forEach(function(k){statsBaseline.keys[k]=statsData.keys?statsData.keys[k]||0:0;});
  renderStats();
}

// ---- 主题切换 ----
function toggleTheme(){
  var t=document.documentElement.getAttribute('data-theme');
  if(t==='light'){document.documentElement.setAttribute('data-theme','dark');localStorage.setItem('theme','dark');document.getElementById('themeBtn').textContent='🌙';}
  else{document.documentElement.setAttribute('data-theme','light');localStorage.setItem('theme','light');document.getElementById('themeBtn').textContent='☀️';}
}
(function(){var s=localStorage.getItem('theme');if(s==='light'){document.documentElement.setAttribute('data-theme','light');document.getElementById('themeBtn').textContent='☀️';}})();

// ---- 配置槽位管理 ----
var slotData=[];

function loadSlots(){
  fetch('/api/slots').then(function(r){return r.json()}).then(function(d){
    slotData=d.slots||[];
    var active=d.active;
    var list=document.getElementById('slotList');
    if(!list)return;
    var html='';
    for(var i=0;i<slotData.length;i++){
      var s=slotData[i];
      var cls='slot-item'+(s.index===active?' active':'');
      html+='<div class="'+cls+'">';
      html+='<span class="slot-num">'+(s.index+1)+'</span>';
      if(s.used){
        html+='<span class="slot-name">'+s.name+'</span>';
        if(s.index===active)html+='<span class="slot-badge">当前</span>';
        html+='<div class="slot-btns">';
        html+='<button class="slot-btn load" onclick="slotLoad('+s.index+')">加载</button>';
        html+='<button class="slot-btn del" onclick="slotDelete('+s.index+')">删除</button>';
        html+='<button class="slot-btn exp" onclick="slotExport('+s.index+')">导出</button>';
        html+='<label class="slot-btn exp import-label">导入<input type="file" accept=".json" onchange="slotImportFile('+s.index+',this)" style="display:none"></label>';
        html+='</div>';
      }else{
        html+='<span class="slot-name empty">空槽位</span>';
        html+='<div class="slot-btns">';
        html+='<button class="slot-btn save" onclick="slotSave('+s.index+')">保存</button>';
        html+='<label class="slot-btn exp import-label">导入<input type="file" accept=".json" onchange="slotImportFile('+s.index+',this)" style="display:none"></label>';
        html+='</div>';
      }
      html+='</div>';
    }
    if(slotData.length===0)html='<div class="slot-empty">无可用槽位</div>';
    list.innerHTML=html;
  }).catch(function(){});
}

function slotSave(idx){
  var name=prompt('输入配置名称：','配置'+(idx+1));
  if(name===null)return;
  if(!name.trim())name='配置'+(idx+1);
  fetch('/api/slot/save',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'slot='+idx+'&name='+encodeURIComponent(name.trim())}).then(function(r){return r.json()}).then(function(d){
    if(d.ok){loadSlots();loadCfg();}else{alert(d.error||'保存失败');}
  });
}

function slotLoad(idx){
  fetch('/api/slot/load',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'slot='+idx}).then(function(r){return r.json()}).then(function(d){
    if(d.ok){loadCfg();loadSlots();updSld();updWt();alert('已加载：'+d.name);}else{alert(d.error||'加载失败');}
  });
}

function slotDelete(idx){
  if(!confirm('确定删除槽位'+(idx+1)+'的配置？'))return;
  fetch('/api/slot/delete',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'slot='+idx}).then(function(r){return r.json()}).then(function(d){
    if(d.ok){loadSlots();}else{alert(d.error||'删除失败');}
  });
}

function slotExport(idx){
  var form=document.createElement('form');
  form.method='POST';
  form.action='/api/slot/export';
  var inp=document.createElement('input');
  inp.type='hidden';inp.name='slot';inp.value=idx;
  form.appendChild(inp);
  document.body.appendChild(form);
  form.submit();
  document.body.removeChild(form);
}

function slotImportFile(idx,input){
  if(!input.files||!input.files[0])return;
  var reader=new FileReader();
  reader.onload=function(e){
    var json=e.target.result;
    fetch('/api/slot/import',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'slot='+idx+'&json='+encodeURIComponent(json)}).then(function(r){return r.json()}).then(function(d){
      if(d.ok){loadSlots();alert('导入成功！');}else{alert(d.error||'导入失败');}
    });
  };
  reader.readAsText(input.files[0]);
  input.value='';
}

function exportCurrent(){
  var form=document.createElement('form');
  form.method='GET';
  form.action='/api/config/export';
  document.body.appendChild(form);
  form.submit();
  document.body.removeChild(form);
}

function importCurrent(input){
  if(!input.files||!input.files[0])return;
  var reader=new FileReader();
  reader.onload=function(e){
    var json=e.target.result;
    fetch('/api/config/import',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'json='+encodeURIComponent(json)}).then(function(r){return r.json()}).then(function(d){
      if(d.ok){loadCfg();loadSlots();updSld();updWt();alert('已导入到当前配置！');}else{alert(d.error||'导入失败');}
    });
  };
  reader.readAsText(input.files[0]);
  input.value='';
}

// ---- 关于弹窗 ----
function showAbout(){document.getElementById('aboutModal').style.display='flex';}
function closeAbout(){document.getElementById('aboutModal').style.display='none';}

// ---- 中英文切换 ----
var lang='zh';
var i18n={
  zh:{pair:'配对',theme:'明暗',langBtn:'En',about:'关于',
      autoMode:'⚙️ 自动模式',on:'▶ 开启',off:'⏹ 关闭',
      interval:'间隔',hold:'持续',total:'总计',
      wForward:'W 前进',wBack:'S 后退',wLeft:'A 左移',wRight:'D 右移',
      tLeft:'← 左转',tRight:'→ 右转',jump:'Space 跳',crouch:'C 蹲下',prone:'Z 趴下',idle:'Idle 空闲',
      apply:'✅ 应用',saveTo:'💾 保存到配置',configMgr:'💾 配置管理',
      exportCurr:'📤 导出当前',importCurr:'📥 导入当前',
      load:'加载',del:'删除',exp:'导出',imp:'导入',save:'保存',emptySlot:'空槽位',current:'当前',
      keyLog:'📋 按键日志',keyStats:'📊 按键统计',clear:'清空',reset:'重置',
      aboutTitle:'关于',author:'作者',license:'许可证',dev:'开发',devText:'在 MimoV2.5Pro 协助下开发',
      waitLog:'等待自动模式按键...',saveSlotTitle:'💾 保存到配置',saveSlotSub:'选择目标槽位：',
      confirm:'确定断开当前连接并重新配对？',enterName:'输入配置名称：',
      delConfirm:'确定删除槽位%d的配置？',saved:'已保存到槽位%d',loaded:'已加载：',
      applied:'已应用！',deleted:'已删除',imported:'导入成功！',
      keyCol:'按键',countCol:'次数',pctCol:'比例',
      bleConnected:'BLE: ✓已连接',bleAdv:'BLE: 广播中',bleOff:'BLE: ✕已停止',
      autoOn:'AUTO: ON',autoOff:'AUTO: OFF'},
  en:{pair:'Pair',theme:'Theme',langBtn:'中',about:'About',
      autoMode:'⚙️ Auto Mode',on:'▶ Start',off:'⏹ Stop',
      interval:'Interval',hold:'Hold',total:'Total',
      wForward:'W Forward',wBack:'S Back',wLeft:'A Left',wRight:'D Right',
      tLeft:'← Turn L',tRight:'→ Turn R',jump:'Space Jump',crouch:'C Crouch',prone:'Z Prone',idle:'Idle',
      apply:'✅ Apply',saveTo:'💾 Save to Slot',configMgr:'💾 Config Manager',
      exportCurr:'📤 Export',importCurr:'📥 Import',
      load:'Load',del:'Delete',exp:'Export',imp:'Import',save:'Save',emptySlot:'Empty',current:'Active',
      keyLog:'📋 Key Log',keyStats:'📊 Key Stats',clear:'Clear',reset:'Reset',
      aboutTitle:'About',author:'Author',license:'License',dev:'Developed',devText:'with MimoV2.5Pro',
      waitLog:'Waiting for auto mode keys...',saveSlotTitle:'💾 Save to Slot',saveSlotSub:'Select target slot:',
      confirm:'Disconnect and re-pair?',enterName:'Enter config name:',
      delConfirm:'Delete slot %d?',saved:'Saved to slot %d',loaded:'Loaded: ',
      applied:'Applied!',deleted:'Deleted',imported:'Imported!',
      keyCol:'Key',countCol:'Count',pctCol:'Ratio',
      bleConnected:'BLE: ✓Connected',bleAdv:'BLE: Advertising',bleOff:'BLE: ✕Stopped',
      autoOn:'AUTO: ON',autoOff:'AUTO: OFF'}
};
function L(k){return i18n[lang][k]||k;}
function toggleLang(){
  lang=lang==='zh'?'en':'zh';
  localStorage.setItem('lang',lang);
  applyLang();
}
function applyLang(){
  var d=i18n[lang];
  // Banner
  document.getElementById('btnPair').textContent=d.pair;
  document.getElementById('btnAbout').textContent=d.about;
  document.getElementById('btnLang').textContent=d.langBtn;
  document.getElementById('themeBtn').textContent=d.theme;
  // About Modal
  document.getElementById('aboutTitle').textContent=d.aboutTitle;
  document.getElementById('aboutAuthor').textContent=d.author;
  document.getElementById('aboutLicense').textContent=d.license;
  document.getElementById('aboutDev').textContent=d.dev;
  document.getElementById('aboutDevText').textContent=d.devText;
  document.getElementById('aboutClose').textContent=d.aboutTitle;
  // Auto Mode Panel
  document.getElementById('lblAutoMode').textContent=d.autoMode;
  document.getElementById('lblInterval').textContent=d.interval;
  document.getElementById('lblHold').textContent=d.hold;
  document.getElementById('lblWF').textContent=d.wForward;
  document.getElementById('lblWS').textContent=d.wBack;
  document.getElementById('lblWA').textContent=d.wLeft;
  document.getElementById('lblWD').textContent=d.wRight;
  document.getElementById('lblWTL').textContent=d.tLeft;
  document.getElementById('lblWTR').textContent=d.tRight;
  document.getElementById('lblWSP').textContent=d.jump;
  document.getElementById('lblWC').textContent=d.crouch;
  document.getElementById('lblWZ').textContent=d.prone;
  document.getElementById('lblWId').textContent=d.idle;
  document.getElementById('lblTotal').textContent=d.total;
  document.getElementById('btnApply').textContent=d.apply;
  document.getElementById('btnSaveTo').textContent=d.saveTo;
  // Config Manager
  document.getElementById('lblConfigMgr').textContent=d.configMgr;
  document.getElementById('btnExportCurr').textContent=d.exportCurr;
  document.getElementById('lblImportCurrText').textContent=d.importCurr;
  // Key Log & Stats
  document.getElementById('lblKeyLog').textContent=d.keyLog;
  document.getElementById('lblKeyStats').textContent=d.keyStats;
  document.getElementById('btnClearLog').textContent=d.clear;
  document.getElementById('btnResetStats').textContent=d.reset;
  document.getElementById('lblKeyCol').textContent=d.keyCol;
  document.getElementById('lblCountCol').textContent=d.countCol;
  document.getElementById('lblPctCol').textContent=d.pctCol;
  document.getElementById('lblTotalRow').textContent=d.total;
  document.getElementById('lblWaitLog').textContent=d.waitLog;
  // Slot Modal
  document.getElementById('slotModalTitle').textContent=d.saveSlotTitle;
  document.getElementById('slotModalSub').textContent=d.saveSlotSub;
  document.getElementById('btnSlotCancel').textContent=d.clear?d.clear:'Cancel';
  // Stats labels
  statsKeyLabels=[d.wForward,d.wBack,d.wLeft,d.wRight,d.tLeft,d.tRight,d.jump,d.crouch,d.prone,d.idle];
  renderStats();
}
(function(){var s=localStorage.getItem('lang');if(s){lang=s;applyLang();}})();

loadCfg();updStatus();setInterval(updStatus,1000);setInterval(pollEvents,500);updStats();setInterval(updStats,2000);loadSlots();
)rawliteral";
}
