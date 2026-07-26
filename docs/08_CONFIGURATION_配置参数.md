# 配置参数

## 概述

`config.h` 包含项目的所有可配置参数，包括 WiFi、BLE、HID 键码、LED、NTP 和自动模式默认参数。

## WiFi 配置

```cpp
#define WIFI_SSID     "Debug"
#define WIFI_PASSWORD "12345678"
```

**说明**：ESP32 将作为 STA 模式连接到此 WiFi 网络。
**注意**：
- ESP32-C3 仅支持 2.4GHz 频段，不支持 5GHz
- SSID 和密码区分大小写
- 连接失败时，系统会在 20 秒后超时，仍可访问 Web 控制面板（但无法通过外网访问）

## BLE 配置

```cpp
#define BLE_DEVICE_NAME "ESP Virtual Keyboard"
```

**说明**：蓝牙广播和配对时显示的设备名称。
**注意**：名称过长可能导致某些蓝牙扫描器截断显示。

## Web 服务器配置

```cpp
#define WEB_SERVER_PORT 80
```

**说明**：HTTP 服务器监听端口。通常使用 80 端口，无需在浏览器中指定端口号。

## 串口配置

```cpp
#define SERIAL_BAUD_RATE 115200
```

**说明**：串口通信波特率。串口监视器需设置为此值。

## LED 配置

| 宏定义 | 值 | 说明 |
|--------|-----|------|
| `LED_D4` | 12 | GPIO12 - 按键指示灯（按下时闪烁） |
| `LED_D5` | 13 | GPIO13 - 状态指示灯（BLE 状态） |
| `LED_BLINK_FAST_MS` | 200 | 开机初始化快闪间隔 |
| `LED_BLINK_SLOW_MS` | 1000 | 自动模式运行中慢闪间隔 |
| `LED_BLINK_ADVERT_MS` | 500 | BLE 广播中闪烁间隔 |
| `LED_KEY_FLASH_MS` | 50 | 按键按下时 D4 闪烁持续时间 |

### LED 状态指示说明

| 状态 | D5 状态灯 | D4 按键灯 |
|------|-----------|-----------|
| BLE 未初始化 | 熄灭 | - |
| BLE 广播中 | 500ms 闪烁 | - |
| BLE 已连接（手动模式） | 常亮 | - |
| BLE 已连接（自动模式） | 1000ms 慢闪 | - |
| 按键按下时 | - | 点亮 50ms |

## NTP 时间同步配置

```cpp
#define NTP_SERVER "ntp.aliyun.com"
#define GMT_OFFSET_SEC 28800    // UTC+8 (中国)
#define DAYLIGHT_OFFSET_SEC 0
```

**说明**：
- `NTP_SERVER`：NTP 服务器地址，可根据地区修改（如 `pool.ntp.org`、`time.nist.gov`）
- `GMT_OFFSET_SEC`：时区偏移（秒），中国为 UTC+8 = 28800 秒
- `DAYLIGHT_OFFSET_SEC`：夏令时偏移（秒），中国不实行夏令时

## 自动模式默认配置

### 时间参数

| 宏定义 | 默认值 | 说明 | 推荐范围 |
|--------|--------|------|----------|
| `DEFAULT_MIN_INTERVAL_MS` | 800 | 最小按键间隔 (ms) | 200-5000 |
| `DEFAULT_MAX_INTERVAL_MS` | 4000 | 最大按键间隔 (ms) | 500-10000 |
| `DEFAULT_MIN_HOLD_MS` | 80 | 最小按键持续时间 (ms) | 20-1000 |
| `DEFAULT_MAX_HOLD_MS` | 600 | 最大按键持续时间 (ms) | 50-3000 |

### 权重参数

