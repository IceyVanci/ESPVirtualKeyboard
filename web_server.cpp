#include "web_server.h"
#include "ble_keyboard.h"
#include "auto_mode.h"
#include "config_manager.h"
#include "seq_mode.h"
#include "keymap.h"
#include <esp_system.h>

// 内嵌 Web 资源（PROGMEM，定义见文件后部；此处仅声明以便 handleRoot 使用）
extern const char WEB_CSS[] PROGMEM;
extern const char WEB_HTML_A[] PROGMEM;
extern const char WEB_HTML_B[] PROGMEM;
extern const char WEB_HTML_C[] PROGMEM;
extern const char WEB_JS_MAIN[] PROGMEM;
extern const char WEB_JS_TAIL_A[] PROGMEM;
extern const char WEB_JS_TAIL_B[] PROGMEM;
#ifdef ENABLE_WEB_AUTH
extern const char WEB_HTML_AUTH_MODALS[] PROGMEM;
extern const char WEB_AUTH_BTNS[] PROGMEM;
extern const char WEB_JS_AUTH[] PROGMEM;
#endif

WebController::WebController(BleKeyboard* keyboard, AutoMode* autoMode, ConfigManager* configMgr, SequenceMode* seqMode)
  : _server(nullptr), _keyboard(keyboard), _autoMode(autoMode), _configMgr(configMgr), _seqMode(seqMode) {
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
  _server->on("/api/ble/name", HTTP_ANY, [this]() { handleBleName(); });
#ifdef ENABLE_WEB_AUTH
  // 认证
  _server->on("/api/login", HTTP_POST, [this]() { handleLogin(); });
  _server->on("/api/logout", HTTP_POST, [this]() { handleLogout(); });
  _server->on("/api/auth/change", HTTP_POST, [this]() { handleAuthChange(); });
#endif
  // 配置槽位管理
  _server->on("/api/slots", HTTP_GET, [this]() { handleSlots(); });
  _server->on("/api/slot/save", HTTP_POST, [this]() { handleSlotSave(); });
  _server->on("/api/slot/load", HTTP_POST, [this]() { handleSlotLoad(); });
  _server->on("/api/slot/delete", HTTP_POST, [this]() { handleSlotDelete(); });
  _server->on("/api/slot/export", HTTP_POST, [this]() { handleSlotExport(); });
  _server->on("/api/slot/import", HTTP_POST, [this]() { handleSlotImport(); });
  _server->on("/api/config/export", HTTP_GET, [this]() { handleConfigExport(); });
  _server->on("/api/config/import", HTTP_POST, [this]() { handleConfigImport(); });
  // 顺序模式
  _server->on("/api/seq/config", HTTP_ANY, [this]() { handleSeqConfig(); });
  _server->on("/api/seq/play", HTTP_POST, [this]() { handleSeqPlay(); });
  _server->on("/api/seq/slots", HTTP_GET, [this]() { handleSeqSlots(); });
  _server->on("/api/seq/slot/save", HTTP_POST, [this]() { handleSeqSlotSave(); });
  _server->on("/api/seq/slot/load", HTTP_POST, [this]() { handleSeqSlotLoad(); });
  _server->on("/api/seq/slot/delete", HTTP_POST, [this]() { handleSeqSlotDelete(); });
  _server->on("/api/seq/slot/import", HTTP_POST, [this]() { handleSeqSlotImport(); });
  _server->on("/api/seq/slot/export", HTTP_POST, [this]() { handleSeqSlotExport(); });
  // 全部导出
  _server->on("/api/config/export-all", HTTP_GET, [this]() { handleExportAll(); });
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
  _server->sendContent("<style>");
  _server->sendContent_P(WEB_CSS);
  _server->sendContent("</style>");
  _server->sendContent("</head><body>");
  _server->sendContent_P(WEB_HTML_A);
#ifdef ENABLE_WEB_AUTH
  _server->sendContent_P(WEB_AUTH_BTNS);
#endif
  _server->sendContent_P(WEB_HTML_B);
  _server->sendContent_P(WEB_HTML_C);
#ifdef ENABLE_WEB_AUTH
  _server->sendContent_P(WEB_HTML_AUTH_MODALS);
#endif
  _server->sendContent("<div class=\"footer\">ESP Virtual Keyboard " FW_VERSION " · Build " FW_BUILD_DATE " " FW_BUILD_TIME "</div>");
  _server->sendContent("<script>");
  _server->sendContent_P(WEB_JS_MAIN);
#ifdef ENABLE_WEB_AUTH
  _server->sendContent_P(WEB_JS_AUTH);
#endif
  _server->sendContent_P(WEB_JS_TAIL_A);
  _server->sendContent(String(DEFAULT_KB_ONLY_MODE));
  _server->sendContent_P(WEB_JS_TAIL_B);
  _server->sendContent("</script>");
  _server->sendContent("</body></html>");
}

void WebController::handleKeyPress() {
  if (!authGuard()) return;
  if (!_server->hasArg("key")) { _server->send(400, "application/json", "{\"error\":\"no key\"}"); return; }
  uint8_t k = mapWebKeyToHid(_server->arg("key"));
  if (k == 0xFF) { _server->send(400, "application/json", "{\"error\":\"unknown\"}"); return; }
  _keyboard->press(k);
  _server->send(200, "application/json", "{\"ok\":true}");
}

void WebController::handleKeyRelease() {
  if (!authGuard()) return;
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
    if (!authGuard()) return;
    AutoModeConfig c = _autoMode->getConfig();
    if (_server->hasArg("enabled")) c.enabled = _server->arg("enabled")=="true";
    // 互斥：开启自动模式时停止顺序模式播放
    if (c.enabled && _seqMode) _seqMode->setPlaying(false);
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
    ",\"seqPlaying\":"+String(_seqMode && _seqMode->isPlaying()?"true":"false")+
    ",\"currentKey\":\""+ck+"\""+
    ",\"ip\":\""+getLocalIP()+"\",\"uptime\":"+String(millis()/1000)+
    ",\"epoch\":" + String((uint32_t)now_t) + "}");
}

void WebController::handleBleReboot() {
  if (!authGuard()) return;
  _keyboard->disconnectAndReboot();
  _server->send(200, "application/json", "{\"ok\":true,\"msg\":\"已断开并重新广播\"}");
}

void WebController::handleBleName() {
  if (_server->method() == HTTP_GET) {
    String name = _configMgr ? _configMgr->getBleName() : String(BLE_DEVICE_NAME);
    _server->send(200, "application/json", "{\"name\":\"" + name + "\"}");
    return;
  }
  if (!authGuard()) return;
  if (!_server->hasArg("name")) {
    _server->send(400, "application/json", "{\"error\":\"missing name\"}");
    return;
  }
  String name = _server->arg("name");
  if (!_configMgr || !_configMgr->setBleName(name)) {
    _server->send(400, "application/json", "{\"error\":\"invalid length\"}");
    return;
  }
  // 保存成功后重启 BLE，使新名称生效（当前连接会被断开）
  _keyboard->setDeviceName(_configMgr->getBleName());
  _keyboard->end();
  _keyboard->begin();
  _server->send(200, "application/json", "{\"ok\":true,\"msg\":\"已保存，请重新配对\"}");
}

// ========== 认证（ENABLE_WEB_AUTH 启用时生效） ==========

bool WebController::authGuard() {
#ifdef ENABLE_WEB_AUTH
  if (_authToken.length() == 0 || !_server->hasArg("token") || _server->arg("token") != _authToken) {
    _server->send(401, "application/json", "{\"error\":\"unauthorized\"}");
    return false;
  }
#endif
  return true;
}

