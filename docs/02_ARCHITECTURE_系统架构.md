# 系统架构

## 整体架构

ESP VirtualKeyboard 采用模块化设计，各模块职责清晰、耦合度低。以下是系统架构概览：

```
┌─────────────────────────────────────────────────┐
│                  主程序                          │
│            ESPVirtualKeyboard.ino               │
│          setup() / loop() 入口                   │
└──────┬──────────┬──────────┬──────────┬─────────┘
       │          │          │          │
       ▼          ▼          ▼          ▼
┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐
│ BLE 键盘  │ │ 自动模式  │ │ Web 服务器│ │ 配置管理  │
│ ble_     │ │ auto_    │ │ web_     │ │ config_  │
│ keyboard │ │ mode     │ │ server   │ │ manager  │
│ .h/.cpp  │ │ .h/.cpp  │ │ .h/.cpp  │ │ .h/.cpp  │
└──────────┘ └──────────┘ └──────────┘ └──────────┘
     │              │            │            │
     ▼              │            │            │
┌──────────┐        │            │            │
│ BLE HID  │        │            │            │
│ 协议栈    │        │            │            │
│ (ESP32)  │        │            │            │
└──────────┘        │            │            │
                    │            │            │
                    ▼            ▼            ▼
               ┌─────────────────────────────────┐
               │        硬件平台 ESP32-C3         │
               │  WiFi 2.4GHz | BLE 5.0 | GPIO   │
               └─────────────────────────────────┘
```

## 模块职责

### 主程序 (ESPVirtualKeyboard.ino)
- 系统初始化：LED、WiFi、NTP、BLE、配置管理器、Web 服务器
- 主循环调度：HTTP 请求处理、自动模式更新、LED 状态刷新、WiFi 保活
- LED 状态指示管理

### BLE 键盘模块 (ble_keyboard.h/cpp)
- 封装 ESP32 BLE HID 协议栈
- 管理键盘按键状态（按下/释放/报告）
- 管理蓝牙连接生命周期（广播/连接/断开）
- 支持运行时修改设备名称（`setDeviceName`，重启 BLE 生效）
- 卡键安全超时检测（`checkStuck`，超时自动释放所有按键）
- 提供按键事件驱动 LED 闪烁

### 自动模式模块 (auto_mode.h/cpp)
- 三状态机实现自动按键模拟
- 加权随机按键选择算法
- Box-Muller 正态分布时间间隔生成
- 按键事件环形缓冲区（20 条记录）
- 按键计数统计
- 参数钳制（interval 100~30000ms、hold 10~5000ms、权重 [0,1]，min≤max 自动交换）

### Web 服务器模块 (web_server.h/cpp)
- HTTP API 服务（20+ 个 REST 端点，含可选认证端点）
- 完整前端控制面板（HTML/CSS/JS 内嵌）
- 键盘独占模式（纯虚拟键盘视图，`DEFAULT_KB_ONLY_MODE` 可选默认）
- 移动端键盘自适应缩放（`fitKeyboard`）
- 在线修改蓝牙名称（`/api/ble/name`）
- 可选登录验证（`ENABLE_WEB_AUTH`：`/api/login` `/api/logout` `/api/auth/change`，写操作鉴权）
- 实时状态轮询、按键日志、统计图表
- 中英文国际化、明暗主题切换

### 配置管理器模块 (config_manager.h/cpp)
- NVS 持久化存储（5 个配置槽位，以 JSON 字符串存储）
- 活动槽位自动加载（旧字节格式自动迁移）
- 认证凭据持久化（`authuser` / `authhash`，SHA-256）
- 蓝牙设备名称持久化（`blename`）
- 手写 JSON 序列化/反序列化（无外部依赖）
- 配置导入/导出

## 数据流

### 手动按键流程

```
用户点击键盘 → HTTP GET /api/press?key=w
                    ↓
       WebServer 接收请求 → mapWebKeyToHid()
                    ↓
          BleKeyboard::press(HID_KEY_W)
                    ↓
         更新 _keyReport[8] 缓冲区
                    ↓
          setValue() + notify() 发送 HID 报告
                    ↓
              电脑接收按键输入
```

