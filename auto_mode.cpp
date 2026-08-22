#include "auto_mode.h"
#include "ble_keyboard.h"
#include <esp_system.h>

AutoMode::AutoMode(BleKeyboard* keyboard)
  : _keyboard(keyboard), _state(AUTO_IDLE), _lastActionTime(0),
    _nextInterval(0), _currentHoldTime(0), _currentKey(0),
    _eventWritePos(0), _eventCounter(0), _totalCount(0) {
  randomSeed(esp_random()); // 使用硬件随机源初始化随机数种子
  memset(_keyCounts, 0, sizeof(_keyCounts));
}

void AutoMode::setConfig(const AutoModeConfig& config) {
  _config = config;

  // 参数钳制（与 JSON 导入规则一致）
  if (_config.minIntervalMs < 100) _config.minIntervalMs = 100;
  if (_config.maxIntervalMs > 30000) _config.maxIntervalMs = 30000;
  if (_config.minIntervalMs > _config.maxIntervalMs) {
    unsigned long tmp = _config.minIntervalMs;
    _config.minIntervalMs = _config.maxIntervalMs;
    _config.maxIntervalMs = tmp;
  }
  if (_config.minHoldMs < 10) _config.minHoldMs = 10;
  if (_config.maxHoldMs > 5000) _config.maxHoldMs = 5000;
  if (_config.minHoldMs > _config.maxHoldMs) {
    unsigned long tmp = _config.minHoldMs;
    _config.minHoldMs = _config.maxHoldMs;
    _config.maxHoldMs = tmp;
  }
  auto clampWeight = [](float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); };
  _config.moveForwardWeight = clampWeight(_config.moveForwardWeight);
  _config.moveBackWeight = clampWeight(_config.moveBackWeight);
  _config.moveLeftWeight = clampWeight(_config.moveLeftWeight);
  _config.moveRightWeight = clampWeight(_config.moveRightWeight);
  _config.turnLeftWeight = clampWeight(_config.turnLeftWeight);
  _config.turnRightWeight = clampWeight(_config.turnRightWeight);
  _config.jumpWeight = clampWeight(_config.jumpWeight);
  _config.weightC = clampWeight(_config.weightC);
  _config.weightZ = clampWeight(_config.weightZ);
  _config.idleWeight = clampWeight(_config.idleWeight);

  // 归一化兜底：如果权重总和 > 1，按比例缩小
  float sum = _config.moveForwardWeight + _config.moveBackWeight +
              _config.moveLeftWeight + _config.moveRightWeight +
              _config.turnLeftWeight + _config.turnRightWeight +
              _config.jumpWeight + _config.weightC + _config.weightZ +
              _config.idleWeight;
  if (sum > 1.0f && sum > 0.0f) {
    float scale = 1.0f / sum;
    _config.moveForwardWeight *= scale;
    _config.moveBackWeight *= scale;
    _config.moveLeftWeight *= scale;
    _config.moveRightWeight *= scale;
    _config.turnLeftWeight *= scale;
    _config.turnRightWeight *= scale;
    _config.jumpWeight *= scale;
    _config.weightC *= scale;
    _config.weightZ *= scale;
    _config.idleWeight *= scale;
    Serial.print("[Auto] 权重总和 ");
    Serial.print(sum, 2);
    Serial.print(" > 1.0，已归一化至 ");
    Serial.println(scale, 4);
  }
  Serial.println("[Auto] 配置已更新");
}

AutoModeConfig AutoMode::getConfig() {
  return _config;
}

void AutoMode::setEnabled(bool enabled) {
  _config.enabled = enabled;
  if (!enabled) {
    // 关闭自动模式时，释放所有按键
    _keyboard->releaseAll();
    _state = AUTO_IDLE;
    Serial.println("[Auto] 自动模式已关闭");
  } else {
    _lastActionTime = millis();
    _nextInterval = getRandomInterval();
    Serial.println("[Auto] 自动模式已开启");
  }
}

bool AutoMode::isEnabled() {
  return _config.enabled;
}

