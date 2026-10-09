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
  String trimmed = truncateName(sanitizeName(name), SLOT_NAME_MAX_LEN);
  if (trimmed.length() == 0) trimmed = "配置" + String(slotIndex + 1);  // 空名兜底
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

// UTF-8 感知的名称消毒：保留可打印 ASCII 与合法多字节 UTF-8（中文/emoji），
// 剔除控制字符、JSON/HTML 危险字符（" \ < >）以及无效/截断字节
String ConfigManager::sanitizeName(const String& in) {
  String out;
  out.reserve(in.length());
  unsigned int i = 0;
  while (i < in.length()) {
    uint8_t c = (uint8_t)in[i];
    if (c < 0x20 || c == 0x7F) { i++; continue; }                          // ASCII 控制字符
    if (c == '"' || c == '\\' || c == '<' || c == '>') { i++; continue; }  // JSON/HTML 危险字符
    if (c < 0x80) { out += (char)c; i++; continue; }                       // ASCII 可打印

    // 多字节 UTF-8 序列解码
    int len = 0;
    uint32_t cp = 0;
    if ((c & 0xE0) == 0xC0) { len = 2; cp = c & 0x1F; }
    else if ((c & 0xF0) == 0xE0) { len = 3; cp = c & 0x0F; }
    else if ((c & 0xF8) == 0xF0) { len = 4; cp = c & 0x07; }
    else { i++; continue; }                                                // 无效首字节
    if (i + (unsigned int)len > in.length()) { i++; continue; }            // 截断序列
    bool ok = true;
    for (int k = 1; k < len; k++) {
      uint8_t cc = (uint8_t)in[i + k];
      if ((cc & 0xC0) != 0x80) { ok = false; break; }
      cp = (cp << 6) | (cc & 0x3F);
    }
    // 非法/控制码点（C1 控制符、BOM）丢弃
    if (!ok || cp < 0x80 || cp > 0x10FFFF || (cp >= 0x80 && cp <= 0x9F) || cp == 0xFEFF) {
      i++;
      continue;
    }
    out.concat(in.c_str() + i, len);
    i += len;
  }
  return out;
}