| 宏定义 | 默认值 | 对应按键 | 说明 |
|--------|--------|----------|------|
| `DEFAULT_WEIGHT_FORWARD` | 0.30 | W | 前进 |
| `DEFAULT_WEIGHT_BACK` | 0.08 | S | 后退 |
| `DEFAULT_WEIGHT_LEFT` | 0.12 | A | 左移 |
| `DEFAULT_WEIGHT_RIGHT` | 0.12 | D | 右移 |
| `DEFAULT_WEIGHT_TURN_LEFT` | 0.10 | ← | 左转 |
| `DEFAULT_WEIGHT_TURN_RIGHT` | 0.10 | → | 右转 |
| `DEFAULT_WEIGHT_JUMP` | 0.08 | Space | 跳跃 |
| `DEFAULT_WEIGHT_C` | 0.05 | C | 蹲下 |
| `DEFAULT_WEIGHT_Z` | 0.05 | Z | 趴下 |
| `DEFAULT_WEIGHT_IDLE` | 0.08 | - | 空闲 |
| **总和** | **0.98** | | |

权重不需要总和为 1.0，后端会自动归一化。但建议总和保持在 1.0 左右以确保行为可预测。

## HID 键码表

### 字母键 (A-Z)

| 宏 | 值 | 键 |
|-----|-----|-----|
| `HID_KEY_A` ~ `HID_KEY_Z` | 0x04 ~ 0x1D | A-Z |

### 数字键 (0-9)

| 宏 | 值 | 键 |
|-----|-----|-----|
| `HID_KEY_1` ~ `HID_KEY_0` | 0x1E ~ 0x27 | 1-0 |

### 功能键

