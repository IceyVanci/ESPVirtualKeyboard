# ESP Virtual Keyboard

ESP32-C3 BLE 虚拟键盘 — 基于蓝牙低功耗的硬件输入模拟器，支持 Web 控制面板、自动模式和配置管理。

[English](#english) | [中文](#中文)

---

## 中文

### 📚 文档

详细文档请参阅 [docs/INDEX.md](./docs/INDEX.md) 文档目录。

### 功能特性

- **BLE 蓝牙键盘**：通过 BLE HID 协议模拟标准键盘，支持所有常用按键
- **Web 控制面板**：手机/电脑浏览器远程控制，完整 ANSI 键盘 + 数字小键盘布局
- **自动模式**：设备具备可配置的自动按键模拟能力，支持加权随机按键分配和正态分布时间间隔
- **配置管理**：5 个持久化配置槽位（NVS），支持保存/加载/删除，JSON 导入/导出（文件名使用槽位名称）；自动模式面板支持「应用」和「保存到配置」按钮
- **权重动态限制**：实时显示权重总计，后端自动归一化兜底
- **按键日志**：实时显示自动模式按键记录
- **按键统计**：按键次数和比例的可视化图表
- **明暗主题**：一键切换，localStorage 持久化
- **LED 状态指示**：D4=按键闪烁，D5=BLE 状态
- **WiFi 断线重连**：自动检测并重连

### ⚠️ 自动模式免责声明

本项目所包含的自动按键模拟功能仅为 ESP32 设备 HID 输入能力的技术演示。在使用此功能前，您应当充分了解并理解，使用该功能可能违反相关软件/游戏的服务条款、使用协议或法律法规。启用此功能即表示您已知悉并自行承担由此产生的一切后果，项目作者不对因使用此功能而导致的任何损失或处罚承担责任。

### 声明

本项目使用合宙ESP32C3作为验证开发板，但基于其在过去做出的某些商业行为，这里并不是推荐或认可。本项目使用小米MimoV2.5pro协作生成。

### 硬件要求

- **开发板**：合宙 ESP32C3-CORE（或其他 ESP32-C3 开发板）
- **Arduino IDE**：选择 `ESP32C3 Dev Module` 开发板
- **Flash Mode**：DIO

### 快速开始

1. 克隆本项目
2. 修改 `config.h` 中的 WiFi SSID 和密码
3. Arduino IDE 打开 `ESPVirtualKeyboard.ino`
4. 选择开发板 `ESP32C3 Dev Module`
5. **设置关键选项**（详见 [Arduino 写入设置指南](ARDUINO_SETUP.md)）：
   - Partition Scheme → `Huge APP (3MB No OTA)`
   - USB CDC On Boot → `Enabled`
6. 编译并上传
7. 打开串口监视器（波特率 115200）查看 IP 地址
8. 浏览器访问 IP 地址打开控制面板
9. Windows 蓝牙设置中配对 `ESP Virtual Keyboard`

### 目录结构

```
ESPVirtualKeyboard.ino - 主程序入口
config.h               - 配置文件（WiFi、BLE、HID 键码、LED、自动模式参数）
ble_keyboard.h/cpp     - BLE HID 键盘模块
auto_mode.h/cpp        - 自动模式（加权随机、事件日志、统计计数）
web_server.h/cpp       - Web 服务器（HTML/CSS/JS 控制面板）
config_manager.h/cpp   - 配置管理（NVS 持久化、槽位管理、JSON 导入导出）
```

### 依赖库

- ESP32 Arduino Core（BLE、WiFi、WebServer、Preferences）

---

## English

### 📚 Documentation

See [docs/INDEX.md](./docs/INDEX.md) for detailed documentation.

### Features

- **BLE Keyboard**: Standard HID keyboard emulation over Bluetooth Low Energy
- **Web Control Panel**: Full ANSI keyboard + numpad layout accessible via browser
- **Auto Mode**: Configurable automatic key simulation capability with weighted random key distribution and normal distribution timing
- **Config Management**: 5 persistent config slots (NVS) with save/load/overwrite/delete and JSON import/export
- **Weight Limiting**: Real-time total display with backend normalization
- **Key Logging**: Live key press log for auto mode
- **Key Statistics**: Visual chart with per-key counts and percentages
- **Theme Toggle**: Dark/Light mode with localStorage persistence
- **LED Indicators**: D4=key flash, D5=BLE status
- **WiFi Reconnect**: Automatic detection and reconnection

### ⚠️ Auto Mode Disclaimer

The auto key simulation feature is provided solely as a technical demonstration of ESP32 HID input capabilities. Before using this feature, you should fully understand that it may violate the terms of service, usage agreements, or applicable laws of relevant software/games. By enabling this feature, you acknowledge that you are aware of and solely responsible for any consequences arising from its use. The project author assumes no liability for any losses or penalties resulting from the use of this feature.

### Disclaimer

This project uses the Luat ESP32C3 as a validation board, but based on some of its past commercial practices, this is not an endorsement or recommendation. This project was developed in collaboration with Xiaomi MiMoV2.5pro.

### Hardware Requirements

- **Board**: ESP32-C3 (Luat ESP32C3-CORE or similar)
- **Arduino IDE**: Select `ESP32C3 Dev Module`
- **Flash Mode**: DIO

### Quick Start

1. Clone this repository
2. Edit `config.h` with your WiFi SSID and password
3. Open `ESPVirtualKeyboard.ino` in Arduino IDE
4. Select board `ESP32C3 Dev Module`
5. **Set critical options** (see [Arduino Setup Guide](ARDUINO_SETUP.md)):
   - Partition Scheme → `Huge APP (3MB No OTA)`
   - USB CDC On Boot → `Enabled`
6. Compile and upload
7. Open serial monitor (baud rate 115200) to find the IP address
8. Visit the IP in your browser for the control panel
9. Pair with `ESP Virtual Keyboard` in Windows Bluetooth settings

### Project Structure

```
ESPVirtualKeyboard.ino - Main entry point
config.h               - Configuration (WiFi, BLE, HID keycodes, LED, auto mode)
ble_keyboard.h/cpp     - BLE HID keyboard module
auto_mode.h/cpp        - Auto mode (weighted random, event log, stats)
web_server.h/cpp       - Web server (HTML/CSS/JS control panel)
config_manager.h/cpp   - Config management (NVS persistence, slot management, JSON I/O)
```

### Dependencies

- ESP32 Arduino Core (BLE, WiFi, WebServer, Preferences)

### License

MIT License - see [LICENSE](LICENSE) file.