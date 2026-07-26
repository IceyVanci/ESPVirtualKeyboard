# BLE 键盘模块

## 概述

`ble_keyboard.h/cpp` 实现了基于 ESP32 BLE HID 协议的键盘模拟器。它通过蓝牙低功耗将 ESP32-C3 模拟为标准的 HID 键盘设备，使电脑能够识别并接收按键输入。

## HID 报告描述符

键盘使用标准的 8 字节 HID 报告格式：

```
字节 0: 修饰键 (Modifier) - 8 位，每位对应一个修饰键
  位 0: Left Control
  位 1: Left Shift
  位 2: Left Alt
  位 3: Left GUI (Windows/Command)
  位 4: Right Control
  位 5: Right Shift
  位 6: Right Alt
  位 7: Right GUI

字节 1: 保留字节 (Reserved) - 恒为 0

字节 2-7: 按键数组 (Key Codes) - 最多同时按下 6 个键
  每个字节存放一个 HID 键码（0x00 = 无按键）
```

## 类结构

```
BleKeyboard
├── 构造函数: BleKeyboard(const char* deviceName)
├── begin()      - 初始化 BLE 设备和 HID 服务
├── end()        - 停止 BLE 键盘
├── isConnected() - 检查 BLE 连接状态
├── getState()   - 获取 BLE 状态枚举
│
├── press(keyCode)           - 按下指定键
├── release(keyCode)         - 释放指定键
├── pressAndRelease(keyCode, holdMs) - 按下并释放（指定持续时间）
├── releaseAll()             - 释放所有按键
├── pressWithModifier(modifier, keyCode)   - 带修饰键按下
├── releaseWithModifier(modifier, keyCode) - 带修饰键释放
│
├── disconnectAndReboot() - 断开连接并重新广播
│
└── 内部方法:
    ├── sendReport()     - 发送 HID 报告
    ├── clearReport()    - 清空报告缓冲区
    └── startAdvertising() - 开始 BLE 广播
```

## BLE 状态枚举

```
BLE_STATE_STOPPED      (0) - 已停止广播
BLE_STATE_ADVERTISING  (1) - 广播中（等待连接）
BLE_STATE_CONNECTED    (2) - 已连接
```

## 连接生命周期

```
BleKeyboard::begin()
    ↓
startAdvertising()
    ↓
BLE_STATE_ADVERTISING ──→ 电脑扫描并配对
    ↓                        ↓
    ↓              onConnect() 回调
    ↓                        ↓
    ↓              BLE_STATE_CONNECTED
    ↓                        ↓
    ↓              onDisconnect() 回调
    ↓                        ↓
    └──→ 重新 startAdvertising()
```

## 按键管理详解

### press(keyCode)
1. 从 `_keyReport[2]` 到 `_keyReport[7]` 查找空位或已存在的键码
2. 如果已存在，直接发送报告并返回（防止重复按下）
3. 如果找到空位，填入键码并发送报告
4. 将 LED D4 置高（按键闪烁由主循环的 `updateStatusLED()` 管理关闭）

### release(keyCode)
1. 在 `_keyReport[2-7]` 中查找指定键码
2. 找到后移除该键码，将后续键码前移填补空位
3. 发送更新后的报告

### releaseAll()
1. 清空整个 `_keyReport` 缓冲区
2. 发送空报告（所有按键释放）

### pressWithModifier / releaseWithModifier
1. 在 `_keyReport[0]` 修饰键字节中设置/清除对应位
2. 调用 press/release 处理普通键码

## 安全配对

```cpp
BLESecurity* pSecurity = new BLESecurity();
pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);  // 绑定模式
pSecurity->setCapability(ESP_IO_CAP_NONE);            // 无 IO 能力
pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
```

- 使用 `ESP_LE_AUTH_BOND` 绑定模式，配对后无需重复配对
- `ESP_IO_CAP_NONE` 表示无显示屏和键盘，配对过程无需用户交互

## 断开重连机制

`disconnectAndReboot()` 提供两种场景的处理：