void WebController::handleLogin() {
#ifdef ENABLE_WEB_AUTH
  unsigned long now = millis();
  if (now < _authLockUntil) {
    _server->send(429, "application/json", "{\"error\":\"locked\",\"retry\":" + String((_authLockUntil - now) / 1000) + "}");
    return;
  }
  if (!_server->hasArg("user") || !_server->hasArg("pass")) {
    _server->send(400, "application/json", "{\"error\":\"missing user or pass\"}");
    return;
  }
  String user = _server->arg("user");
  String pass = _server->arg("pass");
  if (user == _configMgr->getAuthUser() && ConfigManager::sha256Hex(pass) == _configMgr->getAuthPass()) {
    _authFailCount = 0;
    _authToken = generateToken();
    _server->send(200, "application/json", "{\"ok\":true,\"token\":\"" + _authToken + "\"}");
  } else {
    _authFailCount++;
    if (_authFailCount >= WEB_AUTH_LOCKOUT_THRESHOLD) {
      _authLockUntil = now + WEB_AUTH_LOCKOUT_MS;
      _authFailCount = 0;
    }
    _server->send(401, "application/json", "{\"error\":\"invalid\"}");
  }
#else
  _server->send(404, "application/json", "{\"error\":\"not found\"}");
#endif
}

void WebController::handleLogout() {
#ifdef ENABLE_WEB_AUTH
  if (!authGuard()) return;
  _authToken = "";
  _server->send(200, "application/json", "{\"ok\":true}");
#else
  _server->send(404, "application/json", "{\"error\":\"not found\"}");
#endif
}

void WebController::handleAuthChange() {
#ifdef ENABLE_WEB_AUTH
  unsigned long now = millis();
  if (now < _authLockUntil) {
    _server->send(429, "application/json", "{\"error\":\"locked\",\"retry\":" + String((_authLockUntil - now) / 1000) + "}");
    return;
  }
  if (!_server->hasArg("oldUser") || !_server->hasArg("oldPass") ||
      !_server->hasArg("newUser") || !_server->hasArg("newPass")) {
    _server->send(400, "application/json", "{\"error\":\"missing fields\"}");
    return;
  }
  String oldUser = _server->arg("oldUser");
  String oldPass = _server->arg("oldPass");
  if (oldUser == _configMgr->getAuthUser() && ConfigManager::sha256Hex(oldPass) == _configMgr->getAuthPass()) {
    if (_configMgr->setAuthCredentials(_server->arg("newUser"), _server->arg("newPass"))) {
      _authToken = "";
      _server->send(200, "application/json", "{\"ok\":true}");
    } else {
      _server->send(400, "application/json", "{\"error\":\"invalid length\"}");
    }
  } else {
    _authFailCount++;
    if (_authFailCount >= WEB_AUTH_LOCKOUT_THRESHOLD) {
      _authLockUntil = now + WEB_AUTH_LOCKOUT_MS;
      _authFailCount = 0;
    }
    _server->send(401, "application/json", "{\"error\":\"invalid\"}");
  }
#else
  _server->send(404, "application/json", "{\"error\":\"not found\"}");
#endif
}

#ifdef ENABLE_WEB_AUTH
String WebController::generateToken() {
  const char* hex = "0123456789abcdef";
  String t = "";
  for (int i = 0; i < 16; i++) {
    t += String(hex[esp_random() & 0x0F]);
  }
  return t;
}
#endif

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
  return webKeyToHid(key);
}

// ========== 配置槽位管理 API ==========

void WebController::handleSlots() {
  if (!_configMgr) { _server->send(500, "application/json", "{\"error\":\"config manager not available\"}"); return; }
  String j = "{\"slots\":[";
  for (int i = 0; i < SLOT_COUNT; i++) {
    if (i > 0) j += ",";
    SlotSummary s = _configMgr->getSlotSummary(i);
    String cfg = "";
    if (s.used) {
      AutoModeConfig c;
      String n;
      if (_configMgr->loadSlot(i, c, n)) {
        cfg = _configMgr->exportConfig(c, s.name);
      }
    }
    j += "{\"index\":" + String(i);
    j += ",\"used\":" + String(s.used ? "true" : "false");
    j += ",\"name\":\"" + s.name + "\"";
    j += ",\"config\":" + (cfg.length() > 0 ? cfg : "null");
    j += "}";
  }
  j += "],\"active\":" + String(_configMgr->getActiveSlot()) + "}";
  _server->send(200, "application/json", j);
}

void WebController::handleSeqSlots() {
  if (!_configMgr) { _server->send(500, "application/json", "{\"error\":\"config manager not available\"}"); return; }
  String j = "{\"slots\":[";
  for (int i = 0; i < SLOT_COUNT; i++) {
    if (i > 0) j += ",";
    bool used = _configMgr->isSeqSlotUsed(i);
    String nm = used ? _configMgr->getSeqSlotName(i) : "";
    String cfg = "";
    if (used) {
      SeqConfig c;
      String n;
      if (_configMgr->loadSeqSlot(i, c, n)) {
        cfg = _configMgr->seqConfigToJson(c, nm);
      }
    }
    j += "{\"index\":" + String(i);
    j += ",\"used\":" + String(used ? "true" : "false");
    j += ",\"name\":\"" + nm + "\"";
    j += ",\"config\":" + (cfg.length() > 0 ? cfg : "null");
    j += "}";
  }
  j += "],\"active\":" + String(_configMgr->getActiveSeqSlot()) + "}";
  _server->send(200, "application/json", j);
}

