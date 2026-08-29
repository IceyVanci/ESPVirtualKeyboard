#include "config_manager.h"
#include "keymap.h"

#include <ArduinoJson.h>
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
  String trimmed = sanitizeName(name).substring(0, SLOT_NAME_MAX_LEN);
  _prefs.putString(slotNameKey(slotIndex).c_str(), trimmed);
  // 使用 JSON 字符串存储，避免结构体内存布局变更导致旧数据损坏
  _prefs.putString(slotDataKey(slotIndex).c_str(), configToJson(config, trimmed));
  _prefs.putBool(slotUsedKey(slotIndex).c_str(), true);
  return true;
}

bool ConfigManager::loadSlot(int slotIndex, AutoModeConfig& config, String& name) {
  if (slotIndex < 0 || slotIndex >= SLOT_COUNT) return false;
  if (!_prefs.getBool(slotUsedKey(slotIndex).c_str(), false)) return false;
  name = sanitizeName(_prefs.getString(slotNameKey(slotIndex).c_str(), "未命名"));

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

// ========== JSON 序列化 / 反序列化（ArduinoJson） ==========

String ConfigManager::sanitizeName(const String& in) {
  String out;
  out.reserve(in.length());
  for (unsigned int i = 0; i < in.length(); i++) {
    char c = in[i];
    if (c < 0x20 || c > 0x7E) continue;                       // 控制字符 / 非 ASCII
    if (c == '"' || c == '\\' || c == '<' || c == '>') continue; // JSON/HTML 危险字符
    out += c;
  }
  return out;
}

// 将自动模式配置写入 JsonObject（含 version，供单条与汇总导出复用）
static void fillConfigObject(JsonObject o, const AutoModeConfig& c) {
  o["version"] = 1;
  o["enabled"] = c.enabled;
  o["minIntervalMs"] = c.minIntervalMs;
  o["maxIntervalMs"] = c.maxIntervalMs;
  o["minHoldMs"] = c.minHoldMs;
  o["maxHoldMs"] = c.maxHoldMs;
  JsonObject w = o["weights"].to<JsonObject>();
  w["forward"] = c.moveForwardWeight;
  w["back"] = c.moveBackWeight;
  w["left"] = c.moveLeftWeight;
  w["right"] = c.moveRightWeight;
  w["turnLeft"] = c.turnLeftWeight;
  w["turnRight"] = c.turnRightWeight;
  w["jump"] = c.jumpWeight;
  w["c"] = c.weightC;
  w["z"] = c.weightZ;
  w["idle"] = c.idleWeight;
}

// 将顺序配置写入 JsonObject（含 version）
static void fillSeqConfigObject(JsonObject o, const SeqConfig& c) {
  o["version"] = 1;
  o["loop"] = c.loop;
  o["loopGapMs"] = c.loopGapMs;
  JsonArray steps = o["steps"].to<JsonArray>();
  for (int i = 0; i < c.stepCount; i++) {
    JsonObject s = steps.add<JsonObject>();
    s["k"] = c.steps[i].keyName;
    s["h"] = c.steps[i].holdMs;
    s["g"] = c.steps[i].gapMs;
  }
}

String ConfigManager::configToJson(const AutoModeConfig& c, const String& name) {
  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  fillConfigObject(root, c);
  root["name"] = sanitizeName(name);
  String out;
  serializeJson(doc, out);
  return out;
}

bool ConfigManager::jsonToConfig(const String& json, AutoModeConfig& config, String& name) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) return false;
  int ver = doc["version"] | 0;
  if (ver != 1) return false;

  name = doc["name"] | "导入配置";
  config.enabled = doc["enabled"] | true;
  config.minIntervalMs = doc["minIntervalMs"] | DEFAULT_MIN_INTERVAL_MS;
  config.maxIntervalMs = doc["maxIntervalMs"] | DEFAULT_MAX_INTERVAL_MS;
  config.minHoldMs = doc["minHoldMs"] | DEFAULT_MIN_HOLD_MS;
  config.maxHoldMs = doc["maxHoldMs"] | DEFAULT_MAX_HOLD_MS;

  // 权重从 weights 子对象提取
  JsonObject w = doc["weights"];
  config.moveForwardWeight = w["forward"] | DEFAULT_WEIGHT_FORWARD;
  config.moveBackWeight = w["back"] | DEFAULT_WEIGHT_BACK;
  config.moveLeftWeight = w["left"] | DEFAULT_WEIGHT_LEFT;
  config.moveRightWeight = w["right"] | DEFAULT_WEIGHT_RIGHT;
  config.turnLeftWeight = w["turnLeft"] | DEFAULT_WEIGHT_TURN_LEFT;
  config.turnRightWeight = w["turnRight"] | DEFAULT_WEIGHT_TURN_RIGHT;
  config.jumpWeight = w["jump"] | DEFAULT_WEIGHT_JUMP;
  config.weightC = w["c"] | DEFAULT_WEIGHT_C;
  config.weightZ = w["z"] | DEFAULT_WEIGHT_Z;
  config.idleWeight = w["idle"] | DEFAULT_WEIGHT_IDLE;

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

  name = sanitizeName(name).substring(0, SLOT_NAME_MAX_LEN);
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
  String trimmed = sanitizeName(name).substring(0, 24);
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
  String trimmed = sanitizeName(name).substring(0, SLOT_NAME_MAX_LEN);
  _prefs.putString(seqSlotNameKey(slotIndex).c_str(), trimmed);
  _prefs.putString(seqSlotDataKey(slotIndex).c_str(), seqConfigToJson(config, trimmed));
  _prefs.putBool(seqSlotUsedKey(slotIndex).c_str(), true);
  return true;
}

