#include "ble_keyboard.h"

// ========== 回调实现 ==========

void BleKeyboard::ServerCallbacks::onConnect(BLEServer* pServer) {
  _parent->_state = BLE_STATE_CONNECTED;
  // 防御性清空报告，确保重连瞬间不会发送断开期间残留的按键
  _parent->clearReport();
  _parent->sendReport();
  Serial.println("[BLE] 设备已连接");
}

void BleKeyboard::ServerCallbacks::onDisconnect(BLEServer* pServer) {
  _parent->_state = BLE_STATE_ADVERTISING;
  Serial.println("[BLE] 设备已断开");
  _parent->releaseAll();
  // 断开后重新开始广播
  pServer->getAdvertising()->start();
}

// ========== BleKeyboard 实现 ==========

BleKeyboard::BleKeyboard(const char* deviceName)
  : _pServer(nullptr), _hid(nullptr), _inputKeyboard(nullptr),
    _state(BLE_STATE_STOPPED), _lastKeyActivity(0), _advConfigured(false) {
  strncpy(_deviceName, deviceName, sizeof(_deviceName) - 1);
  _deviceName[sizeof(_deviceName) - 1] = '\0';
  memset(_keyReport, 0, sizeof(_keyReport));
}

void BleKeyboard::setDeviceName(const String& name) {
  String trimmed = name.substring(0, sizeof(_deviceName) - 1);
  strncpy(_deviceName, trimmed.c_str(), sizeof(_deviceName) - 1);
  _deviceName[sizeof(_deviceName) - 1] = '\0';
}

void BleKeyboard::begin() {
  Serial.println("[BLE] 初始化蓝牙...");
  _advConfigured = false;  // 重新初始化后需重新配置广播

  // 初始化 BLE 设备
  BLEDevice::init(_deviceName);
  BLEDevice::setMTU(23);

  // 创建 BLE 服务器
  _pServer = BLEDevice::createServer();
  _pServer->setCallbacks(new ServerCallbacks(this));

  // 创建 HID 设备
  _hid = new BLEHIDDevice(_pServer);

  // 设置输入报告特征
  _inputKeyboard = _hid->inputReport(1); // Report ID 1

  // 设置 HID 报告描述符
  _hid->pnp(0x02, 0xe502, 0xa111, 0x0210);
  _hid->hidInfo(0x00, 0x01);

  // 设置安全参数
  BLESecurity* pSecurity = new BLESecurity();
  pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);
  pSecurity->setCapability(ESP_IO_CAP_NONE);
  pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

  // 设置报告描述符
  _hid->reportMap((uint8_t*)hidReportDescriptor, sizeof(hidReportDescriptor));
  _hid->startServices();

  // 开始广播
  startAdvertising();

  Serial.print("[BLE] 键盘 '");
  Serial.print(_deviceName);
  Serial.println("' 初始化完成，等待连接...");
}

void BleKeyboard::end() {
  releaseAll();
  BLEDevice::deinit(true);
  _state = BLE_STATE_STOPPED;
  Serial.println("[BLE] 键盘已停止");
}

bool BleKeyboard::isConnected() {
  return _state == BLE_STATE_CONNECTED;
}

BleState BleKeyboard::getState() {
  return _state;
}

// ========== 键盘操作 ==========

void BleKeyboard::sendReport() {
  if (_state == BLE_STATE_CONNECTED && _inputKeyboard) {
    _inputKeyboard->setValue(_keyReport, sizeof(_keyReport));
    _inputKeyboard->notify();
  }
}

void BleKeyboard::clearReport() {
  memset(_keyReport, 0, sizeof(_keyReport));
}