void WebController::handleSlotSave() {
  if (!authGuard()) return;
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
  if (!authGuard()) return;
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
  if (!authGuard()) return;
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
  if (!authGuard()) return;
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
  if (!authGuard()) return;
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

// ========== 顺序模式 API ==========

void WebController::handleSeqConfig() {
  if (!_seqMode) { _server->send(500, "application/json", "{\"error\":\"seq mode not available\"}"); return; }
  if (_server->method() == HTTP_GET) {
    SeqConfig c = _seqMode->getConfig();
    String name = "";
    if (_configMgr) {
      int active = _configMgr->getActiveSeqSlot();
      if (active >= 0) name = _configMgr->getSeqSlotName(active);
    }
    _server->send(200, "application/json", _configMgr ? _configMgr->seqConfigToJson(c, name) : "{}");
    return;
  }
  if (!authGuard()) return;
  if (!_server->hasArg("json")) {
    _server->send(400, "application/json", "{\"error\":\"missing json\"}");
    return;
  }
  SeqConfig c;
  String name;
  if (!_configMgr || !_configMgr->seqJsonToConfig(_server->arg("json"), c, name)) {
    _server->send(400, "application/json", "{\"error\":\"invalid seq JSON\"}");
    return;
  }
  _seqMode->setConfig(c);
  _server->send(200, "application/json", "{\"ok\":true}");
}

void WebController::handleSeqPlay() {
  if (!authGuard()) return;
  if (!_seqMode) { _server->send(500, "application/json", "{\"error\":\"seq mode not available\"}"); return; }
  if (!_server->hasArg("state")) {
    _server->send(400, "application/json", "{\"error\":\"missing state\"}");
    return;
  }
  bool on = _server->arg("state") == "on";
  if (on) {
    // 互斥：开启顺序播放时停止自动模式
    _autoMode->setEnabled(false);
    _seqMode->setPlaying(true);
  } else {
    _seqMode->setPlaying(false);
  }
  _server->send(200, "application/json", "{\"ok\":true}");
}

void WebController::handleSeqSlotSave() {
  if (!authGuard()) return;
  if (!_configMgr || !_seqMode) { _server->send(500, "application/json", "{\"error\":\"not available\"}"); return; }
  if (!_server->hasArg("slot") || !_server->hasArg("name")) {
    _server->send(400, "application/json", "{\"error\":\"missing slot or name\"}");
    return;
  }
  int slot = _server->arg("slot").toInt();
  if (slot < 0 || slot >= SLOT_COUNT) {
    _server->send(400, "application/json", "{\"error\":\"invalid slot\"}");
    return;
  }
  SeqConfig c = _seqMode->getConfig();
  if (c.stepCount == 0) {
    _server->send(400, "application/json", "{\"error\":\"empty sequence\"}");
    return;
  }
  if (!_configMgr->saveSeqSlot(slot, _server->arg("name"), c)) {
    _server->send(500, "application/json", "{\"error\":\"save failed\"}");
    return;
  }
  _configMgr->setActiveSeqSlot(slot);
  _server->send(200, "application/json", "{\"ok\":true}");
}

void WebController::handleSeqSlotLoad() {
  if (!authGuard()) return;
  if (!_configMgr || !_seqMode) { _server->send(500, "application/json", "{\"error\":\"not available\"}"); return; }
  if (!_server->hasArg("slot")) {
    _server->send(400, "application/json", "{\"error\":\"missing slot\"}");
    return;
  }
  int slot = _server->arg("slot").toInt();
  SeqConfig c;
  String name;
  if (!_configMgr->loadSeqSlot(slot, c, name)) {
    _server->send(404, "application/json", "{\"error\":\"slot empty or invalid\"}");
    return;
  }
  _seqMode->setConfig(c);
  _configMgr->setActiveSeqSlot(slot);
  _server->send(200, "application/json", "{\"ok\":true,\"name\":\"" + name + "\"}");
}

void WebController::handleSeqSlotDelete() {
  if (!authGuard()) return;
  if (!_configMgr) { _server->send(500, "application/json", "{\"error\":\"not available\"}"); return; }
  if (!_server->hasArg("slot")) {
    _server->send(400, "application/json", "{\"error\":\"missing slot\"}");
    return;
  }
  int slot = _server->arg("slot").toInt();
  _server->send(200, "application/json", _configMgr->deleteSeqSlot(slot) ? "{\"ok\":true}" : "{\"error\":\"delete failed\"}");
}

void WebController::handleSeqSlotImport() {
  if (!authGuard()) return;
  if (!_configMgr) { _server->send(500, "application/json", "{\"error\":\"not available\"}"); return; }
  if (!_server->hasArg("slot") || !_server->hasArg("json")) {
    _server->send(400, "application/json", "{\"error\":\"missing slot or json\"}");
    return;
  }
  int slot = _server->arg("slot").toInt();
  if (slot < 0 || slot >= SLOT_COUNT) {
    _server->send(400, "application/json", "{\"error\":\"invalid slot\"}");
    return;
  }
  SeqConfig c;
  String name;
  if (!_configMgr->seqJsonToConfig(_server->arg("json"), c, name)) {
    _server->send(400, "application/json", "{\"error\":\"invalid seq JSON\"}");
    return;
  }
  _configMgr->saveSeqSlot(slot, name, c);
  _server->send(200, "application/json", "{\"ok\":true}");
}

void WebController::handleSeqSlotExport() {
  if (!_configMgr) { _server->send(500, "application/json", "{\"error\":\"config manager not available\"}"); return; }
  if (!_server->hasArg("slot")) {
    _server->send(400, "application/json", "{\"error\":\"missing slot\"}");
    return;
  }
  int slot = _server->arg("slot").toInt();
  SeqConfig c;
  String n;
  if (!_configMgr->loadSeqSlot(slot, c, n)) {
    _server->send(404, "application/json", "{\"error\":\"slot empty\"}");
    return;
  }
  String json = _configMgr->seqConfigToJson(c, n);
  String safeName = n.length() > 0 ? n : "seq" + String(slot);
  _server->sendHeader("Content-Disposition", "attachment; filename=\"" + safeName + ".json\"");
  _server->send(200, "application/json", json);
}

void WebController::handleExportAll() {
  if (!_configMgr) { _server->send(500, "application/json", "{\"error\":\"config manager not available\"}"); return; }
  String json = _configMgr->exportAllConfigs();
  _server->sendHeader("Content-Disposition", "attachment; filename=\"espvk-configs-all.json\"");
  _server->send(200, "application/json", json);
}

// ========== HTML ==========

const char WEB_HTML_A[] PROGMEM = R"rawliteral(
<div class="top-bar">
  <div class="brand">ESP Virtual Keyboard</div>
    <div class="stats">
    <span id="bleSt" class="tag tag-off">BLE: --</span>
    <span id="autoSt" class="tag tag-off">AUTO: OFF</span>
    <span id="upSt" class="tag">00:00:00</span>
    <span id="clockSt" class="tag">--:--:--</span>
    <button class="top-btn" onclick="toggleKbMode()" id="btnMode">🎹 键盘模式</button>
    <button class="top-btn" onclick="togglePanelMode()" id="btnPanelMode">🔁 顺序</button>
    <button class="top-btn" onclick="openBleNameModal()" id="btnBleName">📡 蓝牙名</button>
    <button class="top-btn" onclick="exportAll()" id="btnExportAll">📤 全部导出</button>
    <label class="top-btn import-label" id="lblImportAll"><span id="lblImportAllText">📥 全部导入</span><input type="file" accept=".json" onchange="importAll(this)" style="display:none"></label>
)rawliteral";

const char WEB_HTML_B[] PROGMEM = R"rawliteral(
    <button class="top-btn top-btn-warn" onclick="bleReboot()" id="btnPair">配对</button>
    <button class="theme-btn" onclick="toggleTheme()" id="themeBtn">明暗</button>
    <button class="top-btn" onclick="toggleLang()" id="btnLang">En</button>
    <button class="top-btn" onclick="showAbout()" id="btnAbout">关于</button>
  </div>
</div>

<div class="main">
  <!-- 控制面板 -->
  <div class="ctrl-grid">
    <div class="ctrl-card" id="autoCard">
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
          <button class="btn btn-green btn-half" onclick="saveToSlot('auto')" id="btnSaveTo">💾 保存到配置</button>
        </div>
      </div>
    </div>

    <!-- 顺序模式卡片 -->
    <div class="ctrl-card" id="seqCard" style="display:none">
      <div class="ctrl-title" onclick="togglePanel('seqPanel')"><span id="lblSeqMode">🔁 顺序模式</span> <span class="arrow">▾</span></div>
      <div id="seqPanel" class="ctrl-body">
        <div class="seq-status" id="seqStatus">空闲</div>
        <div class="btn-row">
          <button class="btn btn-green btn-half" onclick="seqRecToggle()" id="seqRecBtn">▶ 开始录制</button>
          <button class="btn btn-blue btn-half" onclick="seqPlay()" id="seqPlayBtn">▶ 播放</button>
        </div>
        <div class="seq-opts">
          <label class="seq-chk"><input type="checkbox" id="seqLoop" onchange="seqUiChanged()"><span id="lblSeqLoop">🔁 循环</span></label>
          <label class="seq-chk"><input type="checkbox" id="seqRecSend" checked><span id="lblSeqRecSend">录制时发送</span></label>
          <label class="seq-chk"><span id="lblSeqLoopGap">循环周期</span><input type="number" id="seqLoopGap" min="0" max="10000" value="1000" style="width:64px" onchange="seqUiChanged()">ms</label>
        </div>
        <div class="seq-toolbar">
          <span class="seq-tb-label"><span id="lblSeqSteps">步骤</span> <span id="seqStepCount">0</span></span>
          <button class="btn btn-warn btn-sm" onclick="seqInsert()" id="btnSeqInsert">➕ 插入</button>
        </div>
        <div id="seqStepsBox" class="seq-steps"><div class="slot-empty" id="lblSeqEmpty">暂无步骤，点击开始录制</div></div>
        <div class="btn-row">
          <button class="btn btn-blue btn-half" onclick="seqApply()" id="btnSeqApply">✅ 应用</button>
          <button class="btn btn-green btn-half" onclick="saveToSlot('seq')" id="btnSeqSaveTo">💾 保存到栏位</button>
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
    <div class="kb-inner" id="kbInner">
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

