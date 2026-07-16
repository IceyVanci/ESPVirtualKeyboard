#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "config.h"

// 前向声明
class BleKeyboard;
class AutoMode;
struct AutoModeConfig;
class ConfigManager;

class WebController {
public:
  WebController(BleKeyboard* keyboard, AutoMode* autoMode, ConfigManager* configMgr);

  void begin();
  void handleClient();
  String getLocalIP();

private:
  WebServer*      _server;
  BleKeyboard*    _keyboard;
  AutoMode*       _autoMode;
  ConfigManager*  _configMgr;

  void handleRoot();
  void handleKeyPress();
  void handleKeyRelease();
  void handleConfig();
  void handleStatus();
  void handleEvents();
  void handleStats();

  // BLE 连接控制
  void handleBleReboot();

  // 配置槽位管理
  void handleSlots();
  void handleSlotSave();
  void handleSlotLoad();
  void handleSlotDelete();
  void handleSlotExport();
  void handleSlotImport();
  void handleConfigExport();
  void handleConfigImport();

  String generateHTML();
  String generateCSS();
  String generateJS();

  uint8_t mapWebKeyToHid(const String& key);
};

#endif // WEB_SERVER_H