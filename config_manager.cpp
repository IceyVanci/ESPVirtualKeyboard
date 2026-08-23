#include "config_manager.h"
#include "keymap.h"

#include <mbedtls/md.h>
#include <stdio.h>

// ========== 构造与初始化 ==========

ConfigManager::ConfigManager() {}

void ConfigManager::begin() {
  _prefs.begin("cfgmgr", false);  // NVS namespace "cfgmgr", read-write
#ifdef ENABLE_WEB_AUTH
  // 首次启动时以 config.h 默认值初始化认证凭据（之后以 NVS 为准）
  if (!hasAuthCredentials()) {
    setAuthCredentials(WEB_AUTH_USERNAME, WEB_AUTH_PASSWORD);
  }
#endif
}

// ========== NVS Key 命名 ==========

String ConfigManager::slotNameKey(int i) {
  return "s" + String(i) + "n";
}

String ConfigManager::slotDataKey(int i) {
  return "s" + String(i) + "d";
}

String ConfigManager::slotUsedKey(int i) {
  return "s" + String(i) + "u";
}

// ========== 槽位 CRUD ==========

bool ConfigManager::saveSlot(int slotIndex, const String& name, const AutoModeConfig& config) {
  if (slotIndex < 0 || slotIndex >= SLOT_COUNT) return false;
  String trimmed = name.substring(0, SLOT_NAME_MAX_LEN);
  _prefs.putString(slotNameKey(slotIndex).c_str(), trimmed);
  // 使用 JSON 字符串存储，避免结构体内存布局变更导致旧数据损坏
  _prefs.putString(slotDataKey(slotIndex).c_str(), configToJson(config, trimmed));
  _prefs.putBool(slotUsedKey(slotIndex).c_str(), true);
  return true;
}

bool ConfigManager::loadSlot(int slotIndex, AutoModeConfig& config, String& name) {
  if (slotIndex < 0 || slotIndex >= SLOT_COUNT) return false;
  if (!_prefs.getBool(slotUsedKey(slotIndex).c_str(), false)) return false;
  name = _prefs.getString(slotNameKey(slotIndex).c_str(), "未命名");

  // 优先读取 JSON 格式（当前版本）
  String json = _prefs.getString(slotDataKey(slotIndex).c_str(), "");
  if (json.length() > 0) {
    String jname;
    if (jsonToConfig(json, config, jname)) {
      if (jname.length() > 0) name = jname;
      return true;
    }
  }

  // 回退：旧版原始字节格式（自动迁移为 JSON）
  size_t len = _prefs.getBytes(slotDataKey(slotIndex).c_str(), &config, sizeof(AutoModeConfig));
  if (len == sizeof(AutoModeConfig)) {
    _prefs.putString(slotDataKey(slotIndex).c_str(), configToJson(config, name));
    return true;
  }
  return false;
}

bool ConfigManager::overwriteSlot(int slotIndex, const String& name, const AutoModeConfig& config) {
  return saveSlot(slotIndex, name, config);
}

bool ConfigManager::deleteSlot(int slotIndex) {
  if (slotIndex < 0 || slotIndex >= SLOT_COUNT) return false;
  _prefs.remove(slotNameKey(slotIndex).c_str());
  _prefs.remove(slotDataKey(slotIndex).c_str());
  _prefs.putBool(slotUsedKey(slotIndex).c_str(), false);
  // 如果删除的是活动槽位，重置为默认
  if (getActiveSlot() == slotIndex) {
    setActiveSlot(-1);
  }
  return true;
}

bool ConfigManager::isSlotUsed(int slotIndex) {
  if (slotIndex < 0 || slotIndex >= SLOT_COUNT) return false;
  return _prefs.getBool(slotUsedKey(slotIndex).c_str(), false);
}

String ConfigManager::getSlotName(int slotIndex) {
  if (slotIndex < 0 || slotIndex >= SLOT_COUNT) return "";
  return _prefs.getString(slotNameKey(slotIndex).c_str(), "未命名");
}

SlotSummary ConfigManager::getSlotSummary(int slotIndex) {
  SlotSummary s;
  s.index = slotIndex;
  s.used = isSlotUsed(slotIndex);
  s.name = s.used ? getSlotName(slotIndex) : "";
  return s;
}

// ========== 活动槽位管理 ==========

void ConfigManager::setActiveSlot(int slotIndex) {
  _prefs.putInt("active", slotIndex);
}