| 宏 | 值 | 键 |
|-----|-----|-----|
| `HID_KEY_ENTER` | 0x28 | Enter |
| `HID_KEY_ESC` | 0x29 | Escape |
| `HID_KEY_BACKSPACE` | 0x2A | Backspace |
| `HID_KEY_TAB` | 0x2B | Tab |
| `HID_KEY_SPACE` | 0x2C | Space |
| `HID_KEY_MINUS` | 0x2D | - _ |
| `HID_KEY_EQUAL` | 0x2E | = + |
| `HID_KEY_LBRACKET` | 0x2F | [ { |
| `HID_KEY_RBRACKET` | 0x30 | ] } |
| `HID_KEY_BACKSLASH` | 0x31 | \ | |
| `HID_KEY_SEMICOLON` | 0x33 | ; : |
| `HID_KEY_APOSTROPHE` | 0x34 | ' " |
| `HID_KEY_GRAVE` | 0x35 | ` ~ |
| `HID_KEY_COMMA` | 0x36 | , < |
| `HID_KEY_PERIOD` | 0x37 | . > |
| `HID_KEY_SLASH` | 0x38 | / ? |
| `HID_KEY_CAPSLOCK` | 0x39 | Caps Lock |

### F 功能键 (F1-F12)

| 宏 | 值 |
|-----|-----|
| `HID_KEY_F1` ~ `HID_KEY_F12` | 0x3A ~ 0x45 |

### 导航键

| 宏 | 值 | 键 |
|-----|-----|-----|
| `HID_KEY_INSERT` | 0x49 | Insert |
| `HID_KEY_HOME` | 0x4A | Home |
| `HID_KEY_PAGEUP` | 0x4B | Page Up |
| `HID_KEY_DELETE` | 0x4C | Delete |
| `HID_KEY_END` | 0x4D | End |
| `HID_KEY_PAGEDOWN` | 0x4E | Page Down |
| `HID_KEY_RIGHT_ARROW` | 0x4F | → |
| `HID_KEY_LEFT_ARROW` | 0x50 | ← |
| `HID_KEY_DOWN_ARROW` | 0x51 | ↓ |
| `HID_KEY_UP_ARROW` | 0x52 | ↑ |
| `HID_KEY_NUMLOCK` | 0x53 | Num Lock |

### 小键盘键

| 宏 | 值 | 键 |
|-----|-----|-----|
| `HID_KEY_NUMPAD_DIV` | 0x54 | / |
| `HID_KEY_NUMPAD_MUL` | 0x55 | * |
| `HID_KEY_NUMPAD_SUB` | 0x56 | - |
| `HID_KEY_NUMPAD_ADD` | 0x57 | + |
| `HID_KEY_NUMPAD_ENTER` | 0x58 | Enter |
| `HID_KEY_NUMPAD_1` ~ `HID_KEY_NUMPAD_0` | 0x59 ~ 0x62 | 1-0 |
| `HID_KEY_NUMPAD_DOT` | 0x63 | . |

### 修饰键

| 宏 | 值 | 位位置 |
|-----|-----|---------|
| `HID_MOD_LEFT_CTRL` | 0x01 | 位 0 |
| `HID_MOD_LEFT_SHIFT` | 0x02 | 位 1 |
| `HID_MOD_LEFT_ALT` | 0x04 | 位 2 |
| `HID_MOD_LEFT_GUI` | 0x08 | 位 3 |
| `HID_MOD_RIGHT_CTRL` | 0x10 | 位 4 |
| `HID_MOD_RIGHT_SHIFT` | 0x20 | 位 5 |
| `HID_MOD_RIGHT_ALT` | 0x40 | 位 6 |
| `HID_MOD_RIGHT_GUI` | 0x80 | 位 7 |

---

# Configuration Parameters

## Overview

`config.h` contains all configurable parameters for the project, including WiFi, BLE, HID keycodes, LED, NTP, and auto mode defaults.

## WiFi Configuration

```cpp
#define WIFI_SSID     "Debug"
#define WIFI_PASSWORD "12345678"
```

**Note**:
- ESP32-C3 only supports 2.4GHz band, not 5GHz
- SSID and password are case-sensitive
- On connection failure, the system will timeout after 20 seconds but still serve the Web panel (though not accessible externally)

## BLE Configuration

```cpp
#define BLE_DEVICE_NAME "ESP Virtual Keyboard"
```

## Web Server Configuration

```cpp
#define WEB_SERVER_PORT 80
```

## Serial Configuration

```cpp
#define SERIAL_BAUD_RATE 115200
```

## LED Configuration

| Macro | Value | Description |
|-------|-------|-------------|
| `LED_D4` | 12 | GPIO12 - Key flash indicator |
| `LED_D5` | 13 | GPIO13 - BLE status indicator |
| `LED_BLINK_FAST_MS` | 200 | Startup initialization fast blink |
| `LED_BLINK_SLOW_MS` | 1000 | Auto mode running slow blink |
| `LED_BLINK_ADVERT_MS` | 500 | BLE advertising blink |
| `LED_KEY_FLASH_MS` | 50 | D4 flash duration on key press |

### LED Status Indicators

| State | D5 Status LED | D4 Key LED |
|-------|---------------|------------|
| BLE not initialized | Off | - |
| BLE advertising | 500ms blink | - |
| BLE connected (manual) | Solid on | - |
| BLE connected (auto mode) | 1000ms slow blink | - |
| Key pressed | - | On for 50ms |

## NTP Configuration

```cpp
#define NTP_SERVER "ntp.aliyun.com"
#define GMT_OFFSET_SEC 28800    // UTC+8 (China)
#define DAYLIGHT_OFFSET_SEC 0
```

## Auto Mode Default Configuration

### Timing Parameters

| Macro | Default | Description | Recommended Range |
|-------|---------|-------------|-------------------|
| `DEFAULT_MIN_INTERVAL_MS` | 800 | Min key interval (ms) | 200-5000 |
| `DEFAULT_MAX_INTERVAL_MS` | 4000 | Max key interval (ms) | 500-10000 |
| `DEFAULT_MIN_HOLD_MS` | 80 | Min key hold time (ms) | 20-1000 |
| `DEFAULT_MAX_HOLD_MS` | 600 | Max key hold time (ms) | 50-3000 |

### Weight Parameters

| Macro | Default | Key | Description |
|-------|---------|-----|-------------|
| `DEFAULT_WEIGHT_FORWARD` | 0.30 | W | Forward |
| `DEFAULT_WEIGHT_BACK` | 0.08 | S | Backward |
| `DEFAULT_WEIGHT_LEFT` | 0.12 | A | Strafe left |
| `DEFAULT_WEIGHT_RIGHT` | 0.12 | D | Strafe right |
| `DEFAULT_WEIGHT_TURN_LEFT` | 0.10 | ← | Turn left |
| `DEFAULT_WEIGHT_TURN_RIGHT` | 0.10 | → | Turn right |
| `DEFAULT_WEIGHT_JUMP` | 0.08 | Space | Jump |
| `DEFAULT_WEIGHT_C` | 0.05 | C | Crouch |
| `DEFAULT_WEIGHT_Z` | 0.05 | Z | Prone |
| `DEFAULT_WEIGHT_IDLE` | 0.08 | - | Idle |
| **Total** | **0.98** | | |

## HID Keycode Table

### Letter Keys (A-Z)

| Macro | Value | Key |
|-------|-------|-----|
| `HID_KEY_A` ~ `HID_KEY_Z` | 0x04 ~ 0x1D | A-Z |

### Number Keys (0-9)

| Macro | Value | Key |
|-------|-------|-----|
| `HID_KEY_1` ~ `HID_KEY_0` | 0x1E ~ 0x27 | 1-0 |

### Function Keys

| Macro | Value | Key |
|-------|-------|-----|
| `HID_KEY_ENTER` | 0x28 | Enter |
| `HID_KEY_ESC` | 0x29 | Escape |
| `HID_KEY_BACKSPACE` | 0x2A | Backspace |
| `HID_KEY_TAB` | 0x2B | Tab |
| `HID_KEY_SPACE` | 0x2C | Space |
| `HID_KEY_MINUS` | 0x2D | - _ |
| `HID_KEY_EQUAL` | 0x2E | = + |
| `HID_KEY_LBRACKET` | 0x2F | [ { |
| `HID_KEY_RBRACKET` | 0x30 | ] } |
| `HID_KEY_BACKSLASH` | 0x31 | \ | |
| `HID_KEY_SEMICOLON` | 0x33 | ; : |
| `HID_KEY_APOSTROPHE` | 0x34 | ' " |
| `HID_KEY_GRAVE` | 0x35 | ` ~ |
| `HID_KEY_COMMA` | 0x36 | , < |
| `HID_KEY_PERIOD` | 0x37 | . > |
| `HID_KEY_SLASH` | 0x38 | / ? |
| `HID_KEY_CAPSLOCK` | 0x39 | Caps Lock |