const char WEB_HTML_C[] PROGMEM = R"rawliteral(
<div id="bleNameModal" class="modal-overlay" style="display:none">
  <div class="modal-card">
    <div class="modal-title" id="bleNameTitle">📡 蓝牙名</div>
    <div class="modal-subtitle" id="bleNameSub">修改后 BLE 将重启，需重新配对</div>
    <input id="bleNameInput" class="modal-input" maxlength="24" placeholder="ESP Virtual Keyboard">
    <div class="btn-row">
      <button class="btn btn-blue btn-half" onclick="doSaveBleName()" id="bleNameSaveBtn">保存</button>
      <button class="btn btn-red btn-half" onclick="closeBleNameModal()" id="bleNameCancelBtn">取消</button>
    </div>
  </div>
</div>

<!-- 导入全部配置：逐项选择目标 -->
<div id="importModal" class="modal-overlay" style="display:none">
  <div class="modal-card" style="width:400px">
    <div class="modal-title" id="impTitle">导入配置</div>
    <div class="modal-subtitle" id="impSub">逐项选择导入目标</div>
    <div id="impBody"></div>
    <div class="btn-row">
      <button class="btn btn-blue btn-half" onclick="impNext(true)" id="impImportBtn">导入</button>
      <button class="btn btn-warn btn-half" onclick="impNext(false)" id="impSkipBtn">跳过</button>
    </div>
  </div>
</div>
)rawliteral";

#ifdef ENABLE_WEB_AUTH
const char WEB_HTML_AUTH_MODALS[] PROGMEM = R"rawliteral(
<div id="loginModal" class="modal-overlay" style="display:none">
  <div class="modal-card">
    <div class="modal-title">登录</div>
    <div class="modal-subtitle">输入管理凭据以控制设备</div>
    <input id="loginUser" class="modal-input" placeholder="用户名" autocomplete="username">
    <input id="loginPass" class="modal-input" type="password" placeholder="密码" autocomplete="current-password">
    <div class="btn-row">
      <button class="btn btn-blue btn-half" onclick="doLogin()">登录</button>
      <button class="btn btn-red btn-half" onclick="closeLogin()">取消</button>
    </div>
  </div>
</div>

<div id="pwdModal" class="modal-overlay" style="display:none">
  <div class="modal-card">
    <div class="modal-title">修改密码</div>
    <div class="modal-subtitle">需验证当前凭据</div>
    <input id="pwdOldUser" class="modal-input" placeholder="当前用户名" autocomplete="username">
    <input id="pwdOldPass" class="modal-input" type="password" placeholder="当前密码" autocomplete="current-password">
    <input id="pwdNewUser" class="modal-input" placeholder="新用户名" autocomplete="username">
    <input id="pwdNewPass" class="modal-input" type="password" placeholder="新密码" autocomplete="new-password">
    <div class="btn-row">
      <button class="btn btn-blue btn-half" onclick="doChangePwd()">确认修改</button>
      <button class="btn btn-red btn-half" onclick="closePwdModal()">取消</button>
    </div>
  </div>
</div>
)rawliteral";
#endif

#ifdef ENABLE_WEB_AUTH
const char WEB_AUTH_BTNS[] PROGMEM =
  "<button class=\"top-btn\" onclick=\"toggleLogin()\" id=\"btnLogin\" title=\"登录\">🔒</button>"
  "<button class=\"top-btn\" onclick=\"openPwdModal()\" id=\"btnChgPwd\" title=\"修改密码\">🔑 修改密码</button>";
#endif

// ========== CSS ==========

const char WEB_CSS[] PROGMEM = R"rawliteral(
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
.kb-wrap{background:var(--card);border:1px solid var(--border);border-radius:12px;padding:16px;display:flex;justify-content:center;align-items:flex-start;overflow-x:auto}
.kb-inner{display:flex;align-items:flex-start;gap:14px;flex-shrink:0;transform-origin:center top;will-change:transform}
body.kb-mode .ctrl-grid,body.kb-mode .log-stats-grid{display:none}
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
.modal-input{width:100%;padding:8px 10px;border-radius:8px;border:1px solid var(--border);background:var(--key-bg);color:var(--text);margin-bottom:8px;font-size:0.85em}
.modal-input:focus{outline:none;border-color:var(--accent)}
.seq-status{padding:6px 10px;font-size:0.85em;color:var(--accent);font-weight:600;background:rgba(88,166,255,0.08);border-radius:8px;margin:8px 0;text-align:center}
.seq-opts{display:flex;flex-wrap:wrap;gap:8px;align-items:center;margin:6px 0;font-size:0.78em;color:var(--dim)}
.seq-chk{display:inline-flex;align-items:center;gap:4px}
.seq-chk input[type=number]{accent-color:var(--accent);background:var(--key-bg);border:1px solid var(--border);color:var(--text);border-radius:6px;padding:2px 4px}
.seq-toolbar{display:flex;justify-content:space-between;align-items:center;margin:6px 0}
.seq-tb-label{font-size:0.8em;color:var(--dim)}
.seq-steps{max-height:220px;overflow-y:auto;border:1px solid var(--border);border-radius:8px;padding:6px;margin-bottom:6px}
.seq-row{display:flex;align-items:center;gap:4px;padding:3px 2px;border-bottom:1px solid rgba(48,54,61,0.4)}
.seq-row:last-child{border-bottom:none}
.seq-row input{background:var(--key-bg);border:1px solid var(--border);color:var(--text);border-radius:6px;padding:3px 5px;font-size:0.78em}
.seq-key{width:64px}
.seq-t{width:64px}
.seq-mini{background:var(--key-bg);border:1px solid var(--border);color:var(--text);border-radius:6px;width:24px;height:22px;font-size:0.7em;cursor:pointer;flex-shrink:0}
.seq-mini:hover{background:var(--key-h)}
.seq-mini.seq-del{color:var(--red)}
.footer{max-width:1200px;margin:12px auto 0;padding:10px 12px;font-size:0.72em;color:var(--dim);text-align:center;border-top:1px solid var(--border)}
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

// ========== JavaScript ==========

const char WEB_JS_MAIN[] PROGMEM = R"rawliteral(
var autoOn=false;
var seqPlaying=false;

function kbDown(key){
  if(seqRec)seqRecPress(key);
  if(!seqRec||seqRecSend)fetch('/api/press?key='+key);
}
function kbUp(key){
  if(seqRec)seqRecRelease(key);
  if(!seqRec||seqRecSend)fetch('/api/release?key='+key);
}

