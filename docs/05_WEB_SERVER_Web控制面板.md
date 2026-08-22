# Web 控制面板

## 概述

`web_server.h/cpp` 实现了完整的 Web 控制面板，包含 HTTP API 后端和单页前端应用（HTML/CSS/JS 内嵌在 C++ 字符串中）。前端提供虚拟键盘、键盘独占模式、移动端缩放、自动模式配置、配置管理、蓝牙名称修改、按键日志、统计图表以及可选的登录验证等功能。

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
    --bg: #f6f8fa;           /* Light theme */
    --card: #ffffff;
    --text: #24292f;
    /* ... */
}
```

## Keyboard-Only Mode

The "🎹 Keyboard" button in the top bar switches the page to a **pure virtual keyboard view**:

- Hides the auto mode panel, config manager, key log, and stats, leaving only the keyboard area (the top banner is unaffected)
- Clicking any key sends `/api/press` / `/api/release` for tap input
- The mode is saved in `localStorage['kbMode']` and persists across refreshes
- Without a saved value, the initial mode is decided by `DEFAULT_KB_ONLY_MODE` in `config.h` (1 = keyboard-only default, 0 = advanced mode default)
- Click again ("🧩 Full") to restore the full panel

## Mobile Auto-Scaling

The keyboard area uses `.kb-inner` to wrap the main keyboard, editing/arrow keys, and numpad. `fitKeyboard()` runs on page load, window resize, orientation change, and mode toggles:

```
scale = min(1, available width / keyboard natural width)
```

- When the natural keyboard width exceeds the container, `.kb-inner` gets `transform: scale(scale)` and the container height is compressed proportionally
- On wide desktop screens `scale = 1`, with no visual change
- On narrow phones the entire keyboard remains fully visible and tappable

## BLE Name Setting

The "📡 BLE Name" button in the top bar opens a settings modal:

- On open, `GET /api/ble/name` pre-fills the current name
- Enter a new name (1-24 chars) and click Save; `POST /api/ble/name` validates length, writes to NVS, and restarts BLE
- On success: "Saved, BLE restarted, please re-pair"

## Login Auth (Optional, ENABLE_WEB_AUTH)

Uncomment `//#define ENABLE_WEB_AUTH 1` in `config.h` to enable (disabled by default). When enabled:

- The top bar shows a 🔒/🔓 login button and a "🔑 Change Password" button
- Write endpoints (key press, auto mode config, BLE reboot, slot read/write, config import, rename) return `401` when not logged in
- Frontend `fetch` is patched globally: it auto-attaches the `token` parameter and opens the login modal on `401`
- Read-only endpoints (`/api/status`, `/api/events`, `/api/stats`, etc.) stay public so the page loads normally
- More than `WEB_AUTH_LOCKOUT_THRESHOLD` consecutive login/change failures lock the panel for `WEB_AUTH_LOCKOUT_MS`, returning `429`
- The session token lives only in device RAM (lost on reboot) and browser `localStorage` (persists across refreshes)

The login and change-password UI is only compiled and rendered when `ENABLE_WEB_AUTH` is enabled; otherwise the page is identical to the previous version.

## 键盘独占模式

顶部栏「🎹 键盘模式」按钮可将页面切换为**纯虚拟键盘视图**：

- 隐藏自动模式面板、配置管理、按键日志与统计，主界面只保留键盘区（顶部 banner 不受影响）
- 点击任意按键即通过 `/api/press` / `/api/release` 实现点按输入
- 切换状态保存在 `localStorage['kbMode']`，刷新/重开浏览器保持
- 无记忆时由 `config.h` 的 `DEFAULT_KB_ONLY_MODE` 决定初始模式（1=默认纯键盘，0=默认高级模式）
- 再次点击按钮（「🧩 全功能」）恢复完整面板

## 移动端自适应缩放

键盘区外层使用 `.kb-inner` 包裹主键盘、右侧编辑/方向键与小键盘。`fitKeyboard()` 会在页面加载、窗口缩放、横竖屏切换及模式切换后执行：

```
scale = min(1, 可用宽度 / 键盘自然宽度)
```

- 当键盘自然宽度超出容器时，对 `.kb-inner` 施加 `transform: scale(scale)` 并按比例压缩容器高度
- 桌面等宽屏下 `scale = 1`，不产生任何视觉变化
- 手机等窄屏设备可完整看到并点按整个键盘

## 蓝牙名称设置

顶部栏「📡 蓝牙名」按钮打开设置弹窗：

- 打开时通过 `GET /api/ble/name` 预填当前名称
- 输入新名称（1~24 字符）点击保存，`POST /api/ble/name` 校验长度后写入 NVS 并重启 BLE
- 成功提示"已保存，BLE 已重启，请重新配对"

## 登录验证（可选，ENABLE_WEB_AUTH）

在 `config.h` 中取消 `//#define ENABLE_WEB_AUTH 1` 的注释即可启用（默认关闭）。启用后：

- 顶部栏显示 🔒/🔓 登录按钮与「🔑 修改密码」按钮
- 未登录时写操作接口（按键、自动模式配置、BLE 重启、槽位读写、配置导入、改名）返回 `401`
- 前端 `fetch` 被统一打补丁：自动携带 `token` 参数，收到 `401` 时弹出登录框
- `/api/status`、`/api/events`、`/api/stats` 等只读端点保持公开，页面可正常加载
- 连续登录/改密失败超过 `WEB_AUTH_LOCKOUT_THRESHOLD` 次将锁定 `WEB_AUTH_LOCKOUT_MS`，期间返回 `429`
- 会话 token 仅存于设备内存（重启即失效）与浏览器 `localStorage`（刷新保持）

登录与修改密码界面为新增 UI，仅在 `ENABLE_WEB_AUTH` 启用时编译渲染；关闭时页面与旧版完全一致。

---

# Web Control Panel

## Overview

The `web_server.h/cpp` implements a complete Web control panel, including an HTTP API backend and a single-page frontend application (HTML/CSS/JS embedded in C++ strings). The frontend provides a virtual keyboard, keyboard-only mode, mobile auto-scaling, auto mode configuration, config management, BLE rename, key logging, statistics charts, and optional login auth.

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