int ConfigManager::getActiveSlot() {
  return _prefs.getInt("active", -1);
}

bool ConfigManager::loadActiveConfig(AutoModeConfig& config) {
  int slot = getActiveSlot();
  if (slot < 0 || slot >= SLOT_COUNT) return false;
  String name;
  return loadSlot(slot, config, name);
}

// ========== JSON 序列化（手写，不依赖 ArduinoJson） ==========

String ConfigManager::configToJson(const AutoModeConfig& c, const String& name) {
  String j = "{";
  j += "\"version\":1,";
  j += "\"name\":\"" + name + "\",";
  j += "\"enabled\":" + String(c.enabled ? "true" : "false") + ",";
  j += "\"minIntervalMs\":" + String(c.minIntervalMs) + ",";
  j += "\"maxIntervalMs\":" + String(c.maxIntervalMs) + ",";
  j += "\"minHoldMs\":" + String(c.minHoldMs) + ",";
  j += "\"maxHoldMs\":" + String(c.maxHoldMs) + ",";
  j += "\"weights\":{";
  j += "\"forward\":" + String(c.moveForwardWeight, 2) + ",";
  j += "\"back\":" + String(c.moveBackWeight, 2) + ",";
  j += "\"left\":" + String(c.moveLeftWeight, 2) + ",";
  j += "\"right\":" + String(c.moveRightWeight, 2) + ",";
  j += "\"turnLeft\":" + String(c.turnLeftWeight, 2) + ",";
  j += "\"turnRight\":" + String(c.turnRightWeight, 2) + ",";
  j += "\"jump\":" + String(c.jumpWeight, 2) + ",";
  j += "\"c\":" + String(c.weightC, 2) + ",";
  j += "\"z\":" + String(c.weightZ, 2) + ",";
  j += "\"idle\":" + String(c.idleWeight, 2);
  j += "}}";
  return j;
}

// ========== JSON 简易解析 ==========

// 辅助：从 JSON 中提取某个 key 的数值
static float jsonGetFloat(const String& json, const String& key, float defaultVal) {
  int idx = json.indexOf("\"" + key + "\"");
  if (idx < 0) return defaultVal;
  int colon = json.indexOf(':', idx);
  if (colon < 0) return defaultVal;
  int end = json.indexOf(',', colon);
  if (end < 0) end = json.indexOf('}', colon);
  if (end < 0) return defaultVal;
  String val = json.substring(colon + 1, end);
  val.trim();
  return val.toFloat();
}

static long jsonGetLong(const String& json, const String& key, long defaultVal) {
  int idx = json.indexOf("\"" + key + "\"");
  if (idx < 0) return defaultVal;
  int colon = json.indexOf(':', idx);
  if (colon < 0) return defaultVal;
  int end = json.indexOf(',', colon);
  if (end < 0) end = json.indexOf('}', colon);
  if (end < 0) return defaultVal;
  String val = json.substring(colon + 1, end);
  val.trim();
  return val.toInt();
}

static bool jsonGetBool(const String& json, const String& key, bool defaultVal) {
  int idx = json.indexOf("\"" + key + "\"");
  if (idx < 0) return defaultVal;
  int colon = json.indexOf(':', idx);
  if (colon < 0) return defaultVal;
  String rest = json.substring(colon + 1);
  rest.trim();
  return rest.startsWith("true");
}

static String jsonGetString(const String& json, const String& key, const String& defaultVal) {
  int idx = json.indexOf("\"" + key + "\"");
  if (idx < 0) return defaultVal;
  int colon = json.indexOf(':', idx);
  if (colon < 0) return defaultVal;
  int q1 = json.indexOf('"', colon + 1);
  if (q1 < 0) return defaultVal;
  int q2 = json.indexOf('"', q1 + 1);
  if (q2 < 0) return defaultVal;
  return json.substring(q1 + 1, q2);
}