document.querySelectorAll('.k[data-k]').forEach(function(k){
  var key=k.dataset.k;
  k.addEventListener('mousedown',function(e){e.preventDefault();kbDown(key);k.classList.add('on');});
  k.addEventListener('mouseup',function(e){e.preventDefault();kbUp(key);k.classList.remove('on');});
  k.addEventListener('mouseleave',function(e){kbUp(key);k.classList.remove('on');});
  k.addEventListener('touchstart',function(e){e.preventDefault();kbDown(key);k.classList.add('on');},{passive:false});
  k.addEventListener('touchend',function(e){e.preventDefault();kbUp(key);k.classList.remove('on');},{passive:false});
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
function saveToSlot(mode){
  window._slotSaveMode=mode||'auto';
  var url=mode==='seq'?'/api/seq/slots':'/api/slots';
  document.getElementById('slotModalTitle').textContent=mode==='seq'?L('seqSaveToTitle'):L('saveSlotTitle');
  document.getElementById('slotModalSub').textContent=mode==='seq'?L('seqSaveToSub'):L('saveSlotSub');
  fetch(url).then(function(r){return r.json()}).then(function(d){
    window._slotModalData=d;
    var slots=d.slots||[];var active=d.active;
    var html='';
    if(active>=0){
      var sn=slots[active]?slots[active].name:L('slotN')+(active+1);
      html+='<div class="modal-slot active" onclick="confirmSlotSave('+active+',true)"><span class="slot-num">★</span><span class="slot-name">'+L('overwriteCur')+': '+sn+'</span><span class="slot-badge">'+L('current')+'</span></div>';
    }
    for(var i=0;i<slots.length;i++){
      var s=slots[i];
      var nm=s.used?s.name:L('emptySlot');
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
  var mode=window._slotSaveMode==='seq'?'seq':'auto';
  var slots=window._slotModalData?window._slotModalData.slots:[];
  var defaultName=slots[idx]&&slots[idx].used?slots[idx].name:L('configN')+(idx+1);
  var name=isOverride?defaultName:prompt(L('enterName'),defaultName);
  if(name===null)return;
  if(!name.trim())name=defaultName;
  var applyPromise;
  if(mode==='seq'){
    seqUiChanged();
    applyPromise=fetch('/api/seq/config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'json='+encodeURIComponent(JSON.stringify(seqData))});
  }else{
    applyPromise=fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:getCfgBody()});
  }
  applyPromise.then(function(){return fetch(mode==='seq'?'/api/seq/slot/save':'/api/slot/save',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'slot='+idx+'&name='+encodeURIComponent(name.trim())})}).then(function(r){return r.json()}).then(function(d){
    if(d.ok){loadSlots();alert(mode==='seq'?L('seqSavedTo')+(idx+1):L('saved').replace('%d',idx+1));}else{alert(d.error||L('saveFail'));}
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
    seqPlaying=!!d.seqPlaying;
    updSeqStatus();
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

// ---- 键盘模式切换与自适应缩放 ----
function isKbMode(){return document.body.classList.contains('kb-mode');}
function updateModeBtn(){
  var b=document.getElementById('btnMode');
  if(b)b.textContent=isKbMode()?L('modeFull'):L('modeKb');
}
function setKbMode(on){
  document.body.classList.toggle('kb-mode',on);
  localStorage.setItem('kbMode',on?'1':'0');
  updateModeBtn();
  fitKeyboard();
}
function toggleKbMode(){
  setKbMode(!isKbMode());
}
function fitKeyboard(){
  var wrap=document.querySelector('.kb-wrap');
  var inner=document.getElementById('kbInner');
  if(!wrap||!inner)return;
  var cs=getComputedStyle(wrap);
  var pad=(parseFloat(cs.paddingLeft)||0)+(parseFloat(cs.paddingRight)||0);
  var avail=wrap.clientWidth-pad;
  var natW=inner.scrollWidth;
  if(natW<=0||avail<=0)return;
  var s=avail/natW;
  if(s>=1){inner.style.transform='';wrap.style.height='';return;}
  inner.style.transform='scale('+s+')';
  wrap.style.height=(inner.offsetHeight*s)+'px';
}

// ---- 蓝牙名称设置 ----
function openBleNameModal(){
  fetch('/api/ble/name').then(function(r){return r.json()}).then(function(d){
    document.getElementById('bleNameInput').value=d.name||'';
    document.getElementById('bleNameModal').style.display='flex';
  }).catch(function(){});
}
function closeBleNameModal(){document.getElementById('bleNameModal').style.display='none';}
function doSaveBleName(){
  var n=document.getElementById('bleNameInput').value.trim();
  if(!n){alert(L('bleNameInvalid'));return;}
  fetch('/api/ble/name',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'name='+encodeURIComponent(n)}).then(function(r){return r.json()}).then(function(d){
    if(d.ok){closeBleNameModal();alert(d.msg||L('bleNameSaved'));}
    else{alert(L('bleNameInvalid'));}
  }).catch(function(){});
}

// ---- 面板模式（自动/顺序） ----
var panelMode=localStorage.getItem('panelMode')||'auto';
function isSeqMode(){return panelMode==='seq';}
function slotsUrl(){return isSeqMode()?'/api/seq/slots':'/api/slots';}
function slotOp(op){return isSeqMode()?'/api/seq/slot/'+op:'/api/slot/'+op;}
function applyPanelMode(){
  var ac=document.getElementById('autoCard');
  var sc=document.getElementById('seqCard');
  if(ac)ac.style.display=isSeqMode()?'none':'';
  if(sc)sc.style.display=isSeqMode()?'':'none';
  var b=document.getElementById('btnPanelMode');
  if(b)b.textContent=isSeqMode()?L('panelAuto'):L('panelSeq');
  refreshSlots();
}
function togglePanelMode(){
  panelMode=isSeqMode()?'auto':'seq';
  localStorage.setItem('panelMode',panelMode);
  if(isKbMode())setKbMode(false);   // 键盘模式下切换自动/顺序时退出纯键盘视图
  applyPanelMode();
}
function refreshSlots(){loadSlots();}

// ---- 顺序模式（录制/编辑/播放） ----
var seqData={name:'',loop:false,loopGapMs:1000,steps:[]};
var seqRec=false;
var seqRecSend=true;
var seqRecDown=0;
var seqRecLast=0;

function esc(s){return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');}

function seqRecPress(key){
  var now=performance.now();
  var s=seqData.steps;
  if(s.length>0&&seqRecLast>0){s[s.length-1].g=Math.max(10,Math.round(now-seqRecLast));}
  seqRecDown=now;
  s.push({k:key,h:100,g:100});
  renderSeqSteps();
}
function seqRecRelease(key){
  var now=performance.now();
  var s=seqData.steps;
  if(s.length>0){s[s.length-1].h=Math.max(10,Math.round(now-seqRecDown));}
  seqRecLast=now;
  renderSeqSteps();
}
function seqRecToggle(){
  if(seqRec){seqRecStop();}
  else{seqRecStart();}
}
function seqRecStart(){
  if(seqData.steps.length>0&&!confirm(L('seqClearConfirm')))return;
  seqData.steps=[];
  seqRec=true;seqRecDown=0;seqRecLast=0;
  var b=document.getElementById('seqRecBtn');
  b.textContent='⏹ '+L('seqRecordStop');b.className='btn btn-red btn-half';
  renderSeqSteps();updSeqStatus();
  fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'enabled=false'});
}
function seqRecStop(){
  seqRec=false;
  var b=document.getElementById('seqRecBtn');
  b.textContent='▶ '+L('seqRecordStart');b.className='btn btn-green btn-half';
  updSeqStatus();
  if(seqData.steps.length>0)saveToSlot('seq');
  else{alert(L('seqNoSteps'));}
}
function seqUiChanged(){
  seqData.loop=document.getElementById('seqLoop').checked;
  var g=parseInt(document.getElementById('seqLoopGap').value,10);
  if(isNaN(g))g=1000;
  seqData.loopGapMs=Math.max(0,Math.min(10000,g));
}
function seqApply(){
  seqUiChanged();
  return fetch('/api/seq/config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'json='+encodeURIComponent(JSON.stringify(seqData))}).then(function(r){return r.json()}).then(function(d){
    if(d.ok){alert(L('applied'));return d;}
    alert(L('seqInvalid'));return d;
  }).catch(function(){});
}
function seqPlay(){
  if(seqData.steps.length===0){alert(L('seqNoSteps'));return;}
  seqUiChanged();
  fetch('/api/seq/config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'json='+encodeURIComponent(JSON.stringify(seqData))}).then(function(){
    return fetch('/api/seq/play',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'state=on'});
  }).then(function(){updStatus();});
}
function seqStop(){
  fetch('/api/seq/play',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'state=off'}).then(function(){updStatus();});
}
function seqInsert(){
  seqData.steps.push({k:'',h:100,g:100});
  renderSeqSteps();
}
function seqStepField(i,f,v){
  if(i>=seqData.steps.length)return;
  if(f==='h'||f==='g'){var n=parseInt(v,10);if(isNaN(n))n=100;seqData.steps[i][f]=Math.max(10,Math.min(10000,n));}
  else{seqData.steps[i].k=v;}
}
function seqMove(i,d){
  var s=seqData.steps;var j=i+d;
  if(j<0||j>=s.length)return;
  var t=s[i];s[i]=s[j];s[j]=t;renderSeqSteps();
}
function seqDel(i){seqData.steps.splice(i,1);renderSeqSteps();}
function renderSeqSteps(){
  var box=document.getElementById('seqStepsBox');
  var s=seqData.steps;
  var c=document.getElementById('seqStepCount');
  if(c)c.textContent=s.length;
  if(!box)return;
  if(s.length===0){box.innerHTML='<div class="slot-empty">'+L('seqEmpty')+'</div>';return;}
  var html='';
  for(var i=0;i<s.length;i++){
    html+='<div class="seq-row">'
      +'<input class="seq-key" value="'+esc(s[i].k)+'" placeholder="'+L('seqKey')+'" oninput="seqStepField('+i+',\'k\',this.value)">'
      +'<input class="seq-t" type="number" min="10" max="10000" value="'+s[i].h+'" title="'+L('seqHold')+'" oninput="seqStepField('+i+',\'h\',this.value)">'
      +'<input class="seq-t" type="number" min="10" max="10000" value="'+s[i].g+'" title="'+L('seqGap')+'" oninput="seqStepField('+i+',\'g\',this.value)">'
      +'<button class="seq-mini" onclick="seqMove('+i+',-1)">↑</button>'
      +'<button class="seq-mini" onclick="seqMove('+i+',1)">↓</button>'
      +'<button class="seq-mini seq-del" onclick="seqDel('+i+')">✕</button>'
      +'</div>';
  }
  box.innerHTML=html;
}
function updSeqStatus(){
  var el=document.getElementById('seqStatus');
  if(!el)return;
  if(seqRec){el.textContent='● '+L('seqRecOn')+' ('+seqData.steps.length+')';}
  else if(seqPlaying){el.textContent='▶ '+L('seqPlayingNow');}
  else{el.textContent=L('seqIdleNow');}
  var pb=document.getElementById('seqPlayBtn');
  if(pb){pb.textContent=seqPlaying?'⏹ '+L('seqStop'):'▶ '+L('seqPlay');pb.className=seqPlaying?'btn btn-red btn-half':'btn btn-blue btn-half';}
  var rb=document.getElementById('seqRecBtn');
  if(rb){rb.textContent=seqRec?('⏹ '+L('seqRecordStop')):('▶ '+L('seqRecordStart'));rb.className=seqRec?'btn btn-red btn-half':'btn btn-green btn-half';}
}
function loadSeqCfg(){
  fetch('/api/seq/config').then(function(r){return r.json()}).then(function(d){
    if(d&&d.steps){
      seqData={name:d.name||'',loop:!!d.loop,loopGapMs:d.loopGapMs||1000,steps:d.steps||[]};
      document.getElementById('seqLoop').checked=seqData.loop;
      document.getElementById('seqLoopGap').value=seqData.loopGapMs;
      renderSeqSteps();
    }
  }).catch(function(){});
}