void AutoMode::update() {
  if (!_config.enabled || !_keyboard->isConnected()) {
    return;
  }

  unsigned long now = millis();

  switch (_state) {
    case AUTO_IDLE:
      // 等待间隔时间到达后开始下一次按键
      if (now - _lastActionTime >= _nextInterval) {
        _currentKey = selectRandomKey();

        if (_currentKey == 0) {
          // 选择了空闲，直接跳到等待下一轮
          _lastActionTime = now;
          _nextInterval = getRandomInterval();
          Serial.println("[Auto] 空闲等待");
        } else {
          // 按下按键
          _state = AUTO_PRESSING;
          _keyboard->press(_currentKey);
          logKeyEvent(getCurrentKeyName(), true);
          _currentHoldTime = getRandomHoldTime();
          _lastActionTime = now;
          Serial.print("[Auto] 按下: 0x");
          Serial.println(_currentKey, HEX);
        }
      }
      break;

    case AUTO_PRESSING:
      // 持续按住，直到 holdTime 到期
      if (now - _lastActionTime >= _currentHoldTime) {
        _keyboard->release(_currentKey);
        logKeyEvent(getCurrentKeyName(), false);
        _lastActionTime = now;
        _nextInterval = getRandomInterval();
        _state = AUTO_IDLE;
        Serial.print("[Auto] 释放: 0x");
        Serial.println(_currentKey, HEX);
      }
      break;

    default:
      _state = AUTO_IDLE;
      break;
  }
}

uint8_t AutoMode::selectRandomKey() {
  // 收集所有权重
  float weights[10] = {
    _config.moveForwardWeight,  // 0: W
    _config.moveBackWeight,     // 1: S
    _config.moveLeftWeight,     // 2: A
    _config.moveRightWeight,    // 3: D
    _config.turnLeftWeight,     // 4: Left Arrow
    _config.turnRightWeight,    // 5: Right Arrow
    _config.jumpWeight,         // 6: Space
    _config.weightC,            // 7: C
    _config.weightZ,            // 8: Z
    _config.idleWeight          // 9: Idle (no key)
  };

  uint8_t keys[10] = {
    HID_KEY_W,
    HID_KEY_S,
    HID_KEY_A,
    HID_KEY_D,
    HID_KEY_LEFT_ARROW,
    HID_KEY_RIGHT_ARROW,
    HID_KEY_SPACE,
    HID_KEY_C,
    HID_KEY_Z,
    0x00 // 空闲，不按任何键
  };

  // 计算权重总和
  float totalWeight = 0;
  for (int i = 0; i < 10; i++) {
    totalWeight += weights[i];
  }

  if (totalWeight <= 0) return 0x00;

  // 加权随机选择
  float r = (float)random(0, 10000) / 10000.0 * totalWeight;
  float cumulative = 0;
  for (int i = 0; i < 10; i++) {
    cumulative += weights[i];
    if (r <= cumulative) {
      return keys[i];
    }
  }

  return keys[0]; // 默认返回 W
}

unsigned long AutoMode::getRandomInterval() {
  // 使用 Box-Muller 变换生成近似正态分布的随机数
  float mean = (_config.minIntervalMs + _config.maxIntervalMs) / 2.0;
  float stddev = (_config.maxIntervalMs - _config.minIntervalMs) / 6.0;
  float interval = boxMullerRandom(mean, stddev);

  // 限制在范围内
  if (interval < _config.minIntervalMs) interval = _config.minIntervalMs;
  if (interval > _config.maxIntervalMs) interval = _config.maxIntervalMs;

  return (unsigned long)interval;
}

unsigned long AutoMode::getRandomHoldTime() {
  float mean = (_config.minHoldMs + _config.maxHoldMs) / 2.0;
  float stddev = (_config.maxHoldMs - _config.minHoldMs) / 6.0;
  float holdTime = boxMullerRandom(mean, stddev);

  if (holdTime < _config.minHoldMs) holdTime = _config.minHoldMs;
  if (holdTime > _config.maxHoldMs) holdTime = _config.maxHoldMs;

  return (unsigned long)holdTime;
}

