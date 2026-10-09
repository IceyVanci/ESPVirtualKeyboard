#include "auto_mode.h"
#include "ble_keyboard.h"

AutoMode::AutoMode(BleKeyboard* keyboard)
  : _keyboard(keyboard), _state(AUTO_IDLE), _lastActionTime(0),
    _nextInterval(0), _currentHoldTime(0), _currentKey(0),
    _eventWritePos(0), _eventCounter(0), _totalCount(0) {
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

RandomKeyWeights AutoMode::getRandomWeights() {
  RandomKeyWeights w = {{
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
  }};
  return w;
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
    _nextInterval = gaussianInRange(_config.minIntervalMs, _config.maxIntervalMs);
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
        _currentKey = pickWeightedHidKey(getRandomWeights());

        if (_currentKey == 0) {
          // 选择了空闲，直接跳到等待下一轮
          _lastActionTime = now;
          _nextInterval = gaussianInRange(_config.minIntervalMs, _config.maxIntervalMs);
          Serial.println("[Auto] 空闲等待");
        } else {
          // 按下按键
          _state = AUTO_PRESSING;
          _keyboard->press(_currentKey);
          logKeyEvent(getCurrentKeyName(), true);
          _currentHoldTime = gaussianInRange(_config.minHoldMs, _config.maxHoldMs);
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
        _nextInterval = gaussianInRange(_config.minIntervalMs, _config.maxIntervalMs);
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

String AutoMode::getCurrentKeyName() {
  if (_state != AUTO_PRESSING || _currentKey == 0) {
    return "";
  }
  for (int i = 0; i < 10; i++) {
    if (RANDOM_KEY_HID[i] != 0 && RANDOM_KEY_HID[i] == _currentKey) {
      return RANDOM_KEY_NAME[i];
    }
  }
  return "";
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
  if (index < 10) return RANDOM_KEY_NAME[index];
  return "";
}

const char* AutoMode::indexToNameCStr(uint8_t index) {
  if (index < 10) return RANDOM_KEY_NAME[index];
  return "";
}

int AutoMode::keyNameToIndex(const String& keyName) {
  for (int i = 0; i < 10; i++) {
    if (keyName == RANDOM_KEY_NAME[i]) return i;
  }
  return -1;
}