// ---- 全部导出 / 全部导入 ----
function exportAll(){
  var form=document.createElement('form');
  form.method='GET';form.action='/api/config/export-all';
  document.body.appendChild(form);form.submit();document.body.removeChild(form);
}
function importAll(input){
  if(!input.files||!input.files[0])return;
  var reader=new FileReader();
  reader.onload=function(e){
    var parsed;
    try{parsed=JSON.parse(e.target.result);}catch(err){alert(L('impInvalid'));return;}
    if(!parsed||!parsed.auto||!parsed.seq){alert(L('impInvalid'));return;}
    Promise.all([fetch('/api/slots').then(function(r){return r.json()}),fetch('/api/seq/slots').then(function(r){return r.json()})]).then(function(res){
      var exA=res[0].slots||[],exS=res[1].slots||[];
      var items=[];
      (parsed.auto||[]).forEach(function(s,i){if(s&&s.used&&s.config)items.push({mode:'auto',name:s.name||('auto'+(i+1)),config:s.config});});
      (parsed.seq||[]).forEach(function(s,i){if(s&&s.used&&s.config)items.push({mode:'seq',name:s.name||('seq'+(i+1)),config:s.config});});
      if(items.length===0){alert(L('impNoItem'));return;}
      var pending=[];
      items.forEach(function(it){
        var c=JSON.stringify(it.config);
        var list=it.mode==='auto'?exA:exS;
        var same=false;
        for(var j=0;j<list.length;j++){if(list[j]&&list[j].config&&JSON.stringify(list[j].config)===c){same=true;break;}}
        if(!same)pending.push(it);
      });
      if(pending.length===0){alert(L('impAllSame'));return;}
      window._impItems=pending;window._impIdx=0;
      impShow();
    }).catch(function(){});
  };
  reader.readAsText(input.files[0]);
  input.value='';
}
function impShow(){
  var it=window._impItems[window._impIdx];
  var html='';
  html+='<div class="imp-item"><span class="imp-lbl">'+L('impName')+'</span><b>'+esc(it.name)+'</b></div>';
  html+='<div class="imp-item"><span class="imp-lbl">'+L('impSource')+'</span>'+(it.mode==='auto'?L('panelAuto'):L('panelSeq'))+'</div>';
  html+='<div class="imp-item"><span class="imp-lbl">'+L('impTargetMode')+'</span><select id="impMode" class="modal-input">'
      +'<option value="auto"'+((it.mode==='auto')?' selected':'')+'>'+L('panelAuto')+'</option>'
      +'<option value="seq"'+((it.mode==='seq')?' selected':'')+'>'+L('panelSeq')+'</option>'
      +'</select></div>';
  html+='<div class="imp-item"><span class="imp-lbl">'+L('impTargetSlot')+'</span><select id="impSlot" class="modal-input">';
  for(var i=1;i<=5;i++){html+='<option value="'+(i-1)+'">'+L('impSlotLbl')+' '+(i)+'</option>';}
  html+='</select></div>';
  html+='<div class="imp-progress">'+(window._impIdx+1)+' / '+window._impItems.length+'</div>';
  document.getElementById('impBody').innerHTML=html;
  document.getElementById('importModal').style.display='flex';
}
function impNext(doImport){
  var it=window._impItems[window._impIdx];
  if(doImport){
    var mode=document.getElementById('impMode').value;
    var slot=document.getElementById('impSlot').value;
    var url=(mode==='auto'?'/api/slot/import':'/api/seq/slot/import');
    fetch(url,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'slot='+slot+'&json='+encodeURIComponent(JSON.stringify(it.config))}).catch(function(){});
  }
  window._impIdx++;
  if(window._impIdx>=window._impItems.length){
    document.getElementById('importModal').style.display='none';
    alert(L('impDone'));
    loadSlots();
  }else{impShow();}
}