bool ConfigManager::jsonToConfig(const String& json, AutoModeConfig& config, String& name) {
  // 验证 version 字段
  int ver = jsonGetLong(json, "version", 0);
  if (ver != 1) return false;

  name = jsonGetString(json, "name", "导入配置");
  config.enabled = jsonGetBool(json, "enabled", true);
  config.minIntervalMs = (unsigned long)jsonGetLong(json, "minIntervalMs", DEFAULT_MIN_INTERVAL_MS);
  config.maxIntervalMs = (unsigned long)jsonGetLong(json, "maxIntervalMs", DEFAULT_MAX_INTERVAL_MS);
  config.minHoldMs = (unsigned long)jsonGetLong(json, "minHoldMs", DEFAULT_MIN_HOLD_MS);
  config.maxHoldMs = (unsigned long)jsonGetLong(json, "maxHoldMs", DEFAULT_MAX_HOLD_MS);

  // 权重从 weights 子对象提取
  config.moveForwardWeight = jsonGetFloat(json, "forward", DEFAULT_WEIGHT_FORWARD);
  config.moveBackWeight = jsonGetFloat(json, "back", DEFAULT_WEIGHT_BACK);
  config.moveLeftWeight = jsonGetFloat(json, "left", DEFAULT_WEIGHT_LEFT);
  config.moveRightWeight = jsonGetFloat(json, "right", DEFAULT_WEIGHT_RIGHT);
  config.turnLeftWeight = jsonGetFloat(json, "turnLeft", DEFAULT_WEIGHT_TURN_LEFT);
  config.turnRightWeight = jsonGetFloat(json, "turnRight", DEFAULT_WEIGHT_TURN_RIGHT);
  config.jumpWeight = jsonGetFloat(json, "jump", DEFAULT_WEIGHT_JUMP);
  config.weightC = jsonGetFloat(json, "c", DEFAULT_WEIGHT_C);
  config.weightZ = jsonGetFloat(json, "z", DEFAULT_WEIGHT_Z);
  config.idleWeight = jsonGetFloat(json, "idle", DEFAULT_WEIGHT_IDLE);

  // 范围验证
  if (config.minIntervalMs < 100) config.minIntervalMs = 100;
  if (config.maxIntervalMs > 30000) config.maxIntervalMs = 30000;
  if (config.minIntervalMs > config.maxIntervalMs) {
    unsigned long tmp = config.minIntervalMs;
    config.minIntervalMs = config.maxIntervalMs;
    config.maxIntervalMs = tmp;
  }
  if (config.minHoldMs < 10) config.minHoldMs = 10;
  if (config.maxHoldMs > 5000) config.maxHoldMs = 5000;
  if (config.minHoldMs > config.maxHoldMs) {
    unsigned long tmp = config.minHoldMs;
    config.minHoldMs = config.maxHoldMs;
    config.maxHoldMs = tmp;
  }

  // 权重范围验证 [0, 1]
  auto clamp = [](float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); };
  config.moveForwardWeight = clamp(config.moveForwardWeight);
  config.moveBackWeight = clamp(config.moveBackWeight);
  config.moveLeftWeight = clamp(config.moveLeftWeight);
  config.moveRightWeight = clamp(config.moveRightWeight);
  config.turnLeftWeight = clamp(config.turnLeftWeight);
  config.turnRightWeight = clamp(config.turnRightWeight);
  config.jumpWeight = clamp(config.jumpWeight);
  config.weightC = clamp(config.weightC);
  config.weightZ = clamp(config.weightZ);
  config.idleWeight = clamp(config.idleWeight);

  name = name.substring(0, SLOT_NAME_MAX_LEN);
  return true;
}

// ========== 公共 JSON 接口 ==========

String ConfigManager::exportConfig(const AutoModeConfig& config, const String& name) {
  return configToJson(config, name);
}

String ConfigManager::exportSlot(int slotIndex) {
  AutoModeConfig config;
  String name;
  if (!loadSlot(slotIndex, config, name)) return "{}";
  return configToJson(config, name);
}

bool ConfigManager::importConfig(const String& json, AutoModeConfig& config, String& name) {
  return jsonToConfig(json, config, name);
}

bool ConfigManager::importToSlot(int slotIndex, const String& json) {
  AutoModeConfig config;
  String name;
  if (!jsonToConfig(json, config, name)) return false;
  return saveSlot(slotIndex, name, config);
}

String ConfigManager::exportCurrentConfig(const AutoModeConfig& config) {
  return configToJson(config, "当前配置");
}

bool ConfigManager::importToCurrentConfig(const String& json, AutoModeConfig& config) {
  String name;
  return jsonToConfig(json, config, name);
}

// ========== 认证凭据（NVS 持久化） ==========