### 自动模式流程

```
AutoMode::update() 每 loop 调用一次
                    ↓
       状态机: AUTO_IDLE → 检查时间间隔
                    ↓
          selectRandomKey() 加权随机选择
                    ↓
        状态机: AUTO_PRESSING → press()
                    ↓
         等待 holdTime 到期 → release()
                    ↓
        状态机: AUTO_IDLE → 等待下一间隔
                    ↓
       事件写入环形缓冲区，前端轮询获取
```

### 配置保存流程

```
用户点击"保存到配置" → 前端弹窗选择槽位
                    ↓
        POST /api/slot/save?slot=0&name=配置1
                    ↓
        ConfigManager::saveSlot() → NVS 写入
                    ↓
         setActiveSlot() 更新活动槽位索引
                    ↓
              返回 JSON 响应
```

## 初始化流程

```
ESP32 上电复位
    ↓
Serial.begin(115200)  ← 串口初始化
    ↓
pinMode(LED_D4/LED_D5)  ← LED 初始化 + 开机快闪
    ↓
startWiFi()  ← 非阻塞连接 WiFi（开机等待最多 10 秒）
    ↓
configTime()  ← NTP 时间同步
    ↓
configMgr.begin()  ← 配置管理器初始化（读取 NVS）
    ↓
keyboard.setDeviceName(configMgr.getBleName())  ← 应用蓝牙名称
    ↓
keyboard.begin()  ← BLE 键盘初始化
    ↓
loadActiveConfig()  ← 加载上次保存的配置
    ↓
webCtrl.begin()  ← Web 服务器启动
    ↓
系统就绪！串口输出 IP 地址
```

## 主循环任务调度

```
loop() 每轮执行:
    ├── webCtrl.handleClient()  ← 处理 HTTP 请求（非阻塞）
    ├── autoMode.update()       ← 自动模式状态机更新
    ├── keyboard.checkStuck()   ← 卡键安全超时检查（自动释放）
    ├── updateStatusLED()       ← LED 状态指示（非阻塞）
    └── handleWiFi()            ← WiFi 状态机（连接/重连，非阻塞）
```

---

# System Architecture

## Overall Architecture

ESP VirtualKeyboard adopts a modular design with clear responsibilities and low coupling. Below is the architecture overview:

```
┌─────────────────────────────────────────────────┐
│                  Main Program                    │
│            ESPVirtualKeyboard.ino               │
│          setup() / loop() Entry                  │
└──────┬──────────┬──────────┬──────────┬─────────┘
       │          │          │          │
       ▼          ▼          ▼          ▼
┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐
│ BLE      │ │ Auto     │ │ Web      │ │ Config   │
│ Keyboard │ │ Mode     │ │ Server   │ │ Manager  │
│ ble_     │ │ auto_    │ │ web_     │ │ config_  │
│ keyboard │ │ mode     │ │ server   │ │ manager  │
│ .h/.cpp  │ │ .h/.cpp  │ │ .h/.cpp  │ │ .h/.cpp  │
└──────────┘ └──────────┘ └──────────┘ └──────────┘
     │              │            │            │
     ▼              │            │            │
┌──────────┐        │            │            │
│ BLE HID  │        │            │            │
│ Protocol │        │            │            │
│ (ESP32)  │        │            │            │
└──────────┘        │            │            │
                    │            │            │
                    ▼            ▼            ▼
               ┌─────────────────────────────────┐
               │      Hardware Platform          │
               │  ESP32-C3 WiFi/BLE 5.0/GPIO     │
               └─────────────────────────────────┘
```

## Module Responsibilities

### Main Program (ESPVirtualKeyboard.ino)
- System initialization: LED, WiFi, NTP, BLE, Config Manager, Web Server
- Main loop scheduling: HTTP handling, Auto Mode update, key-stuck check, LED status, WiFi state machine
- LED status indication management

