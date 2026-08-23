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
class SequenceMode;

class WebController {
public:
  WebController(BleKeyboard* keyboard, AutoMode* autoMode, ConfigManager* configMgr, SequenceMode* seqMode);

  void begin();
  void handleClient();
  String getLocalIP();

private:
  WebServer*      _server;
  BleKeyboard*    _keyboard;
  AutoMode*       _autoMode;
  ConfigManager*  _configMgr;
  SequenceMode*   _seqMode;

  void handleRoot();
  void handleKeyPress();
  void handleKeyRelease();
  void handleConfig();
  void handleStatus();
  void handleEvents();
  void handleStats();

  // BLE 连接控制
  void handleBleReboot();
  void handleBleName();

  // 认证（ENABLE_WEB_AUTH 启用时生效）
  void handleLogin();
  void handleLogout();
  void handleAuthChange();
  bool authGuard();   // 未授权时发 401 并返回 false；功能关闭时恒为 true

  // 配置槽位管理
  void handleSlots();
  void handleSlotSave();
  void handleSlotLoad();
  void handleSlotDelete();
  void handleSlotExport();
  void handleSlotImport();
  void handleConfigExport();
  void handleConfigImport();

  // 顺序模式
  void handleSeqConfig();
  void handleSeqPlay();
  void handleSeqSlots();
  void handleSeqSlotSave();
  void handleSeqSlotLoad();
  void handleSeqSlotDelete();
  void handleSeqSlotImport();
  void handleSeqSlotExport();

  // 全部导出
  void handleExportAll();

  uint8_t mapWebKeyToHid(const String& key);

#ifdef ENABLE_WEB_AUTH
  String generateToken();
#endif

  // 认证状态（仅 ENABLE_WEB_AUTH 时占用 RAM）
#ifdef ENABLE_WEB_AUTH
  String        _authToken;       // 当前有效会话 token（空=未登录）
  unsigned int  _authFailCount;   // 连续登录失败计数
  unsigned long _authLockUntil;   // 锁定截止时间
#endif
};

#endif // WEB_SERVER_H