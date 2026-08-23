#ifndef BLE_KEYBOARD_H
#define BLE_KEYBOARD_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <BLEHIDDevice.h>
#include "config.h"

// HID 报告描述符 - 标准键盘
static const uint8_t hidReportDescriptor[] = {
  USAGE_PAGE(1),      0x01,       // Generic Desktop
  USAGE(1),           0x06,       // Keyboard
  COLLECTION(1),      0x01,       // Application
  REPORT_ID(1),       0x01,       // Report ID 1

  USAGE_PAGE(1),      0x07,       // Keyboard/Keypad
  USAGE_MINIMUM(1),   0xE0,       // Left Control
  USAGE_MAXIMUM(1),   0xE7,       // Right GUI
  LOGICAL_MINIMUM(1), 0x00,
  LOGICAL_MAXIMUM(1), 0x01,
  REPORT_SIZE(1),     0x01,
  REPORT_COUNT(1),    0x08,
  HIDINPUT(1),        0x02,       // Data, Variable, Absolute (modifier byte)

  REPORT_COUNT(1),    0x01,
  REPORT_SIZE(1),     0x08,
  HIDINPUT(1),        0x01,       // Constant (reserved byte)

  REPORT_COUNT(1),    0x06,
  REPORT_SIZE(1),     0x08,
  LOGICAL_MINIMUM(1), 0x00,
  LOGICAL_MAXIMUM(1), 0x65,       // 101 keys
  USAGE_PAGE(1),      0x07,
  USAGE_MINIMUM(1),   0x00,
  USAGE_MAXIMUM(1),   0x65,
  HIDINPUT(1),        0x00,       // Data, Array

  END_COLLECTION(0)
};

// BLE 状态枚举
enum BleState {
  BLE_STATE_STOPPED,    // 已停止广播
  BLE_STATE_ADVERTISING,// 广播中（等待连接）
  BLE_STATE_CONNECTED   // 已连接
};

class BleKeyboard {
public:
  BleKeyboard(const char* deviceName = BLE_DEVICE_NAME);

  void begin();
  void end();
  bool isConnected();
  BleState getState();

  // 设置蓝牙设备名称（需在 begin() 前调用，或调用后 end()+begin() 生效）
  void setDeviceName(const String& name);
  const char* getDeviceName() const { return _deviceName; }

  // 键盘操作
  void press(uint8_t keyCode);
  void release(uint8_t keyCode);
  void pressAndRelease(uint8_t keyCode, unsigned long holdMs = 100);
  void releaseAll();
  void pressWithModifier(uint8_t modifier, uint8_t keyCode);
  void releaseWithModifier(uint8_t modifier, uint8_t keyCode);

  // BLE 连接控制：断开当前连接并重新广播（允许重新配对）
  void disconnectAndReboot();

  // 卡键安全超时：BLE 连接下若超过 timeoutMs 无任何按键活动且仍有按键被按住，自动释放所有按键
  void checkStuck(unsigned long timeoutMs);

private:
  BLEServer*            _pServer;
  BLEHIDDevice*         _hid;
  BLECharacteristic*    _inputKeyboard;
  char                  _deviceName[32];
  BleState              _state;
  uint8_t               _keyReport[8]; // [modifier, reserved, key1..key6]
  unsigned long         _lastKeyActivity; // 最近一次按键活动时间
  bool                  _advConfigured;   // 广播服务 UUID 是否已配置（避免重复追加）

  void sendReport();
  void clearReport();
  void startAdvertising();

  // 回调类
  class ServerCallbacks : public BLEServerCallbacks {
  public:
    ServerCallbacks(BleKeyboard* parent) : _parent(parent) {}
    void onConnect(BLEServer* pServer) override;
    void onDisconnect(BLEServer* pServer) override;
  private:
    BleKeyboard* _parent;
  };
};

#endif // BLE_KEYBOARD_H