// 按 Unicode 码点截断（不会切断多字节字符）
String ConfigManager::truncateName(const String& in, unsigned int maxChars) {
  unsigned int i = 0, n = 0;
  while (i < in.length() && n < maxChars) {
    uint8_t c = (uint8_t)in[i];
    unsigned int len = 1;
    if ((c & 0xE0) == 0xC0) len = 2;
    else if ((c & 0xF0) == 0xE0) len = 3;
    else if ((c & 0xF8) == 0xF0) len = 4;
    if (i + len > in.length()) break;
    i += len;
    n++;
  }
  return in.substring(0, i);
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

// 将顺序配置写入 JsonObject（v2：按省略规则输出默认字段）
static void fillSeqConfigObject(JsonObject o, const SeqConfig& c) {
  o["version"] = 2;
  o["loop"] = c.loop;
  o["loopGapMs"] = c.loopGapMs;
  JsonArray steps = o["steps"].to<JsonArray>();
  for (int i = 0; i < c.stepCount; i++) {
    const SeqStep& st = c.steps[i];
    JsonObject s = steps.add<JsonObject>();
    s["k"] = st.keyName;
    s["h"] = st.holdMs;
    s["g"] = st.gapMs;
    if (st.repeat > 1) s["r"] = st.repeat;
    if (st.randomKey) s["rk"] = 1;
    if (st.holdMinMs > 0) {
      JsonArray hr = s["hr"].to<JsonArray>();
      hr.add(st.holdMinMs);
      hr.add(st.holdMaxMs);
    }
    if (st.gapMinMs > 0) {
      JsonArray gr = s["gr"].to<JsonArray>();
      gr.add(st.gapMinMs);
      gr.add(st.gapMaxMs);
    }
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

  name = truncateName(sanitizeName(name), SLOT_NAME_MAX_LEN);
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
  String trimmed = truncateName(sanitizeName(name), 24);
  if (trimmed.length() == 0) return false;
  _prefs.putString("blename", trimmed);
  return true;
}

// ========== WiFi 凭据（NVS 持久化，串口配网） ==========

// 剔除控制字符与分隔符（\r \n \0 及 0x00-0x1F）
static String sanitizeWifiValue(const String& in) {
  String out;
  out.reserve(in.length());
  for (unsigned int i = 0; i < in.length(); i++) {
    char c = in[i];
    if (c < 0x20) continue;
    out += c;
  }
  return out;
}

String ConfigManager::getWifiSsid() {
  return _prefs.getString("wifissid", "");
}

String ConfigManager::getWifiPass() {
  return _prefs.getString("wifipass", "");
}

bool ConfigManager::hasWifiCredentials() {
  return _prefs.getString("wifissid", "").length() > 0;
}

bool ConfigManager::setWifiCredentials(const String& ssid, const String& pass) {
  String s = sanitizeWifiValue(ssid).substring(0, 32);
  String p = sanitizeWifiValue(pass).substring(0, 63);
  if (s.length() == 0) {
    // 空 SSID 语义：清除 NVS 凭据
    _prefs.remove("wifissid");
    _prefs.remove("wifipass");
    return true;
  }
  _prefs.putString("wifissid", s);
  _prefs.putString("wifipass", p);
  return true;
}

bool ConfigManager::isWifiOverride() {
  return _prefs.getBool("wifiovr", false);
}

bool ConfigManager::setWifiOverride(bool enable) {
  _prefs.putBool("wifiovr", enable);
  return true;
}

bool ConfigManager::clearWifiCredentials() {
  _prefs.remove("wifissid");
  _prefs.remove("wifipass");
  _prefs.remove("wifiovr");
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
  String trimmed = truncateName(sanitizeName(name), SLOT_NAME_MAX_LEN);
  if (trimmed.length() == 0) trimmed = "序列" + String(slotIndex + 1);  // 空名兜底

  // 写前体积复检（防御式；能写进 NVS 的配置必然已过守卫）
  String json;
  if (!seqConfigToJsonChecked(config, trimmed, json)) return false;

  String nameKey = seqSlotNameKey(slotIndex);
  String dataKey = seqSlotDataKey(slotIndex);
  String usedKey = seqSlotUsedKey(slotIndex);

  if (trimmed.length() > 0) {
    if (_prefs.putString(nameKey.c_str(), trimmed) == 0) return false;
  } else {
    _prefs.remove(nameKey.c_str());
  }
  if (_prefs.putString(dataKey.c_str(), json) == 0) {
    // NVS 满/写入失败：清理半截数据并回滚已用标记，避免「已用栏位读不出」
    _prefs.remove(dataKey.c_str());
    _prefs.putBool(usedKey.c_str(), false);
    return false;
  }
  _prefs.putBool(usedKey.c_str(), true);
  return true;
}

bool ConfigManager::loadSeqSlot(int slotIndex, SeqConfig& config, String& name) {
  if (slotIndex < 0 || slotIndex >= SLOT_COUNT) return false;
  if (!_prefs.getBool(seqSlotUsedKey(slotIndex).c_str(), false)) return false;
  name = sanitizeName(_prefs.getString(seqSlotNameKey(slotIndex).c_str(), "未命名"));
  String json = _prefs.getString(seqSlotDataKey(slotIndex).c_str(), "");
  if (json.length() == 0) return false;
  String jname;
  if (seqJsonToConfig(json, config, jname) != SEQ_PARSE_OK) return false;
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

// 序列化 + 体积校验（供写入路径使用；超限返回 false 且不落盘）
bool ConfigManager::seqConfigToJsonChecked(const SeqConfig& c, const String& name, String& out) {
  out = seqConfigToJson(c, name);
  return out.length() <= SEQ_JSON_MAX_BYTES;
}

SeqParseResult ConfigManager::seqJsonToConfig(const String& json, SeqConfig& config, String& name) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) return SEQ_PARSE_INVALID;

  int ver = doc["version"] | 1;   // 缺省按 v1（旧版「导出当前」不含 version）
  if (ver != 1 && ver != 2) return SEQ_PARSE_INVALID;

  JsonArray steps = doc["steps"].as<JsonArray>();
  if (steps.isNull()) return SEQ_PARSE_INVALID;
  if (steps.size() > SEQ_MAX_STEPS) return SEQ_PARSE_TOO_MANY;

  name = doc["name"] | "顺序配置";
  config.loop = doc["loop"] | false;
  config.loopGapMs = (uint16_t)clampLong((long)(doc["loopGapMs"] | 1000), 0, 10000);
  config.stepCount = 0;

  for (JsonObject obj : steps) {
    String k = obj["k"] | "";
    k.toLowerCase();                                  // 容错大写键名
    bool rk = (int)(obj["rk"] | 0) != 0;
    if (!rk && k.length() > 0 && webKeyToHid(k) == 0xFF) k = "";  // 非法键名降级为暂停

    SeqStep& s = config.steps[config.stepCount];
    s.keyName = k;
    s.holdMs = (uint16_t)clampLong((long)(obj["h"] | 100), 10, 10000);
    s.gapMs  = (uint16_t)clampLong((long)(obj["g"] | 100), 10, 10000);
    s.repeat = (uint8_t)clampLong((long)(obj["r"] | 1), 1, 99);
    s.randomKey = rk;
    s.holdMinMs = 0; s.holdMaxMs = 0; s.gapMinMs = 0; s.gapMaxMs = 0;

    if (obj["hr"].is<JsonArray>()) {
      JsonArray hr = obj["hr"].as<JsonArray>();
      if (hr.size() == 2) {
        long a = clampLong((long)(hr[0] | 0), 10, 10000);
        long b = clampLong((long)(hr[1] | 0), 10, 10000);
        if (a > b) { long t = a; a = b; b = t; }
        s.holdMinMs = (uint16_t)a;
        s.holdMaxMs = (uint16_t)b;
      }
    }
    if (obj["gr"].is<JsonArray>()) {
      JsonArray gr = obj["gr"].as<JsonArray>();
      if (gr.size() == 2) {
        long a = clampLong((long)(gr[0] | 0), 10, 10000);
        long b = clampLong((long)(gr[1] | 0), 10, 10000);
        if (a > b) { long t = a; a = b; b = t; }
        s.gapMinMs = (uint16_t)a;
        s.gapMaxMs = (uint16_t)b;
      }
    }
    config.stepCount++;
  }

  if (config.stepCount == 0) return SEQ_PARSE_INVALID;
  name = truncateName(sanitizeName(name), SLOT_NAME_MAX_LEN);

  // 解析并重新序列化后的体积校验（与落盘格式一致）
  String out = seqConfigToJson(config, name);
  if (out.length() > SEQ_JSON_MAX_BYTES) return SEQ_PARSE_TOO_LARGE;
  return SEQ_PARSE_OK;
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
        JsonObject co = o["config"].to<JsonObject>();
        fillConfigObject(co, c);
        co["name"] = sanitizeName(n);  // 名称同步写入 config，保证导出→导入往返不丢名
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
        JsonObject co = o["config"].to<JsonObject>();
        fillSeqConfigObject(co, c);
        co["name"] = sanitizeName(n);  // 名称同步写入 config，保证导出→导入往返不丢名
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