### F Keys (F1-F12)

| Macro | Value |
|-------|-------|
| `HID_KEY_F1` ~ `HID_KEY_F12` | 0x3A ~ 0x45 |

### Navigation Keys

| Macro | Value | Key |
|-------|-------|-----|
| `HID_KEY_INSERT` | 0x49 | Insert |
| `HID_KEY_HOME` | 0x4A | Home |
| `HID_KEY_PAGEUP` | 0x4B | Page Up |
| `HID_KEY_DELETE` | 0x4C | Delete |
| `HID_KEY_END` | 0x4D | End |
| `HID_KEY_PAGEDOWN` | 0x4E | Page Down |
| `HID_KEY_RIGHT_ARROW` | 0x4F | → |
| `HID_KEY_LEFT_ARROW` | 0x50 | ← |
| `HID_KEY_DOWN_ARROW` | 0x51 | ↓ |
| `HID_KEY_UP_ARROW` | 0x52 | ↑ |
| `HID_KEY_NUMLOCK` | 0x53 | Num Lock |

### Numpad Keys

| Macro | Value | Key |
|-------|-------|-----|
| `HID_KEY_NUMPAD_DIV` | 0x54 | / |
| `HID_KEY_NUMPAD_MUL` | 0x55 | * |
| `HID_KEY_NUMPAD_SUB` | 0x56 | - |
| `HID_KEY_NUMPAD_ADD` | 0x57 | + |
| `HID_KEY_NUMPAD_ENTER` | 0x58 | Enter |
| `HID_KEY_NUMPAD_1` ~ `HID_KEY_NUMPAD_0` | 0x59 ~ 0x62 | 1-0 |
| `HID_KEY_NUMPAD_DOT` | 0x63 | . |

### Modifier Keys

| Macro | Value | Bit Position |
|-------|-------|-------------|
| `HID_MOD_LEFT_CTRL` | 0x01 | Bit 0 |
| `HID_MOD_LEFT_SHIFT` | 0x02 | Bit 1 |
| `HID_MOD_LEFT_ALT` | 0x04 | Bit 2 |
| `HID_MOD_LEFT_GUI` | 0x08 | Bit 3 |
| `HID_MOD_RIGHT_CTRL` | 0x10 | Bit 4 |
| `HID_MOD_RIGHT_SHIFT` | 0x20 | Bit 5 |
| `HID_MOD_RIGHT_ALT` | 0x40 | Bit 6 |
| `HID_MOD_RIGHT_GUI` | 0x80 | Bit 7 |