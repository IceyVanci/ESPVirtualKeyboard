#include "config_manager.h"

// ========== 构造与初始化 ==========

ConfigManager::ConfigManager() {}

void ConfigManager::begin() {
  _prefs.begin("cfgmgr", false);  // NVS namespace "cfgmgr", read-write
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
  _prefs.putBytes(slotDataKey(slotIndex).c_str(), &config, sizeof(AutoModeConfig));
  _prefs.putBool(slotUsedKey(slotIndex).c_str(), true);
  return true;
}

bool ConfigManager::loadSlot(int slotIndex, AutoModeConfig& config, String& name) {
  if (slotIndex < 0 || slotIndex >= SLOT_COUNT) return false;
  if (!_prefs.getBool(slotUsedKey(slotIndex).c_str(), false)) return false;
  name = _prefs.getString(slotNameKey(slotIndex).c_str(), "未命名");
  size_t len = _prefs.getBytes(slotDataKey(slotIndex).c_str(), &config, sizeof(AutoModeConfig));
  return (len == sizeof(AutoModeConfig));
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