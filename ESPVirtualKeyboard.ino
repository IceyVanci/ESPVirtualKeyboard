/*
 * ESP32 BLE Keyboard - 硬件随机输入器
 * 
 * 功能：
 * - BLE 蓝牙键盘模拟（连接电脑作为无线键盘）
 * - WiFi Web 控制面板（手机/电脑浏览器控制）
 * - 自动模式：随机模拟 WASD、方向键、空格
 * - 手动模式：网页完整键盘，点击即发送
 * - LED 状态指示：D4=按键灯, D5=状态灯
 * 
 * 使用方法：
 * 1. 在 config.h 填写 WIFI_SSID/WIFI_PASSWORD（编译期凭据，优先于 NVS）后编译；
 *    若两者为空则进入串口配网等待，可用 firmware-updater 工具「仅配网」写入 WiFi
 *    （分发包的固件由构建脚本强制置空，不含任何凭据）
 * 2. Arduino IDE 选择 ESP32C3 Dev Module
 * 3. 烧录后打开串口监视器查看 IP 地址
 * 4. 浏览器访问 IP 地址打开控制面板
 * 5. 在 Windows 蓝牙设置中配对 "ESP32 Keyboard"
 */

#include "config.h"
#include "ble_keyboard.h"
#include "auto_mode.h"
#include "web_server.h"
#include "config_manager.h"
#include "seq_mode.h"
#include <ArduinoJson.h>

// 全局对象
BleKeyboard    keyboard(BLE_DEVICE_NAME);
AutoMode       autoMode(&keyboard);
SequenceMode   seqMode(&keyboard);
ConfigManager  configMgr;
WebController  webCtrl(&keyboard, &autoMode, &configMgr, &seqMode);

// LED 状态管理变量
unsigned long lastLedToggle = 0;
bool ledToggleState = false;
unsigned long keyFlashStart = 0;
bool keyFlashing = false;
bool lastD4State = false;  // 用于检测 D4 上升沿

// WiFi 重连（非阻塞状态机）
enum WifiState {
  WIFI_DISCONNECTED,
  WIFI_PROVISION,   // 空凭据：等待串口配网
  WIFI_CONNECTING,
  WIFI_CONNECTED,
  WIFI_FAILED
};
WifiState      wifiState = WIFI_DISCONNECTED;
unsigned long  wifiAttemptStart = 0;
int            wifiAttemptCount = 0;
unsigned long  wifiRetryAt = 0;        // 退避等待结束时间
unsigned long  wifiBackoffMs = WIFI_RETRY_BACKOFF_MS;

// 解析后的 WiFi 凭据（启动时确定，配网后刷新）
String wifiSsid = "";
String wifiPass = "";
bool   wifiCompileCreds = false;       // 编译期 WIFI_SSID 非空
bool   wifiUseNvs = false;             // 实际凭据来自 NVS

#if ENABLE_SERIAL_PROVISION
// 串口配网：行缓冲（仅解析 "@" 前缀单行 JSON，最长 256B）
String serialLine = "";
bool   wifiConnectEventSent = false;   // wifi_connecting 去重标志

void handleSerialProvision();
void processSerialCommand(const String& payload);
void sendSerialEvent(const char* evt, const String& msg);
void sendPong();
void sendWifiSaved(bool overrideOn);
void sendWifiConnecting();
void sendWifiFailed();
void sendStatus();
#else
static inline void sendWifiConnecting() {}
static inline void sendWifiFailed() {}
static inline void sendStatus() {}
#endif

