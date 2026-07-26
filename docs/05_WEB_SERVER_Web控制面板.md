# Web 控制面板

## 概述

`web_server.h/cpp` 实现了完整的 Web 控制面板，包含 HTTP API 后端和单页前端应用（HTML/CSS/JS 内嵌在 C++ 字符串中）。前端提供虚拟键盘、自动模式配置、配置管理、按键日志和统计图表等功能。

## 前端架构

### 键盘布局

控制面板包含完整的键盘布局，分为以下几个区域：

**1. 功能键行 (F 行)**
```
Esc | F1 F2 F3 F4 | F5 F6 F7 F8 | F9 F10 F11 F12
```

**2. 数字行**
```
`~ 1! 2@ 3# 4$ 5% 6^ 7& 8* 9( 0) -_ =+ ←Bksp
```

**3. QWERTY 行**
```
Tab | Q W E R T Y U I O P [{ ]} \|
```

**4. ASDF 行**
```
Caps | A S D F G H J K L ;: '" Enter
```

**5. ZXCV 行**
```
Shift | Z X C V B N M ,< .> /? Shift
```

**6. 底部修饰键行**
```
Ctrl | Win | Alt | Space | Alt | Win | Menu | Ctrl
```

**7. 编辑键区（右侧）**
```
Ins Home PgUp
Del End  PgDn
    ▲
◀  ▼  ▶
```

**8. 数字小键盘**
```
Num  ÷  ×  −
  7  8  9  +
  4  5  6
  1  2  3  ⏎
  0     .
```

### 事件处理

键盘按键支持鼠标和触摸事件：

```javascript
// 鼠标事件
k.addEventListener('mousedown', function(e) {
    k.classList.add('on');
    fetch('/api/press?key=' + key);
});
k.addEventListener('mouseup', function(e) {
    k.classList.remove('on');
    fetch('/api/release?key=' + key);
});
k.addEventListener('mouseleave', function(e) {
    k.classList.remove('on');
    fetch('/api/release?key=' + key);
});

// 触摸事件（移动端）
k.addEventListener('touchstart', function(e) {
    e.preventDefault();
    k.classList.add('on');
    fetch('/api/press?key=' + key);
}, {passive: false});
k.addEventListener('touchend', function(e) {
    e.preventDefault();
    k.classList.remove('on');
    fetch('/api/release?key=' + key);
}, {passive: false});
```

## 状态轮询

前端使用 `setInterval` 轮询后端状态：

| 轮询 | 间隔 | 端点 | 用途 |
|------|------|------|------|
| `updStatus()` | 1 秒 | `/api/status` | BLE 状态、自动模式状态、运行时间、时钟 |
| `pollEvents()` | 0.5 秒 | `/api/events` | 按键事件日志增量拉取 |
| `updStats()` | 2 秒 | `/api/stats` | 按键统计更新 |

## 自动模式配置面板

### 滑块控件

- **间隔范围**：两个滑块分别设置最小/最大按键间隔 (200-10000ms)
- **持续时间范围**：两个滑块分别设置最小/最大按键持续时间 (20-3000ms)
- **权重滑块**：10 个滑块分别设置每种按键的权重 (0-100)，实时显示总和

### 配置操作

- **应用**：将当前面板设置发送到后端
- **保存到配置**：弹窗选择目标槽位保存
- **开启/关闭**：切换自动模式启停

## 配置管理

### 槽位列表

显示 5 个配置槽位，每个槽位显示：
- 序号 (1-5)
- 名称（已保存的槽位）或 "空槽位"
- 当前活动槽位高亮显示并有 "当前" 标记
- 操作按钮：加载、删除、导出、导入

### 导入导出

- **导出当前**：下载当前配置为 JSON 文件
- **导入当前**：从 JSON 文件导入配置
- **槽位导出**：下载指定槽位配置为 JSON 文件
- **槽位导入**：从 JSON 文件导入到指定槽位

## 按键日志

实时显示自动模式的按键记录：
- 时间戳（相对运行时间 + 系统时钟）
- 按键图标：⬇ 按下 / ⬆ 释放
- 按键名称（WASD、方向键、Space 等）
- 自动滚动到最新条目
- 清空按钮

## 按键统计

可视化按键统计图表：
- 10 种按键的计数和百分比
- 柱状图显示比例
- 总计行
- 重置按钮（设置基线，从当前计数开始重新统计）

## 国际化

支持中英文切换，使用 `localStorage` 持久化语言偏好：

```javascript
var i18n = {
    zh: { /* 中文翻译 */ },
    en: { /* English translations */ }
};
function L(k) { return i18n[lang][k] || k; }
function toggleLang() { lang = lang === 'zh' ? 'en' : 'zh'; applyLang(); }
```

## 主题切换

支持深色/浅色主题，使用 CSS 自定义属性和 `localStorage` 持久化：

```css
:root {
    --bg: #0d1117;           /* 深色主题 */
    --card: #161b22;
    --text: #c9d1d9;
    /* ... */
}
[data-theme="light"] {
    --bg: #f6f8fa;           /* 浅色主题 */
    --card: #ffffff;
    --text: #24292f;
    /* ... */
}
```

---

# Web Control Panel

## Overview

The `web_server.h/cpp` implements a complete Web control panel, including an HTTP API backend and a single-page frontend application (HTML/CSS/JS embedded in C++ strings). The frontend provides a virtual keyboard, auto mode configuration, config management, key logging, and statistics charts.

## Frontend Architecture

### Keyboard Layout

The control panel includes a complete keyboard layout divided into the following areas:

**1. Function Key Row (F Row)**
```
Esc | F1 F2 F3 F4 | F5 F6 F7 F8 | F9 F10 F11 F12
```

**2. Number Row**
```
`~ 1! 2@ 3# 4$ 5% 6^ 7& 8* 9( 0) -_ =+ ←Bksp
```

