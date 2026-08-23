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
 * 1. 修改 config.h 中的 WiFi SSID 和密码
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
  WIFI_CONNECTING,
  WIFI_CONNECTED,
  WIFI_FAILED
};
WifiState      wifiState = WIFI_DISCONNECTED;
unsigned long  wifiAttemptStart = 0;
int            wifiAttemptCount = 0;

void startWiFi();
void handleWiFi();
void updateStatusLED();

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

  // 连接 WiFi（非阻塞，等待最多 10 秒以获得可用 IP）
  startWiFi();
  {
    unsigned long waitStart = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - waitStart < 10000) {
      delay(100);
    }
  }

  // NTP 时间同步
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  Serial.println("[NTP] 时间同步中...");

  // 初始化配置管理器（先于 BLE，读取蓝牙设备名称等 NVS 配置）
  configMgr.begin();

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

  Serial.println("================================");
  Serial.println("系统就绪！");
  Serial.print("控制面板: http://");
  Serial.println(webCtrl.getLocalIP());
  Serial.println("在蓝牙设置中配对: " + configMgr.getBleName());
  Serial.println("================================");
}

void loop() {
  webCtrl.handleClient();
  autoMode.update();
  seqMode.update();
  keyboard.checkStuck(WEB_KEY_STUCK_TIMEOUT_MS);
  updateStatusLED();
  handleWiFi();
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
void startWiFi() {
  Serial.print("[WiFi] 正在连接 ");
  Serial.print(WIFI_SSID);
  Serial.println("...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  wifiState = WIFI_CONNECTING;
  wifiAttemptStart = millis();
  wifiAttemptCount++;
}

void handleWiFi() {
  unsigned long now = millis();

  switch (wifiState) {
    case WIFI_CONNECTING:
      if (WiFi.status() == WL_CONNECTED) {
        wifiState = WIFI_CONNECTED;
        Serial.println();
        Serial.print("[WiFi] 已连接! IP: ");
        Serial.println(WiFi.localIP());
      } else if (now - wifiAttemptStart >= 10000) {
        if (wifiAttemptCount < 3) {
          startWiFi();  // 重试
        } else {
          wifiState = WIFI_FAILED;
          Serial.println();
          Serial.println("[WiFi] 连接失败，稍后自动重试");
        }
      }
      break;

    case WIFI_CONNECTED:
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] 连接丢失，尝试重连...");
        wifiState = WIFI_DISCONNECTED;
        wifiAttemptCount = 0;
        startWiFi();
      }
      break;

    case WIFI_FAILED:
      // 30 秒后重新尝试连接
      if (now - wifiAttemptStart >= 30000) {
        wifiAttemptCount = 0;
        startWiFi();
      }
      break;

    default:
      break;
  }
}