void startWiFi();
void handleWiFi();
void updateStatusLED();
void resolveWifiCredentials();
void syncNtp();

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  delay(1000);
  Serial.println();
  Serial.println("================================");
  Serial.println("  ESP32 BLE Keyboard Controller");
  Serial.println("================================");

  // 初始化 LED
  pinMode(LED_D4, OUTPUT);
  pinMode(LED_D5, OUTPUT);
  digitalWrite(LED_D4, LOW);
  digitalWrite(LED_D5, LOW);

  // 开机快闪指示初始化中
  for (int i = 0; i < 6; i++) {
    digitalWrite(LED_D5, ledToggleState ? HIGH : LOW);
    ledToggleState = !ledToggleState;
    delay(LED_BLINK_FAST_MS);
  }
  digitalWrite(LED_D5, LOW);

  // 初始化配置管理器（先于一切，读取 NVS：蓝牙名/WiFi 凭据等）
  configMgr.begin();

  // 解析 WiFi 凭据优先级：覆盖标志 > 编译期非空 > NVS > 空（进入配网等待）
  resolveWifiCredentials();

  // 设置蓝牙设备名（NVS 覆盖默认值）
  keyboard.setDeviceName(configMgr.getBleName());

  // 初始化 BLE 键盘
  keyboard.begin();

  // 尝试加载上次保存的配置
  AutoModeConfig savedConfig;
  if (configMgr.loadActiveConfig(savedConfig)) {
    autoMode.setConfig(savedConfig);
    int activeSlot = configMgr.getActiveSlot();
    Serial.print("[Config] 已加载槽位 ");
    Serial.print(activeSlot);
    Serial.print(" 的配置: ");
    Serial.println(configMgr.getSlotName(activeSlot));
  } else {
    Serial.println("[Config] 使用默认配置");
  }

  // 尝试加载上次保存的顺序配置
  SeqConfig savedSeq;
  if (configMgr.loadActiveSeqConfig(savedSeq)) {
    seqMode.setConfig(savedSeq);
    Serial.println("[Seq] 已加载顺序配置");
  }

  // 自动模式默认关闭
  autoMode.setEnabled(false);

  // 启动 Web 服务器
  webCtrl.begin();

  // 启动 WiFi（非阻塞；空凭据进入串口配网等待，配网后自动连接）
  startWiFi();

  Serial.println("================================");
  Serial.println("系统就绪！");
  Serial.print("控制面板: http://");
  Serial.println(webCtrl.getLocalIP());
  Serial.println("在蓝牙设置中配对: " + configMgr.getBleName());
  if (wifiState == WIFI_PROVISION) {
    Serial.println("[WiFi] 未配置凭据：等待串口配网（firmware-updater「仅配网」）");
  }
  Serial.println("================================");
}

void loop() {
  webCtrl.handleClient();
  autoMode.update();
  seqMode.update();
  keyboard.checkStuck(WEB_KEY_STUCK_TIMEOUT_MS);
  updateStatusLED();
  handleWiFi();
#if ENABLE_SERIAL_PROVISION
  handleSerialProvision();
#endif
}

// ========== LED 状态管理（非阻塞） ==========
void updateStatusLED() {
  unsigned long now = millis();
  BleState state = keyboard.getState();

  // D5 状态灯
  if (state == BLE_STATE_STOPPED) {
    digitalWrite(LED_D5, LOW);
  } else if (state == BLE_STATE_ADVERTISING) {
    if (now - lastLedToggle >= LED_BLINK_ADVERT_MS) {
      ledToggleState = !ledToggleState;
      digitalWrite(LED_D5, ledToggleState ? HIGH : LOW);
      lastLedToggle = now;
    }
  } else if (state == BLE_STATE_CONNECTED) {
    if (autoMode.isEnabled()) {
      if (now - lastLedToggle >= LED_BLINK_SLOW_MS) {
        ledToggleState = !ledToggleState;
        digitalWrite(LED_D5, ledToggleState ? HIGH : LOW);
        lastLedToggle = now;
      }
    } else {
      digitalWrite(LED_D5, HIGH);
    }
  }

  // D4 按键闪烁 - 检测上升沿并定时关闭
  bool curD4 = digitalRead(LED_D4);
  if (curD4 && !lastD4State) {
    // 上升沿：刚被按下，启动计时
    keyFlashStart = now;
    keyFlashing = true;
  }
  lastD4State = curD4;
  if (keyFlashing && (now - keyFlashStart >= LED_KEY_FLASH_MS)) {
    digitalWrite(LED_D4, LOW);
    keyFlashing = false;
  }
}

// ========== WiFi 连接（非阻塞状态机） ==========