### BLE Keyboard Module (ble_keyboard.h/cpp)
- Wraps the ESP32 BLE HID protocol stack
- Manages keyboard key states (press/release/report)
- Manages Bluetooth connection lifecycle (advertising/connected/disconnected)
- Supports runtime device rename (`setDeviceName`, takes effect after BLE restart)
- Key-stuck safety timeout (`checkStuck`, auto-releases all keys on timeout)
- Drives LED flashing on key events

### Auto Mode Module (auto_mode.h/cpp)
- Three-state machine for automatic key simulation
- Weighted random key selection algorithm
- Box-Muller normal distribution timing generation
- Key event ring buffer (20 entries)
- Key count statistics
- Parameter clamping (interval 100-30000ms, hold 10-5000ms, weights [0,1], min/max auto-swap)

### Web Server Module (web_server.h/cpp)
- HTTP API service (20+ REST endpoints, including optional auth endpoints)
- Complete frontend control panel (HTML/CSS/JS embedded)
- Keyboard-only mode (pure virtual keyboard view, `DEFAULT_KB_ONLY_MODE` selectable default)
- Mobile keyboard auto-scaling (`fitKeyboard`)
- Online BLE rename (`/api/ble/name`)
- Optional login auth (`ENABLE_WEB_AUTH`: `/api/login` `/api/logout` `/api/auth/change`, write-operation auth)
- Real-time status polling, key logging, statistics charts
- Chinese/English i18n, dark/light theme switching

### Config Manager Module (config_manager.h/cpp)
- NVS persistent storage (5 configuration slots, stored as JSON strings)
- Auto-load active slot on startup (with legacy binary format migration)
- Auth credentials persistence (`authuser` / `authhash`, SHA-256)
- BLE device name persistence (`blename`)
- Hand-written JSON serialization/deserialization (no external dependencies)
- Configuration import/export

## Data Flow

### Manual Key Press Flow

```
User clicks key → HTTP GET /api/press?key=w
                    ↓
       WebServer receives → mapWebKeyToHid()
                    ↓
          BleKeyboard::press(HID_KEY_W)
                    ↓
         Update _keyReport[8] buffer
                    ↓
          setValue() + notify() send HID report
                    ↓
            Computer receives key input
```

### Auto Mode Flow

```
AutoMode::update() called every loop
                    ↓
       State: AUTO_IDLE → check interval
                    ↓
          selectRandomKey() weighted random
                    ↓
        State: AUTO_PRESSING → press()
                    ↓
         Wait for holdTime → release()
                    ↓
         State: AUTO_IDLE → wait for next interval
                    ↓
       Event written to ring buffer, frontend polls
```

### Config Save Flow

```
User clicks "Save to Slot" → modal selects slot
                    ↓
        POST /api/slot/save?slot=0&name=Config1
                    ↓
        ConfigManager::saveSlot() → NVS write
                    ↓
         setActiveSlot() update active slot index
                    ↓
              Return JSON response
```

## Initialization Flow

```
ESP32 Power On Reset
    ↓
Serial.begin(115200)  ← Serial init
    ↓
pinMode(LED_D4/LED_D5)  ← LED init + startup blink
    ↓
startWiFi()  ← Non-blocking WiFi connect (boot wait max 10s)
    ↓
configTime()  ← NTP time sync
    ↓
configMgr.begin()  ← Config manager init (read NVS)
    ↓
keyboard.setDeviceName(configMgr.getBleName())  ← Apply BLE name
    ↓
keyboard.begin()  ← BLE keyboard init
    ↓
loadActiveConfig()  ← Load last saved config
    ↓
webCtrl.begin()  ← Web server start
    ↓
System ready! Serial output IP address
```

## Main Loop Task Scheduling

```
loop() each iteration:
    ├── webCtrl.handleClient()  ← Handle HTTP (non-blocking)
    ├── autoMode.update()       ← Auto mode state machine
    ├── keyboard.checkStuck()   ← Key-stuck timeout check (auto release)
    ├── updateStatusLED()       ← LED status (non-blocking)
    └── handleWiFi()            ← WiFi state machine (connect/reconnect, non-blocking)