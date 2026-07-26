# 快速上手指南

## 硬件需求

- **开发板**: ESP32-C3 开发板
- **USB 数据线**: Type-C 数据线（用于烧录和供电）
- **电脑**: Windows / macOS / Linux（用于开发烧录）
- **手机或另一台电脑**: 用于访问 Web 控制面板

## 环境搭建

### Arduino IDE

1. 下载并安装 [Arduino IDE](https://www.arduino.cc/en/software)
2. 添加 ESP32 开发板支持：
   - 文件 → 首选项 → 附加开发板管理器网址
   - 添加：`https://espressif.github.io/arduino-esp32/package_esp32_index.json`
   - 工具 → 开发板 → 开发板管理器 → 搜索 "ESP32" → 安装
3. 选择开发板：`ESP32C3 Dev Module`

### PlatformIO（可选替代方案）

1. 在 VS Code 中安装 PlatformIO 扩展
2. 打开项目文件夹，PlatformIO 会自动识别
3. 在 `platformio.ini` 中配置开发板（如果未自动配置）

## 配置 WiFi

1. 打开 `config.h` 文件
2. 修改以下两行：
   ```cpp
   #define WIFI_SSID     "你的WiFi名称"
   #define WIFI_PASSWORD "你的WiFi密码"
   ```
3. 其他参数使用默认值即可，后续可通过 Web 控制面板调整

## 编译烧录

### Arduino IDE

1. 打开 `ESPVirtualKeyboard.ino`
2. 工具 → 开发板 → `ESP32C3 Dev Module`
3. **关键设置**：
   - Partition Scheme → `Huge APP (3MB No OTA)`
   - USB CDC On Boot → `Enabled`
   - Flash Mode → `DIO`
4. 点击 → 编译并上传
5. 等待烧录完成

### 烧录后

1. 工具 → 串口监视器（波特率 115200）
2. 观察输出，等待 WiFi 连接成功
3. 记录显示的 IP 地址（例如 `192.168.1.100`）

## 首次使用

### 1. 连接蓝牙

1. 打开电脑的蓝牙设置
2. 搜索蓝牙设备
3. 找到 `ESP Virtual Keyboard` 并点击配对
4. 配对成功后，串口监视器会显示 "设备已连接"

### 2. 访问控制面板

1. 在手机或电脑浏览器中打开 `http://<IP地址>`（例如 `http://192.168.1.100`）
2. 您将看到完整的虚拟键盘控制面板

### 3. 测试键盘

1. 在电脑上打开一个文本编辑器（如记事本）
2. 在 Web 控制面板上点击任意按键
3. 文本编辑器中应出现对应的字符

## 常见问题

### WiFi 连接失败

- 检查 SSID 和密码是否正确
- 确认路由器工作在 2.4GHz 频段（ESP32-C3 不支持 5GHz）
- 检查防火墙是否阻挡了连接

### 蓝牙配对失败

- 点击控制面板上的 "配对" 按钮断开并重新广播
- 在电脑蓝牙设置中删除已配对的设备后重新搜索
- 确保 ESP32 的 BLE 广播未被其他设备占用

### 按键没有反应

- 检查 BLE 状态指示灯（D5）：常亮表示已连接，闪烁表示广播中
- 确认电脑蓝牙已连接 "ESP Virtual Keyboard"
- 尝试重新配对

### 串口监视器无输出

- 检查 USB CDC On Boot 是否设置为 Enabled
- 尝试更换 USB 数据线（部分数据线仅支持充电）
- 确认选择了正确的串口号

---

# Quick Start Guide

## Hardware Requirements

- **Board**: ESP32-C3 development board (recommended: AirM2M CORE ESP32C3 or compatible)
- **USB Cable**: Type-C data cable (for flashing and power)
- **Computer**: Windows / macOS / Linux (for development and flashing)
- **Phone or another computer**: For accessing the Web control panel

## Environment Setup

### Arduino IDE

1. Download and install [Arduino IDE](https://www.arduino.cc/en/software)
2. Add ESP32 board support:
   - File → Preferences → Additional Boards Manager URLs
   - Add: `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
   - Tools → Board → Boards Manager → Search "ESP32" → Install
3. Select board: `ESP32C3 Dev Module`

### PlatformIO (Alternative)

1. Install PlatformIO extension in VS Code
2. Open the project folder, PlatformIO will auto-detect
3. Configure the board in `platformio.ini` (if not auto-configured)

## WiFi Configuration

1. Open `config.h`
2. Modify these two lines:
   ```cpp
   #define WIFI_SSID     "Your_WiFi_Name"
   #define WIFI_PASSWORD "Your_WiFi_Password"
   ```
3. Other parameters can use default values and be adjusted later via the Web panel

## Compile & Flash

### Arduino IDE

1. Open `ESPVirtualKeyboard.ino`
2. Tools → Board → `ESP32C3 Dev Module`
3. **Critical settings**:
   - Partition Scheme → `Huge APP (3MB No OTA)`
   - USB CDC On Boot → `Enabled`
   - Flash Mode → `DIO`
4. Click → compile and upload
5. Wait for flashing to complete

### After Flashing

1. Tools → Serial Monitor (baud rate 115200)
2. Observe output, wait for WiFi connection
3. Note the displayed IP address (e.g., `192.168.1.100`)

## First Use

### 1. Connect Bluetooth

1. Open your computer's Bluetooth settings
2. Search for Bluetooth devices
3. Find `ESP Virtual Keyboard` and click Pair
4. After pairing, the serial monitor will show "设备已连接" (Device connected)

### 2. Access Control Panel

1. Open `http://<IP address>` in a browser (e.g., `http://192.168.1.100`)
2. You will see the full virtual keyboard control panel

### 3. Test Keyboard

1. Open a text editor on your computer (e.g., Notepad)
2. Click any key on the Web control panel
3. The corresponding character should appear in the text editor

## Troubleshooting

### WiFi Connection Failed

- Check SSID and password are correct
- Ensure the router operates on 2.4GHz (ESP32-C3 does not support 5GHz)
- Check if the firewall is blocking the connection

### Bluetooth Pairing Failed

- Click the "Pair" button on the control panel to disconnect and re-advertise
- Remove the paired device from computer Bluetooth settings and re-search
- Ensure the ESP32 BLE advertising is not occupied by other devices

### Keys Not Responding

- Check BLE status LED (D5): solid = connected, blinking = advertising
- Confirm computer Bluetooth is connected to "ESP Virtual Keyboard"
- Try re-pairing

### Serial Monitor No Output

- Check if USB CDC On Boot is set to Enabled
- Try a different USB cable (some cables only support charging)
- Confirm the correct COM port is selected