// 凭据优先级：NVS 覆盖标志 && NVS 非空 > 编译期非空 > NVS 非空 > 空（配网等待）
void resolveWifiCredentials() {
  wifiCompileCreds = strlen(WIFI_SSID) > 0;
  bool nvsHas = configMgr.hasWifiCredentials();
  bool ovr = configMgr.isWifiOverride();

  if (ovr && nvsHas) {
    wifiSsid = configMgr.getWifiSsid();
    wifiPass = configMgr.getWifiPass();
    wifiUseNvs = true;
  } else if (wifiCompileCreds) {
    wifiSsid = String(WIFI_SSID);
    wifiPass = String(WIFI_PASSWORD);
    wifiUseNvs = false;
  } else if (nvsHas) {
    wifiSsid = configMgr.getWifiSsid();
    wifiPass = configMgr.getWifiPass();
    wifiUseNvs = true;
  } else {
    wifiSsid = "";
    wifiPass = "";
    wifiUseNvs = false;
  }
}

void syncNtp() {
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  Serial.println("[NTP] 时间同步中...");
}

void startWiFi() {
  if (wifiSsid.length() == 0) {
    wifiState = WIFI_PROVISION;
    wifiConnectEventSent = false;
    Serial.println("[WiFi] 未配置凭据，进入串口配网等待...");
    return;
  }

  Serial.print("[WiFi] 正在连接 ");
  Serial.print(wifiSsid);
  Serial.println("...");

  // 连接前复位驱动状态（重试时避免旧状态卡死）
  WiFi.disconnect();

  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());
  wifiState = WIFI_CONNECTING;
  wifiAttemptStart = millis();
  wifiAttemptCount = 0;
  wifiBackoffMs = WIFI_RETRY_BACKOFF_MS;
  sendWifiConnecting();
}

void handleWiFi() {
  unsigned long now = millis();

  switch (wifiState) {
    case WIFI_PROVISION:
      // 空凭据：等待串口配网命令，不做连接尝试
      break;

    case WIFI_CONNECTING:
      if (WiFi.status() == WL_CONNECTED) {
        wifiState = WIFI_CONNECTED;
        wifiConnectEventSent = false;
        Serial.println();
        Serial.print("[WiFi] 已连接! IP: ");
        Serial.println(WiFi.localIP());
        syncNtp();
        sendStatus();
      } else if (now - wifiAttemptStart >= WIFI_CONNECT_TIMEOUT_MS) {
        if (wifiAttemptCount < 2) {
          // 软重试 + 指数退避
          wifiAttemptCount++;
          wifiState = WIFI_DISCONNECTED;
          wifiRetryAt = now + wifiBackoffMs;
          wifiBackoffMs = min(wifiBackoffMs * 2, (unsigned long)WIFI_MAX_BACKOFF_MS);
          Serial.print("[WiFi] 连接超时，");
          Serial.print(wifiRetryAt - now);
          Serial.println("ms 后重试");
        } else {
          wifiState = WIFI_FAILED;
          wifiConnectEventSent = false;
          Serial.println();
          Serial.print("[WiFi] 连接失败（status=");
          Serial.print(WiFi.status());
          Serial.println("），执行射频硬复位后重试");
          sendWifiFailed();
        }
      }
      break;

    case WIFI_DISCONNECTED:
      if (now >= wifiRetryAt) startWiFi();
      break;

    case WIFI_CONNECTED:
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] 连接丢失，尝试重连...");
        wifiState = WIFI_DISCONNECTED;
        wifiAttemptCount = 0;
        wifiRetryAt = millis();
      }
      break;

    case WIFI_FAILED:
      // 射频硬复位：彻底关闭再重新初始化 WiFi 驱动，规避共存/驱动卡死
      WiFi.disconnect(true);   // wifiOff=true
      delay(100);
      WiFi.mode(WIFI_STA);
      WiFi.persistent(false);
      WiFi.setAutoReconnect(true);
      WiFi.setSleep(false);
      WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());
      wifiState = WIFI_CONNECTING;
      wifiAttemptStart = millis();
      wifiAttemptCount = 0;
      wifiBackoffMs = WIFI_RETRY_BACKOFF_MS;
      sendWifiConnecting();
      break;

    default:
      break;
  }
}