// ---- 配置槽位管理 ----
var slotData=[];

function loadSlots(){
  fetch(slotsUrl()).then(function(r){return r.json()}).then(function(d){
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
        html+='<button class="slot-btn load" onclick="slotLoad('+s.index+')">'+L('load')+'</button>';
        html+='<button class="slot-btn del" onclick="slotDelete('+s.index+')">'+L('del')+'</button>';
        html+='<button class="slot-btn exp" onclick="slotExport('+s.index+')">'+L('exp')+'</button>';
        html+='<label class="slot-btn exp import-label">'+L('imp')+'<input type="file" accept=".json" onchange="slotImportFile('+s.index+',this)" style="display:none"></label>';
        html+='</div>';
      }else{
        html+='<span class="slot-name empty">'+L('emptySlot')+'</span>';
        html+='<div class="slot-btns">';
        html+='<button class="slot-btn save" onclick="slotSave('+s.index+')">'+L('save')+'</button>';
        html+='<label class="slot-btn exp import-label">'+L('imp')+'<input type="file" accept=".json" onchange="slotImportFile('+s.index+',this)" style="display:none"></label>';
        html+='</div>';
      }
      html+='</div>';
    }
    if(slotData.length===0)html='<div class="slot-empty">'+L('noSlot')+'</div>';
    list.innerHTML=html;
  }).catch(function(){});
}

function slotSave(idx){
  var name=prompt(L('enterName'),L('configN')+(idx+1));
  if(name===null)return;
  if(!name.trim())name=L('configN')+(idx+1);
  fetch(slotOp('save'),{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'slot='+idx+'&name='+encodeURIComponent(name.trim())}).then(function(r){return r.json()}).then(function(d){
    if(d.ok){loadSlots();if(isSeqMode())loadSeqCfg();else loadCfg();}else{alert(d.error||L('saveFail'));}
  });
}

function slotLoad(idx){
  fetch(slotOp('load'),{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'slot='+idx}).then(function(r){return r.json()}).then(function(d){
    if(d.ok){
      if(isSeqMode())loadSeqCfg();
      else{loadCfg();updSld();updWt();}
      loadSlots();
      alert(L('loaded')+d.name);
    }else{alert(d.error||L('loadFail'));}
  });
}

function slotDelete(idx){
  if(!confirm(L('delConfirm').replace('%d',idx+1)))return;
  fetch(slotOp('delete'),{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'slot='+idx}).then(function(r){return r.json()}).then(function(d){
    if(d.ok){loadSlots();}else{alert(d.error||L('delFail'));}
  });
}

function slotExport(idx){
  var form=document.createElement('form');
  form.method='POST';
  form.action=slotOp('export');
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
    fetch(slotOp('import'),{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'slot='+idx+'&json='+encodeURIComponent(json)}).then(function(r){return r.json()}).then(function(d){
      if(d.ok){loadSlots();alert(L('imported'));}else{alert(d.error||L('importFail'));}
    });
  };
  reader.readAsText(input.files[0]);
  input.value='';
}

function exportCurrent(){
  if(isSeqMode()){
    seqUiChanged();
    var blob=new Blob([JSON.stringify(seqData)],{type:'application/json'});
    var a=document.createElement('a');
    a.href=URL.createObjectURL(blob);
    a.download=(seqData.name||'seq')+'.json';
    a.click();
    setTimeout(function(){URL.revokeObjectURL(a.href);},1000);
    return;
  }
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
    if(isSeqMode()){
      var parsed;
      try{parsed=JSON.parse(json);}catch(err){alert(L('impInvalid'));return;}
      if(!parsed||!parsed.steps){alert(L('impInvalid'));return;}
      seqData={name:parsed.name||'',loop:!!parsed.loop,loopGapMs:parsed.loopGapMs||1000,steps:parsed.steps||[]};
      document.getElementById('seqLoop').checked=seqData.loop;
      document.getElementById('seqLoopGap').value=seqData.loopGapMs;
      renderSeqSteps();
      seqApply();
      return;
    }
    fetch('/api/config/import',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'json='+encodeURIComponent(json)}).then(function(r){return r.json()}).then(function(d){
      if(d.ok){loadCfg();loadSlots();updSld();updWt();alert(L('imported'));}else{alert(d.error||L('importFail'));}
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
      autoOn:'AUTO: ON',autoOff:'AUTO: OFF',
      modeKb:'🎹 键盘模式',modeFull:'🧩 全功能',
      bleName:'📡 蓝牙名',bleNameSub:'修改后 BLE 将重启，需重新配对',
      cancel:'取消',bleNameSaved:'已保存，BLE 已重启，请重新配对',
      bleNameInvalid:'名称需为 1-24 个字符',
      panelAuto:'⚙️ 自动',panelSeq:'🔁 顺序',exportAll:'📤 全部导出',importAll:'📥 全部导入',
      seqTitle:'🔁 顺序模式',seqRecordStart:'开始录制',seqRecordStop:'终止',
      seqPlay:'播放',seqStop:'停止',seqLoop:'🔁 循环',seqRecSend:'录制时发送',
      seqLoopGap:'循环周期',seqSteps:'步骤',seqInsert:'➕ 插入',
      seqKey:'键',seqHold:'时长',seqGap:'间隔',seqEmpty:'暂无步骤，点击开始录制',
      seqNoSteps:'暂无步骤',seqClearConfirm:'将开始新录制，是否清除当前步骤？',
      seqInvalid:'顺序配置无效',seqSavedTo:'已保存到顺序栏位',seqRecOn:'录制中',
      seqPlayingNow:'播放中',seqIdleNow:'空闲',seqSaveTo:'💾 保存到栏位',
      seqSaveToTitle:'💾 保存到顺序栏位',seqSaveToSub:'选择目标栏位：',
      overwriteCur:'覆盖当前',slotN:'槽位',configN:'配置',
      saveFail:'保存失败',loadFail:'加载失败',delFail:'删除失败',importFail:'导入失败',noSlot:'无可用槽位',
      impTitle:'导入配置',impSub:'逐项选择导入目标',impImport:'导入',impSkip:'跳过',
      impInvalid:'无效的配置文件',impNoItem:'文件中没有已使用的配置',impAllSame:'所有配置均与现有相同，已跳过',
      impName:'名称',impSource:'来源',impTargetMode:'目标模式',impTargetSlot:'目标栏位',impSlotLbl:'栏位',impDone:'导入完成'},
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
      autoOn:'AUTO: ON',autoOff:'AUTO: OFF',
      modeKb:'🎹 Keyboard',modeFull:'🧩 Full',
      bleName:'📡 BLE Name',bleNameSub:'Changing restarts BLE; re-pair required',
      cancel:'Cancel',bleNameSaved:'Saved, BLE restarted, please re-pair',
      bleNameInvalid:'Name must be 1-24 characters',
      panelAuto:'⚙️ Auto',panelSeq:'🔁 Seq',exportAll:'📤 Export All',importAll:'📥 Import All',
      seqTitle:'🔁 Sequence Mode',seqRecordStart:'Start Record',seqRecordStop:'Stop',
      seqPlay:'Play',seqStop:'Stop',seqLoop:'Loop',seqRecSend:'Send while recording',
      seqLoopGap:'Loop Gap',seqSteps:'Steps',seqInsert:'➕ Insert',
      seqKey:'Key',seqHold:'Hold',seqGap:'Gap',seqEmpty:'No steps; start recording',
      seqNoSteps:'No steps yet',seqClearConfirm:'Start new recording? Clear current steps?',
      seqInvalid:'Invalid sequence',seqSavedTo:'Saved to seq slot',seqRecOn:'Recording',
      seqPlayingNow:'Playing',seqIdleNow:'Idle',seqSaveTo:'💾 Save to Slot',
      seqSaveToTitle:'💾 Save to Sequence Slot',seqSaveToSub:'Select target slot:',
      overwriteCur:'Overwrite current',slotN:'Slot',configN:'Config',
      saveFail:'Save failed',loadFail:'Load failed',delFail:'Delete failed',importFail:'Import failed',noSlot:'No slots',
      impTitle:'Import Configs',impSub:'Choose targets one by one',impImport:'Import',impSkip:'Skip',
      impInvalid:'Invalid config file',impNoItem:'No used configs in file',impAllSame:'All configs identical; skipped',
      impName:'Name',impSource:'Source',impTargetMode:'Target Mode',impTargetSlot:'Target Slot',impSlotLbl:'Slot',impDone:'Import finished'}
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
  // Mode & BLE Name
  document.getElementById('btnMode').textContent=isKbMode()?d.modeFull:d.modeKb;
  document.getElementById('btnBleName').textContent=d.bleName;
  document.getElementById('bleNameTitle').textContent=d.bleName;
  document.getElementById('bleNameSub').textContent=d.bleNameSub;
  document.getElementById('bleNameSaveBtn').textContent=d.save;
  document.getElementById('bleNameCancelBtn').textContent=d.cancel;
  // Panel & Sequence & Import
  document.getElementById('btnPanelMode').textContent=isSeqMode()?d.panelAuto:d.panelSeq;
  document.getElementById('btnExportAll').textContent=d.exportAll;
  document.getElementById('lblImportAllText').textContent=d.importAll;
  document.getElementById('lblSeqMode').textContent=d.seqTitle;
  document.getElementById('lblSeqLoop').textContent=d.seqLoop;
  document.getElementById('lblSeqRecSend').textContent=d.seqRecSend;
  document.getElementById('lblSeqLoopGap').textContent=d.seqLoopGap;
  document.getElementById('lblSeqSteps').textContent=d.seqSteps;
  document.getElementById('btnSeqInsert').textContent=d.seqInsert;
  document.getElementById('btnSeqApply').textContent=d.apply;
  document.getElementById('btnSeqSaveTo').textContent=d.seqSaveTo;
  var le=document.getElementById('lblSeqEmpty');if(le)le.textContent=d.seqEmpty;
  document.getElementById('impTitle').textContent=d.impTitle;
  document.getElementById('impSub').textContent=d.impSub;
  document.getElementById('impImportBtn').textContent=d.impImport;
  document.getElementById('impSkipBtn').textContent=d.impSkip;
  updSeqStatus();
  // Stats labels
  statsKeyLabels=[d.wForward,d.wBack,d.wLeft,d.wRight,d.tLeft,d.tRight,d.jump,d.crouch,d.prone,d.idle];
  renderStats();
}
(function(){var s=localStorage.getItem('lang');if(s){lang=s;applyLang();}})();
)rawliteral";