bool ConfigManager::loadSeqSlot(int slotIndex, SeqConfig& config, String& name) {
  if (slotIndex < 0 || slotIndex >= SLOT_COUNT) return false;
  if (!_prefs.getBool(seqSlotUsedKey(slotIndex).c_str(), false)) return false;
  name = sanitizeName(_prefs.getString(seqSlotNameKey(slotIndex).c_str(), "未命名"));
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

// ========== 顺序模式 JSON 序列化（ArduinoJson，风格同自动模式） ==========

String ConfigManager::seqConfigToJson(const SeqConfig& c, const String& name) {
  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  fillSeqConfigObject(root, c);
  root["name"] = sanitizeName(name);
  String out;
  serializeJson(doc, out);
  return out;
}

bool ConfigManager::seqJsonToConfig(const String& json, SeqConfig& config, String& name) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) return false;
  int ver = doc["version"] | 0;
  if (ver != 1) return false;
  name = doc["name"] | "顺序配置";
  config.loop = doc["loop"] | false;
  config.loopGapMs = (uint16_t)clampLong((long)(doc["loopGapMs"] | 1000), 0, 10000);
  config.stepCount = 0;

  for (JsonObject obj : doc["steps"].as<JsonArray>()) {
    if (config.stepCount >= SEQ_MAX_STEPS) break;
    String k = obj["k"] | "";
    k.toLowerCase();                                  // 容错大写键名
    if (k.length() > 0 && webKeyToHid(k) == 0xFF) k = "";  // 非法键名降级为暂停步骤
    SeqStep& s = config.steps[config.stepCount];
    s.keyName = k;
    s.holdMs = (uint16_t)clampLong((long)(obj["h"] | 100), 10, 10000);
    s.gapMs = (uint16_t)clampLong((long)(obj["g"] | 100), 10, 10000);
    config.stepCount++;
  }

  name = sanitizeName(name).substring(0, SLOT_NAME_MAX_LEN);
  return config.stepCount > 0;
}

// ========== 全部导出 ==========

String ConfigManager::exportAllConfigs() {
  JsonDocument doc;
  doc["app"] = "ESPVirtualKeyboard";
  doc["version"] = 1;
  JsonArray a = doc["auto"].to<JsonArray>();
  for (int i = 0; i < SLOT_COUNT; i++) {
    JsonObject o = a.add<JsonObject>();
    if (isSlotUsed(i)) {
      AutoModeConfig c;
      String n;
      if (loadSlot(i, c, n)) {
        o["used"] = true;
        o["name"] = sanitizeName(n);
        fillConfigObject(o["config"].to<JsonObject>(), c);
        continue;
      }
    }
    o["used"] = false;
    o["name"] = "";
  }
  JsonArray s = doc["seq"].to<JsonArray>();
  for (int i = 0; i < SLOT_COUNT; i++) {
    JsonObject o = s.add<JsonObject>();
    if (isSeqSlotUsed(i)) {
      SeqConfig c;
      String n;
      if (loadSeqSlot(i, c, n)) {
        o["used"] = true;
        o["name"] = sanitizeName(n);
        fillSeqConfigObject(o["config"].to<JsonObject>(), c);
        continue;
      }
    }
    o["used"] = false;
    o["name"] = "";
  }
  String out;
  serializeJson(doc, out);
  return out;
}