#if ENABLE_SERIAL_PROVISION
// ========== 串口配网协议（"@" 前缀单行 JSON，115200，非阻塞） ==========

void sendSerialEvent(const char* evt, const String& msg) {
  JsonDocument doc;
  doc["evt"] = evt;
  doc["msg"] = msg;
  String out;
  serializeJson(doc, out);
  Serial.println("@" + out);
}

void sendPong() {
  JsonDocument doc;
  doc["evt"] = "pong";
  doc["proto"] = 1;
  doc["fw"] = FW_VERSION;
  String out;
  serializeJson(doc, out);
  Serial.println("@" + out);
}

void sendWifiSaved(bool overrideOn) {
  JsonDocument doc;
  doc["evt"] = "wifi_saved";
  doc["override"] = overrideOn;
  String out;
  serializeJson(doc, out);
  Serial.println("@" + out);
}

void sendWifiConnecting() {
  if (wifiConnectEventSent) return;  // 重试期间不重复上报
  wifiConnectEventSent = true;
  JsonDocument doc;
  doc["evt"] = "wifi_connecting";
  String out;
  serializeJson(doc, out);
  Serial.println("@" + out);
}

void sendWifiFailed() {
  JsonDocument doc;
  doc["evt"] = "wifi_failed";
  doc["status"] = (int)WiFi.status();
  String out;
  serializeJson(doc, out);
  Serial.println("@" + out);
}

const char* wifiStateName() {
  switch (wifiState) {
    case WIFI_PROVISION:  return "provision";
    case WIFI_CONNECTING: return "connecting";
    case WIFI_CONNECTED:  return "connected";
    case WIFI_FAILED:     return "failed";
    default:              return "disconnected";
  }
}

void sendStatus() {
  JsonDocument doc;
  doc["evt"] = "status";
  doc["compile"] = wifiCompileCreds;
  doc["nvs"] = configMgr.hasWifiCredentials();
  doc["override"] = configMgr.isWifiOverride();
  doc["state"] = wifiStateName();
  if (WiFi.status() == WL_CONNECTED) doc["ip"] = WiFi.localIP().toString();
  String out;
  serializeJson(doc, out);
  Serial.println("@" + out);
}

// 配网/清除后：刷新凭据、应答、按结果进入连接或配网等待
void applyProvisionedCredentials() {
  resolveWifiCredentials();
  sendWifiSaved(configMgr.isWifiOverride());
  if (wifiSsid.length() == 0) {
    wifiState = WIFI_PROVISION;
    wifiConnectEventSent = false;
    sendStatus();
  } else {
    startWiFi();
  }
}

void processSerialCommand(const String& payload) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    sendSerialEvent("error", "bad json");
    return;
  }
  String cmd = doc["cmd"] | "";

  if (cmd == "ping") {
    sendPong();
  } else if (cmd == "wifi") {
    String ssid = doc["ssid"] | "";
    String pass = doc["pass"] | "";
    bool ovr = doc["override"] | false;
    if (ssid.length() == 0) {
      // 空 SSID：清除 NVS 凭据与覆盖标志
      configMgr.clearWifiCredentials();
    } else {
      configMgr.setWifiCredentials(ssid, pass);
      configMgr.setWifiOverride(ovr);
    }
    applyProvisionedCredentials();
  } else if (cmd == "wifi_clear") {
    configMgr.clearWifiCredentials();
    applyProvisionedCredentials();
  } else if (cmd == "status") {
    sendStatus();
  } else {
    sendSerialEvent("error", "unknown cmd");
  }
}

void handleSerialProvision() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\n') {
      if (serialLine.length() > 0) {
        if (serialLine[0] == '@') {
          processSerialCommand(serialLine.substring(1));
        }
        serialLine = "";
      }
    } else if (c != '\r') {
      if (serialLine.length() < 256) {
        serialLine += c;
      } else {
        serialLine = "";  // 超长丢弃，防缓冲区滥用
      }
    }
  }
}
#endif // ENABLE_SERIAL_PROVISION