String ConfigManager::sha256Hex(const String& input) {
  unsigned char hash[32];
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0);
  mbedtls_md_starts(&ctx);
  mbedtls_md_update(&ctx, (const unsigned char*)input.c_str(), input.length());
  mbedtls_md_finish(&ctx, hash);
  mbedtls_md_free(&ctx);

  String hex = "";
  char buf[3];
  for (int i = 0; i < 32; i++) {
    snprintf(buf, sizeof(buf), "%02x", hash[i]);
    hex += buf;
  }
  return hex;
}

String ConfigManager::getAuthUser() {
  String u = _prefs.getString("authuser", "");
  if (u.length() == 0) return String(WEB_AUTH_USERNAME);
  return u;
}

String ConfigManager::getAuthPass() {
  String h = _prefs.getString("authhash", "");
  if (h.length() == 0) return sha256Hex(String(WEB_AUTH_PASSWORD));
  return h;
}

bool ConfigManager::hasAuthCredentials() {
  return _prefs.isKey("authuser") && _prefs.isKey("authhash");
}

bool ConfigManager::setAuthCredentials(const String& user, const String& pass) {
  if (user.length() == 0 || user.length() > 20) return false;
  if (pass.length() == 0 || pass.length() > 20) return false;
  _prefs.putString("authuser", user);
  _prefs.putString("authhash", sha256Hex(pass));
  return true;
}

// ========== 蓝牙设备名称（NVS 持久化） ==========

String ConfigManager::getBleName() {
  String n = _prefs.getString("blename", "");
  if (n.length() == 0) return String(BLE_DEVICE_NAME);
  return n;
}

bool ConfigManager::setBleName(const String& name) {
  String trimmed = name.substring(0, 24);
  if (trimmed.length() == 0) return false;
  _prefs.putString("blename", trimmed);
  return true;
}

// ========== 顺序模式槽位（NVS 持久化） ==========

static String seqSlotNameKey(int i) { return "sq" + String(i) + "n"; }
static String seqSlotDataKey(int i) { return "sq" + String(i) + "d"; }
static String seqSlotUsedKey(int i) { return "sq" + String(i) + "u"; }

