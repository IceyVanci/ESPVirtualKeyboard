#ifndef AUTO_MODE_H
#define AUTO_MODE_H

#include <Arduino.h>
#include "config.h"

// 按键事件结构体
struct KeyEvent {
  String keyName;        // 按键名称（如 "W", "Space"）
  bool pressed;          // true=按下, false=释放
  unsigned long timestamp; // millis() 时间戳
  uint32_t eventId;      // 递增事件ID，用于增量拉取
};

// 前向声明
class BleKeyboard;

// 自动模式配置结构体
struct AutoModeConfig {
  bool enabled;
  unsigned long minIntervalMs;
  unsigned long maxIntervalMs;
  unsigned long minHoldMs;
  unsigned long maxHoldMs;
  float moveForwardWeight;
  float moveBackWeight;
  float moveLeftWeight;
  float moveRightWeight;
  float turnLeftWeight;
  float turnRightWeight;
  float jumpWeight;
  float weightC;
  float weightZ;
  float idleWeight;

  AutoModeConfig() {
    enabled = true;
    minIntervalMs = DEFAULT_MIN_INTERVAL_MS;
    maxIntervalMs = DEFAULT_MAX_INTERVAL_MS;
    minHoldMs = DEFAULT_MIN_HOLD_MS;
    maxHoldMs = DEFAULT_MAX_HOLD_MS;
    moveForwardWeight = DEFAULT_WEIGHT_FORWARD;
    moveBackWeight = DEFAULT_WEIGHT_BACK;
    moveLeftWeight = DEFAULT_WEIGHT_LEFT;
    moveRightWeight = DEFAULT_WEIGHT_RIGHT;
    turnLeftWeight = DEFAULT_WEIGHT_TURN_LEFT;
    turnRightWeight = DEFAULT_WEIGHT_TURN_RIGHT;
    jumpWeight = DEFAULT_WEIGHT_JUMP;
    weightC = DEFAULT_WEIGHT_C;
    weightZ = DEFAULT_WEIGHT_Z;
    idleWeight = DEFAULT_WEIGHT_IDLE;
  }
};

// 自动模式状态枚举
enum AutoModeState {
  AUTO_IDLE,          // 等待下一次按键
  AUTO_PRESSING,      // 正在按住按键
  AUTO_RELEASING,     // 按键已释放，等待间隔
};

class AutoMode {
public:
  AutoMode(BleKeyboard* keyboard);

  void setConfig(const AutoModeConfig& config);
  AutoModeConfig getConfig();
  void setEnabled(bool enabled);
  bool isEnabled();
  void update();
  String getCurrentKeyName();  // 返回当前按下的键名（用于网页高亮）

  // 按键事件日志
  static const int EVENT_BUFFER_SIZE = 20;
  KeyEvent getEvent(int index) const;      // 获取环形缓冲区中的事件
  uint32_t getLastEventId() const;         // 获取最新事件ID
  int getEventCount() const;               // 获取缓冲区中事件总数

  // 按键统计
  uint32_t getKeyCount(int index) const;   // 获取某个键的按下次数
  uint32_t getTotalCount() const;          // 获取总按键次数

private:
  // 按键计数器（W,S,A,D,TL,TR,SP,C,Z,Idle）
  uint32_t        _keyCounts[10];
  BleKeyboard*    _keyboard;
  AutoModeConfig  _config;
  AutoModeState   _state;
  unsigned long   _lastActionTime;
  unsigned long   _nextInterval;
  unsigned long   _currentHoldTime;
  uint8_t         _currentKey;

  // 环形缓冲区
  KeyEvent        _events[EVENT_BUFFER_SIZE];
  int             _eventWritePos;   // 下一个写入位置
  uint32_t        _eventCounter;    // 递增事件ID

  void            logKeyEvent(const String& keyName, bool pressed);
  int             keyNameToIndex(const String& keyName);
  uint8_t         selectRandomKey();
  unsigned long   getRandomInterval();
  unsigned long   getRandomHoldTime();
  float           boxMullerRandom(float mean, float stddev);
};

#endif // AUTO_MODE_H