**3. QWERTY Row**
```
Tab | Q W E R T Y U I O P [{ ]} \|
```

**4. ASDF Row**
```
Caps | A S D F G H J K L ;: '" Enter
```

**5. ZXCV Row**
```
Shift | Z X C V B N M ,< .> /? Shift
```

**6. Bottom Modifier Row**
```
Ctrl | Win | Alt | Space | Alt | Win | Menu | Ctrl
```

**7. Editing Keys (Right Side)**
```
Ins Home PgUp
Del End  PgDn
    ▲
◀  ▼  ▶
```

**8. Numpad**
```
Num  ÷  ×  −
  7  8  9  +
  4  5  6
  1  2  3  ⏎
  0     .
```

### Event Handling

Keyboard keys support mouse and touch events:

```javascript
// Mouse events
k.addEventListener('mousedown', function(e) {
    k.classList.add('on');
    fetch('/api/press?key=' + key);
});
k.addEventListener('mouseup', function(e) {
    k.classList.remove('on');
    fetch('/api/release?key=' + key);
});
k.addEventListener('mouseleave', function(e) {
    k.classList.remove('on');
    fetch('/api/release?key=' + key);
});

// Touch events (mobile)
k.addEventListener('touchstart', function(e) {
    e.preventDefault();
    k.classList.add('on');
    fetch('/api/press?key=' + key);
}, {passive: false});
k.addEventListener('touchend', function(e) {
    e.preventDefault();
    k.classList.remove('on');
    fetch('/api/release?key=' + key);
}, {passive: false});
```

## Status Polling

The frontend uses `setInterval` to poll backend status:

| Poll | Interval | Endpoint | Purpose |
|------|----------|----------|---------|
| `updStatus()` | 1s | `/api/status` | BLE status, auto mode, uptime, clock |
| `pollEvents()` | 0.5s | `/api/events` | Incremental key event log fetch |
| `updStats()` | 2s | `/api/stats` | Key statistics update |

## Auto Mode Configuration Panel

### Slider Controls

- **Interval range**: Two sliders for min/max key interval (200-10000ms)
- **Hold time range**: Two sliders for min/max key hold time (20-3000ms)
- **Weight sliders**: 10 sliders for each key's weight (0-100), real-time total display

### Config Operations

- **Apply**: Send current panel settings to backend
- **Save to Slot**: Modal to select target slot for saving
- **Toggle On/Off**: Start/stop auto mode

## Configuration Management

### Slot List

Displays 5 configuration slots, each showing:
- Index (1-5)
- Name (saved slots) or "Empty slot"
- Active slot highlighted with "Active" badge
- Action buttons: Load, Delete, Export, Import

### Import/Export

- **Export Current**: Download current config as JSON file
- **Import Current**: Import config from JSON file
- **Slot Export**: Download specified slot config as JSON file
- **Slot Import**: Import JSON file into specified slot

## Key Log

Real-time display of auto mode key events:
- Timestamp (relative runtime + system clock)
- Key icon: ⬇ press / ⬆ release
- Key name (WASD, arrow keys, Space, etc.)
- Auto-scroll to latest entry
- Clear button

## Key Statistics

Visual key statistics chart:
- Count and percentage for 10 keys
- Bar chart for ratio display
- Total row
- Reset button (set baseline, restart counting from current)

## Internationalization

Supports Chinese/English switching with `localStorage` persistence:

```javascript
var i18n = {
    zh: { /* Chinese translations */ },
    en: { /* English translations */ }
};
function L(k) { return i18n[lang][k] || k; }
function toggleLang() { lang = lang === 'zh' ? 'en' : 'zh'; applyLang(); }
```

## Theme Switching

Supports dark/light themes with CSS custom properties and `localStorage` persistence:

```css
:root {
    --bg: #0d1117;           /* Dark theme */
    --card: #161b22;
    --text: #c9d1d9;
    /* ... */
}
[data-theme="light"] {
    --bg: #f6f8fa;           /* Light theme */
    --card: #ffffff;
    --text: #24292f;
    /* ... */
}