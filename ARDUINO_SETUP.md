# Arduino IDE 写入设置指南 / Arduino IDE Upload Setup Guide

[中文](#中文) | [English](#english)

---

## 中文

### 开发板选择

Arduino IDE 菜单：**工具 → 开发板 → esp32 → AirM2M CORE ESP32C3**

如果找不到此选项，请先安装 ESP32 开发板支持包：
1. **文件 → 首选项 → 附加开发板管理器网址**，添加：
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
2. **工具 → 开发板 → 开发板管理器**，搜索 `esp32` 并安装

### 关键设置

以下是本项目必须的 Arduino IDE 设置（**工具** 菜单中配置）：

| 选项 | 设置值 | 说明 |
|------|--------|------|
| **Partition Scheme** | `Huge APP (3MB No OTA)` | 本项目代码量较大，默认分区空间不足 |
| **USB CDC On Boot** | `Enabled` | 启用 USB 串口输出，否则串口监视器无内容 |
| **Flash Mode** | `DIO` | 默认值，通常无需修改 |
| **Upload Speed** | `921600` | 默认值即可 |

### 串口监视器设置

上传完成后打开串口监视器：

1. **工具 → 串口监视器**（快捷键 `Ctrl+Shift+M`）
2. 右下角波特率选择 **115200**
3. 按一下开发板上的 **RST（复位）** 按钮
4. 应看到以下输出：
   ```
   ================================
     ESP32 BLE Keyboard Controller
   ================================
   [WiFi] 已连接! IP: 192.168.x.x
   [Web] 服务器已启动: http://192.168.x.x
   ================================
   系统就绪！
   ================================
   ```

### 常见问题排查

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| 编译报错 `Sketch too big` | Partition Scheme 空间不足 | 改为 `Huge APP (3MB No OTA)` |
| 串口监视器只有 `ESP-ROM:esp32c3...` | USB CDC On Boot 未启用 | 设置为 `Enabled` |
| 串口监视器完全无输出 | 波特率不匹配或 USB 线不支持数据 | 确认波特率 115200，换数据线 |
| 上传失败 | 开发板未进入下载模式 | 按住 BOOT → 按 RST → 松开 BOOT |

### Python 安装 ESP32 开发板支持（可选）

如果 Arduino IDE 开发板管理器下载速度慢，可以手动安装：

```bash
# 使用 pip 安装 esptool（用于固件烧录）
pip install esptool
```

---

## English

### Board Selection

Arduino IDE menu: **Tools → Board → esp32 → AirM2M CORE ESP32C3**

If this option is not found, install the ESP32 board support package first:
1. **File → Preferences → Additional Board Manager URLs**, add:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
2. **Tools → Board → Boards Manager**, search for `esp32` and install

### Critical Settings

The following Arduino IDE settings are required for this project (configure in the **Tools** menu):

| Option | Value | Description |
|--------|-------|-------------|
| **Partition Scheme** | `Huge APP (3MB No OTA)` | Default partition is too small for this project |
| **USB CDC On Boot** | `Enabled` | Enable USB serial output, otherwise serial monitor shows nothing |
| **Flash Mode** | `DIO` | Default value, usually no change needed |
| **Upload Speed** | `921600` | Default value is fine |

### Serial Monitor Setup

After uploading, open the serial monitor:

1. **Tools → Serial Monitor** (shortcut `Ctrl+Shift+M`)
2. Set baud rate to **115200** (bottom right corner)
3. Press the **RST (Reset)** button on the board
4. You should see:
   ```
   ================================
     ESP32 BLE Keyboard Controller
   ================================
   [WiFi] Connected! IP: 192.168.x.x
   [Web] Server started: http://192.168.x.x
   ================================
   System Ready!
   ================================
   ```

### Troubleshooting

| Problem | Cause | Solution |
|---------|-------|----------|
| Compile error: `Sketch too big` | Partition Scheme too small | Change to `Huge APP (3MB No OTA)` |
| Serial shows only `ESP-ROM:esp32c3...` | USB CDC On Boot not enabled | Set to `Enabled` |
| Serial monitor completely empty | Baud rate mismatch or charge-only USB cable | Confirm baud rate 115200, use a data cable |
| Upload failed | Board not in download mode | Hold BOOT → Press RST → Release BOOT |

### Python ESP32 Tools (Optional)

If the Arduino IDE Board Manager download is slow, you can install tools manually:

```bash
# Install esptool for firmware flashing
pip install esptool