#ifdef ENABLE_WEB_AUTH
const char WEB_JS_AUTH[] PROGMEM = R"rawliteral(
// ---- Web 认证（仅 ENABLE_WEB_AUTH 时启用） ----
var authToken = localStorage.getItem('espAuthToken') || '';
var loginOpen = false;
var loginDismissed = false;

(function(){
  var __of = window.fetch;
  window.fetch = function(u, o){
    if (typeof u === 'string' && u.indexOf('/api/') === 0) {
      u += (u.indexOf('?') >= 0 ? '&' : '?') + 'token=' + encodeURIComponent(authToken);
    }
    return __of(u, o).then(function(r){
      if (r.status === 401 && u.indexOf('/api/login') < 0 && u.indexOf('/api/auth/change') < 0 && !loginOpen && !loginDismissed) {
        openLogin();
      }
      return r;
    });
  };
})();

function updateLoginBtn(){
  var b = document.getElementById('btnLogin');
  if (b) b.textContent = authToken ? '🔓' : '🔒';
}
function openLogin(){loginOpen=true;loginDismissed=false;document.getElementById('loginModal').style.display='flex';}
function closeLogin(){loginOpen=false;loginDismissed=true;document.getElementById('loginModal').style.display='none';}
function openPwdModal(){loginDismissed=false;document.getElementById('pwdModal').style.display='flex';}
function closePwdModal(){loginDismissed=false;document.getElementById('pwdModal').style.display='none';}

function doLogin(){
  var u = document.getElementById('loginUser').value;
  var p = document.getElementById('loginPass').value;
  fetch('/api/login',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'user='+encodeURIComponent(u)+'&pass='+encodeURIComponent(p)}).then(function(r){return r.json()}).then(function(d){
    if (d.ok) {
      authToken = d.token;
      localStorage.setItem('espAuthToken', authToken);
      closeLogin();
      updateLoginBtn();
      updStatus();
    } else {
      alert(d.error==='locked' ? '尝试次数过多，请稍后再试' : (d.error==='invalid' ? '用户名或密码错误' : '登录失败'));
    }
  }).catch(function(){});
}

function doLogout(){
  if (!confirm('确定登出当前会话？')) return;
  fetch('/api/logout',{method:'POST'}).then(function(){
    authToken = '';
    localStorage.removeItem('espAuthToken');
    updateLoginBtn();
  }).catch(function(){});
}

function toggleLogin(){
  if (authToken) { doLogout(); } else { openLogin(); }
}

function doChangePwd(){
  var body = 'oldUser='+encodeURIComponent(document.getElementById('pwdOldUser').value)
           + '&oldPass='+encodeURIComponent(document.getElementById('pwdOldPass').value)
           + '&newUser='+encodeURIComponent(document.getElementById('pwdNewUser').value)
           + '&newPass='+encodeURIComponent(document.getElementById('pwdNewPass').value);
  fetch('/api/auth/change',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body}).then(function(r){return r.json()}).then(function(d){
    if (d.ok) {
      authToken = '';
      localStorage.removeItem('espAuthToken');
      closePwdModal();
      updateLoginBtn();
      alert('已修改，请使用新凭据登录');
    } else {
      alert(d.error==='locked' ? '尝试次数过多，请稍后再试' : (d.error==='invalid' ? '当前凭据错误' : (d.error==='invalid length' ? '用户名/密码需为 1-20 个字符' : '修改失败')));
    }
  }).catch(function(){});
}

updateLoginBtn();
)rawliteral";
#endif

const char WEB_JS_TAIL_A[] PROGMEM = R"rawliteral(
// 初始键盘模式：有记忆用记忆，无记忆用编译期默认（DEFAULT_KB_ONLY_MODE）
(function(){
  var saved=localStorage.getItem('kbMode');
  var kbMode;
  if(saved==='1'||saved==='0'){kbMode=(saved==='1');}
  else{kbMode=(
)rawliteral";

const char WEB_JS_TAIL_B[] PROGMEM = R"rawliteral(
==='1');}
  if(kbMode)document.body.classList.add('kb-mode');
  updateModeBtn();
  fitKeyboard();
})();
window.addEventListener('resize',fitKeyboard);
window.addEventListener('orientationchange',function(){setTimeout(fitKeyboard,200);});

applyPanelMode();loadSeqCfg();loadCfg();updStatus();setInterval(updStatus,1000);setInterval(pollEvents,500);updStats();setInterval(updStats,2000);loadSlots();
)rawliteral";