void BleKeyboard::press(uint8_t keyCode) {
  if (_state != BLE_STATE_CONNECTED) return;  // 未连接时不写入缓冲，避免重连鬼键
  _lastKeyActivity = millis();
  // 修饰键哨兵码：按位写入修饰字节
  if (keyCode >= HID_SENTINEL_LCTRL && keyCode <= HID_SENTINEL_RGUI) {
    _keyReport[0] |= (uint8_t)(1 << (keyCode - HID_SENTINEL_LCTRL));
    sendReport();
    return;
  }
  for (int i = 2; i < 8; i++) {
    if (_keyReport[i] == keyCode) {
      sendReport();
      return;
    }
    if (_keyReport[i] == 0) {
      _keyReport[i] = keyCode;
      sendReport();
      // LED D4 闪烁指示按下（仅按下时闪，释放不闪）
      digitalWrite(LED_D4, HIGH);
      return;
    }
  }
  Serial.printf("[BLE] 按键报告已满（6键上限），忽略按键 0x%02X\n", keyCode);
  sendReport();
}

void BleKeyboard::release(uint8_t keyCode) {
  if (_state != BLE_STATE_CONNECTED) return;
  _lastKeyActivity = millis();
  // 修饰键哨兵码：清除修饰字节对应位
  if (keyCode >= HID_SENTINEL_LCTRL && keyCode <= HID_SENTINEL_RGUI) {
    _keyReport[0] &= (uint8_t)~(1 << (keyCode - HID_SENTINEL_LCTRL));
    sendReport();
    return;
  }
  for (int i = 2; i < 8; i++) {
    if (_keyReport[i] == keyCode) {
      _keyReport[i] = 0;
      for (int j = i; j < 7; j++) {
        _keyReport[j] = _keyReport[j + 1];
        if (_keyReport[j + 1] == 0) break;
      }
      _keyReport[7] = 0;
      break;
    }
  }
  sendReport();
}

void BleKeyboard::pressAndRelease(uint8_t keyCode, unsigned long holdMs) {
  press(keyCode);
  delay(holdMs);
  release(keyCode);
}

void BleKeyboard::releaseAll() {
  _lastKeyActivity = millis();
  clearReport();
  sendReport();
}

void BleKeyboard::checkStuck(unsigned long timeoutMs) {
  if (_state != BLE_STATE_CONNECTED) return;
  bool anyKey = false;
  for (int i = 0; i < 8; i++) {
    if (_keyReport[i]) { anyKey = true; break; }
  }
  if (anyKey && (millis() - _lastKeyActivity >= timeoutMs)) {
    Serial.println("[BLE] 检测到卡键，自动释放所有按键");
    releaseAll();
  }
}

void BleKeyboard::pressWithModifier(uint8_t modifier, uint8_t keyCode) {
  if (_state != BLE_STATE_CONNECTED) return;
  _keyReport[0] |= modifier;
  press(keyCode);
}

void BleKeyboard::releaseWithModifier(uint8_t modifier, uint8_t keyCode) {
  if (_state != BLE_STATE_CONNECTED) return;
  release(keyCode);
  _keyReport[0] &= ~modifier;
}

// ========== BLE 连接控制 ==========

void BleKeyboard::disconnectAndReboot() {
  Serial.println("[BLE] 断开连接并重新广播...");

  // 释放所有按键
  releaseAll();

  // 断开当前连接（onDisconnect 回调会自动重新广播）
  if (_pServer && _state == BLE_STATE_CONNECTED) {
    int connCount = _pServer->getConnectedCount();
    if (connCount > 0) {
      _pServer->disconnect(0);
    }
    // disconnect() 会触发 onDisconnect 回调，自动重启广播
    // 等待一小段时间确保断开完成
    // 注：此处为阻塞等待，仅在用户手动操作"配对"时触发，可接受
    delay(200);
  } else {
    // 未连接状态下，直接重启广播
    _pServer->getAdvertising()->stop();
    delay(100);
    startAdvertising();
  }

  Serial.println("[BLE] 已重新广播，可以重新配对");
}

void BleKeyboard::startAdvertising() {
  if (_pServer) {
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->setAppearance(HID_KEYBOARD);
    if (!_advConfigured) {
      // 仅首次调用时追加服务 UUID；deinit 会释放广播对象，无需手动清除
      pAdvertising->addServiceUUID(_hid->hidService()->getUUID());
      _advConfigured = true;
    }
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMaxPreferred(0x12);
    pAdvertising->start();
    _state = BLE_STATE_ADVERTISING;
    Serial.println("[BLE] 开始广播");
  }
}