String AutoMode::getCurrentKeyName() {
  if (_state != AUTO_PRESSING || _currentKey == 0) {
    return "";
  }
  switch (_currentKey) {
    case HID_KEY_W: return "w";
    case HID_KEY_S: return "s";
    case HID_KEY_A: return "a";
    case HID_KEY_D: return "d";
    case HID_KEY_LEFT_ARROW: return "left";
    case HID_KEY_RIGHT_ARROW: return "right";
    case HID_KEY_SPACE: return "space";
    case HID_KEY_C: return "c";
    case HID_KEY_Z: return "z";
    default: return "";
  }
}

float AutoMode::boxMullerRandom(float mean, float stddev) {
  // Box-Muller 变换：将均匀分布转换为正态分布
  float u1 = (float)random(1, 10000) / 10000.0; // 避免 0
  float u2 = (float)random(0, 10000) / 10000.0;
  float z = sqrt(-2.0 * log(u1)) * cos(2.0 * PI * u2);
  return mean + z * stddev;
}

// ---- 按键事件日志 ----

void AutoMode::logKeyEvent(const String& keyName, bool pressed) {
  _eventCounter++;
  _events[_eventWritePos].keyName = keyName;
  _events[_eventWritePos].keyIndex = (uint8_t)keyNameToIndex(keyName);
  _events[_eventWritePos].pressed = pressed;
  _events[_eventWritePos].timestamp = millis();
  _events[_eventWritePos].eventId = _eventCounter;
  _eventWritePos = (_eventWritePos + 1) % EVENT_BUFFER_SIZE;
  // 按下时递增计数器
  if (pressed) {
    int idx = keyNameToIndex(keyName);
    if (idx >= 0 && idx < 10) { _keyCounts[idx]++; _totalCount++; }
  }
}

KeyEvent AutoMode::getEvent(int index) const {
  // index 0 = 最旧的事件，index = getEventCount()-1 = 最新的事件
  // 环形缓冲区：如果没满，从 0 开始；如果满了，从 _eventWritePos 开始
  int count = getEventCount();
  if (index < 0 || index >= count) {
    KeyEvent empty;
    empty.keyName = "";
    empty.pressed = false;
    empty.timestamp = 0;
    empty.eventId = 0;
    return empty;
  }
  int startPos;
  if (count < EVENT_BUFFER_SIZE) {
    startPos = 0;
  } else {
    startPos = _eventWritePos;
  }
  int readPos = (startPos + index) % EVENT_BUFFER_SIZE;
  return _events[readPos];
}

uint32_t AutoMode::getLastEventId() const {
  return _eventCounter;
}

int AutoMode::getEventCount() const {
  if (_eventCounter < EVENT_BUFFER_SIZE) {
    return (int)_eventCounter;
  }
  return EVENT_BUFFER_SIZE;
}

uint32_t AutoMode::getKeyCount(int index) const {
  if (index >= 0 && index < 10) return _keyCounts[index];
  return 0;
}

uint32_t AutoMode::getTotalCount() const {
  return _totalCount;
}

// ========== 静态工具方法：索引/名称转换 ==========

String AutoMode::indexToName(uint8_t index) {
  switch (index) {
    case 0: return "w";
    case 1: return "s";
    case 2: return "a";
    case 3: return "d";
    case 4: return "left";
    case 5: return "right";
    case 6: return "space";
    case 7: return "c";
    case 8: return "z";
    case 9: return "";
    default: return "";
  }
}

const char* AutoMode::indexToNameCStr(uint8_t index) {
  switch (index) {
    case 0: return "w";
    case 1: return "s";
    case 2: return "a";
    case 3: return "d";
    case 4: return "left";
    case 5: return "right";
    case 6: return "space";
    case 7: return "c";
    case 8: return "z";
    case 9: return "";
    default: return "";
  }
}

int AutoMode::keyNameToIndex(const String& keyName) {
  if (keyName == "w") return 0;
  if (keyName == "s") return 1;
  if (keyName == "a") return 2;
  if (keyName == "d") return 3;
  if (keyName == "left") return 4;
  if (keyName == "right") return 5;
  if (keyName == "space") return 6;
  if (keyName == "c") return 7;
  if (keyName == "z") return 8;
  if (keyName == "") return 9;  // idle
  return -1;
}