static long clampLong(long v, long lo, long hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

bool ConfigManager::saveSeqSlot(int slotIndex, const String& name, const SeqConfig& config) {
  if (slotIndex < 0 || slotIndex >= SLOT_COUNT) return false;
  String trimmed = name.substring(0, SLOT_NAME_MAX_LEN);
  _prefs.putString(seqSlotNameKey(slotIndex).c_str(), trimmed);
  _prefs.putString(seqSlotDataKey(slotIndex).c_str(), seqConfigToJson(config, trimmed));
  _prefs.putBool(seqSlotUsedKey(slotIndex).c_str(), true);
  return true;
}

bool ConfigManager::loadSeqSlot(int slotIndex, SeqConfig& config, String& name) {
  if (slotIndex < 0 || slotIndex >= SLOT_COUNT) return false;
  if (!_prefs.getBool(seqSlotUsedKey(slotIndex).c_str(), false)) return false;
  name = _prefs.getString(seqSlotNameKey(slotIndex).c_str(), "未命名");
  String json = _prefs.getString(seqSlotDataKey(slotIndex).c_str(), "");
  if (json.length() == 0) return false;
  String jname;
  if (!seqJsonToConfig(json, config, jname)) return false;
  if (jname.length() > 0) name = jname;
  return true;
}

bool ConfigManager::deleteSeqSlot(int slotIndex) {
  if (slotIndex < 0 || slotIndex >= SLOT_COUNT) return false;
  _prefs.remove(seqSlotNameKey(slotIndex).c_str());
  _prefs.remove(seqSlotDataKey(slotIndex).c_str());
  _prefs.putBool(seqSlotUsedKey(slotIndex).c_str(), false);
  if (getActiveSeqSlot() == slotIndex) setActiveSeqSlot(-1);
  return true;
}

bool ConfigManager::isSeqSlotUsed(int slotIndex) {
  if (slotIndex < 0 || slotIndex >= SLOT_COUNT) return false;
  return _prefs.getBool(seqSlotUsedKey(slotIndex).c_str(), false);
}

String ConfigManager::getSeqSlotName(int slotIndex) {
  if (slotIndex < 0 || slotIndex >= SLOT_COUNT) return "";
  return _prefs.getString(seqSlotNameKey(slotIndex).c_str(), "未命名");
}

void ConfigManager::setActiveSeqSlot(int slotIndex) {
  _prefs.putInt("sqactive", slotIndex);
}

int ConfigManager::getActiveSeqSlot() {
  return _prefs.getInt("sqactive", -1);
}

bool ConfigManager::loadActiveSeqConfig(SeqConfig& config) {
  int slot = getActiveSeqSlot();
  if (slot < 0 || slot >= SLOT_COUNT) return false;
  String name;
  return loadSeqSlot(slot, config, name);
}

// ========== 顺序模式 JSON 序列化（手写，风格同自动模式） ==========

String ConfigManager::seqConfigToJson(const SeqConfig& c, const String& name) {
  String j = "{";
  j += "\"version\":1,";
  j += "\"name\":\"" + name + "\",";
  j += "\"loop\":" + String(c.loop ? "true" : "false") + ",";
  j += "\"loopGapMs\":" + String(c.loopGapMs) + ",";
  j += "\"steps\":[";
  for (int i = 0; i < c.stepCount; i++) {
    if (i > 0) j += ",";
    j += "{\"k\":\"" + c.steps[i].keyName + "\",\"h\":" + String(c.steps[i].holdMs) +
         ",\"g\":" + String(c.steps[i].gapMs) + "}";
  }
  j += "]}";
  return j;
}

bool ConfigManager::seqJsonToConfig(const String& json, SeqConfig& config, String& name) {
  int ver = jsonGetLong(json, "version", 0);
  if (ver != 1) return false;
  name = jsonGetString(json, "name", "顺序配置");
  config.loop = jsonGetBool(json, "loop", false);
  config.loopGapMs = (uint16_t)clampLong(jsonGetLong(json, "loopGapMs", 1000), 0, 10000);
  config.stepCount = 0;

  int arrIdx = json.indexOf("\"steps\"");
  if (arrIdx >= 0) {
    int colon = json.indexOf(':', arrIdx);
    int q1 = json.indexOf('[', colon);
    int arrEnd = json.indexOf(']', q1);
    if (q1 >= 0 && arrEnd > q1) {
      int pos = q1;
      while (pos < arrEnd && config.stepCount < SEQ_MAX_STEPS) {
        int ob = json.indexOf('{', pos);
        if (ob < 0 || ob > arrEnd) break;
        int cb = json.indexOf('}', ob);
        if (cb < 0 || cb > arrEnd) break;
        String elem = json.substring(ob, cb + 1);
        String k = jsonGetString(elem, "k", "");
        if (!(k.length() == 0 || webKeyToHid(k) != 0xFF)) break;  // 非法键名终止
        SeqStep& s = config.steps[config.stepCount];
        s.keyName = k;
        s.holdMs = (uint16_t)clampLong(jsonGetLong(elem, "h", 100), 10, 10000);
        s.gapMs = (uint16_t)clampLong(jsonGetLong(elem, "g", 100), 10, 10000);
        config.stepCount++;
        pos = cb + 1;
      }
    }
  }

  name = name.substring(0, SLOT_NAME_MAX_LEN);
  return config.stepCount > 0;
}

// ========== 全部导出 ==========

String ConfigManager::exportAllConfigs() {
  String j = "{\"app\":\"ESPVirtualKeyboard\",\"version\":1,\"auto\":[";
  for (int i = 0; i < SLOT_COUNT; i++) {
    if (i > 0) j += ",";
    if (isSlotUsed(i)) {
      AutoModeConfig c;
      String n;
      if (loadSlot(i, c, n)) {
        j += "{\"used\":true,\"name\":\"" + n + "\",\"config\":" + configToJson(c, n) + "}";
      } else {
        j += "{\"used\":false,\"name\":\"\"}";
      }
    } else {
      j += "{\"used\":false,\"name\":\"\"}";
    }
  }
  j += "],\"seq\":[";
  for (int i = 0; i < SLOT_COUNT; i++) {
    if (i > 0) j += ",";
    if (isSeqSlotUsed(i)) {
      SeqConfig c;
      String n;
      if (loadSeqSlot(i, c, n)) {
        j += "{\"used\":true,\"name\":\"" + n + "\",\"config\":" + seqConfigToJson(c, n) + "}";
      } else {
        j += "{\"used\":false,\"name\":\"\"}";
      }
    } else {
      j += "{\"used\":false,\"name\":\"\"}";
    }
  }
  j += "]}";
  return j;
}