1. **已连接状态**：调用 `pServer->disconnect(0)` 触发 `onDisconnect` 回调，自动重启广播
2. **未连接状态**：直接停止广播并重新开始

---

# BLE Keyboard Module

## Overview

The `ble_keyboard.h/cpp` implements a keyboard emulator based on the ESP32 BLE HID protocol. It emulates a standard HID keyboard device via Bluetooth Low Energy, allowing computers to recognize and receive key input.

## HID Report Descriptor

The keyboard uses a standard 8-byte HID report format:

```
Byte 0: Modifier - 8 bits, each bit corresponds to a modifier key
  Bit 0: Left Control
  Bit 1: Left Shift
  Bit 2: Left Alt
  Bit 3: Left GUI (Windows/Command)
  Bit 4: Right Control
  Bit 5: Right Shift
  Bit 6: Right Alt
  Bit 7: Right GUI

Byte 1: Reserved - Always 0

Bytes 2-7: Key Codes - Up to 6 simultaneous key presses
  Each byte holds a HID keycode (0x00 = no key)
```

## Class Structure

```
BleKeyboard
├── Constructor: BleKeyboard(const char* deviceName)
├── begin()      - Initialize BLE device and HID service
├── end()        - Stop BLE keyboard
├── isConnected() - Check BLE connection status
├── getState()   - Get BLE state enum
│
├── press(keyCode)           - Press a key
├── release(keyCode)         - Release a key
├── pressAndRelease(keyCode, holdMs) - Press and release (with duration)
├── releaseAll()             - Release all keys
├── pressWithModifier(modifier, keyCode)   - Press with modifier
├── releaseWithModifier(modifier, keyCode) - Release with modifier
│
├── disconnectAndReboot() - Disconnect and re-advertise
│
└── Internal methods:
    ├── sendReport()     - Send HID report
    ├── clearReport()    - Clear report buffer
    └── startAdvertising() - Start BLE advertising
```

## BLE State Enum

```
BLE_STATE_STOPPED      (0) - Stopped advertising
BLE_STATE_ADVERTISING  (1) - Advertising (waiting for connection)
BLE_STATE_CONNECTED    (2) - Connected
```

## Connection Lifecycle

```
BleKeyboard::begin()
    ↓
startAdvertising()
    ↓
BLE_STATE_ADVERTISING ──→ Computer scans and pairs
    ↓                        ↓
    ↓              onConnect() callback
    ↓                        ↓
    ↓              BLE_STATE_CONNECTED
    ↓                        ↓
    ↓              onDisconnect() callback
    ↓                        ↓
    └──→ Restart advertising
```

## Key Management Details

### press(keyCode)
1. Search `_keyReport[2]` through `_keyReport[7]` for existing keycode or empty slot
2. If already present, send report and return (prevent duplicate press)
3. If empty slot found, fill keycode and send report
4. Set LED D4 high (flash timing managed by `updateStatusLED()` in main loop)

### release(keyCode)
1. Search `_keyReport[2-7]` for the specified keycode
2. Remove it and shift remaining keycodes forward
3. Send updated report

### releaseAll()
1. Clear entire `_keyReport` buffer
2. Send empty report (all keys released)

### pressWithModifier / releaseWithModifier
1. Set/clear corresponding bits in `_keyReport[0]` modifier byte
2. Call press/release for the regular keycode

## Security Pairing

```cpp
BLESecurity* pSecurity = new BLESecurity();
pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);  // Bonding mode
pSecurity->setCapability(ESP_IO_CAP_NONE);            // No I/O capability
pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
```

- Uses `ESP_LE_AUTH_BOND` bonding mode, no re-pairing needed after initial pairing
- `ESP_IO_CAP_NONE` means no display or keyboard, pairing process requires no user interaction

## Disconnect and Reconnect

`disconnectAndReboot()` handles two scenarios:

1. **Connected state**: Calls `pServer->disconnect(0)` to trigger `onDisconnect` callback, which auto-restarts advertising
2. **Disconnected state**: Stops advertising directly and restarts