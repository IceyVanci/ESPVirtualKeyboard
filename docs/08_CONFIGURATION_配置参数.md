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
- 连接采用**非阻塞状态机**：单次尝试最长 10 秒、最多 3 次，失败后每 30 秒重试；期间 Web 服务器始终可用

## BLE 配置

```cpp
#define BLE_DEVICE_NAME "ESP Virtual Keyboard"
```

**说明**：蓝牙广播和配对时显示的设备名称。
**注意**：名称过长可能导致某些蓝牙扫描器截断显示。
**运行时修改**：此宏仅作为初始默认值；实际名称可在 Web 控制面板「📡 蓝牙名」处修改并持久化到 NVS（键 `blename`，最多 24 字符），重启 BLE 生效。

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

## Web 认证（可选，默认关闭）

```cpp
//#define ENABLE_WEB_AUTH 1
#define WEB_AUTH_USERNAME "admin"
#define WEB_AUTH_PASSWORD "12345678"
#define WEB_AUTH_LOCKOUT_THRESHOLD 5
#define WEB_AUTH_LOCKOUT_MS 30000
```

**说明**：
- 取消 `ENABLE_WEB_AUTH` 的注释启用 Web 登录验证，防止局域网内任意设备注入按键
- `WEB_AUTH_USERNAME / WEB_AUTH_PASSWORD` 仅在首次使用时写入 NVS 作为初始凭据（之后以 NVS 为准）
- 密码在 NVS 中以 SHA-256 哈希存储，不存明文
- 连续登录/改密失败达到 `WEB_AUTH_LOCKOUT_THRESHOLD` 次后，锁定 `WEB_AUTH_LOCKOUT_MS` 毫秒
- 默认关闭时，认证功能与页面按钮均不编译，行为与旧版一致

## 固件信息

```cpp
#define FW_VERSION "v1.0"
#define FW_BUILD_DATE __DATE__   // 编译日期
#define FW_BUILD_TIME __TIME__   // 编译时间
```

**说明**：显示在 Web 页面底部页脚（版本 + 构建日期/时间），由编译器自动生成。

## 卡键安全超时

```cpp
#define WEB_KEY_STUCK_TIMEOUT_MS 5000
```

**说明**：Web 面板按下按键后，若超过该时间未收到释放（浏览器崩溃/断连等），自动释放所有按键。默认开启。

## 界面默认模式

```cpp
#define DEFAULT_KB_ONLY_MODE 0
```

**说明**：`1` = 开机默认纯虚拟键盘模式（只显示键盘），`0` = 开机默认高级模式（完整控制面板）。运行时切换会被记忆（localStorage），此宏仅在浏览器无记忆时决定初始模式。

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

> 应用配置时（`setConfig`）会自动钳制：interval 限制在 100~30000ms、hold 限制在 10~5000ms、权重限制在 [0,1]，且当 min > max 时自动交换。

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
- Connection uses a **non-blocking state machine**: max 10s per attempt, up to 3 attempts, retry every 30s after failure; the Web server stays available throughout

## BLE Configuration

```cpp
#define BLE_DEVICE_NAME "ESP Virtual Keyboard"
```

**Note**: This macro is only the initial default. The actual name can be changed via the "📡 BLE Name" button in the web panel and is persisted to NVS (key `blename`, max 24 chars); it takes effect after a BLE restart.

## Web Server Configuration

```cpp
#define WEB_SERVER_PORT 80
```

## Serial Configuration

```cpp
#define SERIAL_BAUD_RATE 115200
```

## Web Auth (Optional, Disabled by Default)

```cpp
//#define ENABLE_WEB_AUTH 1
#define WEB_AUTH_USERNAME "admin"
#define WEB_AUTH_PASSWORD "12345678"
#define WEB_AUTH_LOCKOUT_THRESHOLD 5
#define WEB_AUTH_LOCKOUT_MS 30000
```

**Note**:
- Uncomment `ENABLE_WEB_AUTH` to enable web login verification, preventing unauthorized keystroke injection on the LAN
- `WEB_AUTH_USERNAME / WEB_AUTH_PASSWORD` are seeded into NVS on first use as initial credentials (NVS takes precedence afterwards)
- Passwords are stored in NVS as SHA-256 hashes, never plaintext
- After `WEB_AUTH_LOCKOUT_THRESHOLD` consecutive login/change failures, the panel is locked for `WEB_AUTH_LOCKOUT_MS` milliseconds
- When disabled, auth features and page buttons are not compiled; behavior is identical to the previous version

## Firmware Info

```cpp
#define FW_VERSION "v1.0"
#define FW_BUILD_DATE __DATE__   // Build date
#define FW_BUILD_TIME __TIME__   // Build time
```

**Note**: Displayed in the footer at the bottom of the web page (version + build date/time), generated automatically by the compiler.

## Key-Stuck Timeout

```cpp
#define WEB_KEY_STUCK_TIMEOUT_MS 5000
```

**Note**: After a web panel key press, if no release is received within this time (browser crash/disconnect, etc.), all keys are auto-released. Enabled by default.

## Default UI Mode

```cpp
#define DEFAULT_KB_ONLY_MODE 0
```

**Note**: `1` = default to keyboard-only mode (only the keyboard shown), `0` = default to advanced mode (full panel). Runtime toggles are remembered (localStorage); this macro only decides the initial mode when there is no saved preference.

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

The weights don't need to sum to 1.0; the backend auto-normalizes. It is recommended to keep the total around 1.0 for predictable behavior.

> When applying config (`setConfig`), values are clamped automatically: interval to 100-30000ms, hold to 10-5000ms, weights to [0,1], and min/max are auto-swapped if min > max.

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