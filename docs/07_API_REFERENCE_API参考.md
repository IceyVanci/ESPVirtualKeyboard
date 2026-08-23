# Web API 参考

## 概述

Web 控制面板提供 20+ 个 REST API 端点（含可选认证端点），所有端点返回 JSON 格式响应。基础 URL 为 `http://<ESP32_IP>`。

> **认证**：仅在 `config.h` 中启用 `ENABLE_WEB_AUTH` 后生效。启用时，写操作端点需在请求中携带 `token` 参数（如 `/api/press?key=w&token=xxx`），否则返回 `401`；连续失败锁定返回 `429`。只读端点（`/api/status`、`/api/events`、`/api/stats`、`/api/slots`、`/api/seq/slots`、`/api/config/export`、`/api/slot/export`、`/api/config/export-all`、`/api/seq/config`（GET）、`/api/seq/slot/export`）始终公开。详见文末「认证端点」。

## 端点列表

### 1. 获取控制面板页面

```
GET /
```

返回完整的控制面板 HTML 页面（包含内嵌的 CSS 和 JavaScript）。

**响应**: `Content-Type: text/html`

---

### 2. 按下按键

```
GET /api/press?key=<key>
```

**参数**:
- `key` (必需): 按键名称，参见下方键名映射表（含 `lctrl`/`lshift`/`lalt`/`rctrl`/`rshift`/`ralt` 等修饰键，按下时写入修饰字节，可组合使用）

**响应**:
```json
{"ok": true}
```

**错误**:
```json
{"error": "no key"}
{"error": "unknown"}
```

> 注：仅在 BLE 已连接时生效；未连接时返回 `{"ok":true}` 但按键被丢弃，不会缓冲。

---

### 3. 释放按键

```
GET /api/release?key=<key>
```

**参数**:
- `key` (必需): 按键名称

**响应**:
```json
{"ok": true}
```

---

### 4. 获取/更新配置

```
GET /api/config
POST /api/config
```

#### GET 响应

```json
{
    "enabled": false,
    "minInterval": 800,
    "maxInterval": 4000,
    "minHold": 80,
    "maxHold": 600,
    "weightW": 0.30,
    "weightS": 0.08,
    "weightA": 0.12,
    "weightD": 0.12,
    "weightTL": 0.10,
    "weightTR": 0.10,
    "weightSP": 0.08,
    "weightC": 0.05,
    "weightZ": 0.05,
    "weightIdle": 0.08
}
```

#### POST 参数

`Content-Type: application/x-www-form-urlencoded`

| 参数 | 类型 | 说明 |
|------|------|------|
| `enabled` | bool | 自动模式开关 |
| `minInterval` | int | 最小按键间隔 (ms) |
| `maxInterval` | int | 最大按键间隔 (ms) |
| `minHold` | int | 最小按键持续时间 (ms) |
| `maxHold` | int | 最大按键持续时间 (ms) |
| `weightW` | float | W 前进权重 |
| `weightS` | float | S 后退权重 |
| `weightA` | float | A 左移权重 |
| `weightD` | float | D 右移权重 |
| `weightTL` | float | 左转权重 |
| `weightTR` | float | 右转权重 |
| `weightSP` | float | Space 跳跃权重 |
| `weightC` | float | C 蹲下权重 |
| `weightZ` | float | Z 趴下权重 |
| `weightIdle` | float | 空闲权重 |

所有参数均为可选，仅发送需要修改的字段即可。

---

### 5. 获取系统状态

```
GET /api/status
```

**响应**:
```json
{
    "bleState": "connected",
    "bleConnected": true,
    "autoMode": false,
    "seqPlaying": false,
    "currentKey": "",
    "ip": "192.168.1.100",
    "uptime": 3600,
    "epoch": 1700000000
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `bleState` | string | `"connected"` / `"advertising"` / `"stopped"` |
| `bleConnected` | bool | 是否已连接 BLE |
| `autoMode` | bool | 自动模式是否开启 |
| `seqPlaying` | bool | 顺序模式是否播放中 |
| `currentKey` | string | 当前按下的键名（自动模式） |
| `ip` | string | ESP32 的 IP 地址 |
| `uptime` | int | 运行时间（秒） |
| `epoch` | int | NTP 同步后的 Unix 时间戳 |

---

### 6. 获取按键事件

```
GET /api/events?since=<eventId>
```

**参数**:
- `since` (可选): 事件 ID，只返回比此 ID 新的事件

**响应**:
```json
{
    "events": [
        {
            "id": 1,
            "key": "w",
            "down": true,
            "t": 12345
        }
    ],
    "lastId": 1
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| `events[].id` | int | 事件唯一 ID |
| `events[].key` | string | 按键名称 |
| `events[].down` | bool | true=按下, false=释放 |
| `events[].t` | int | millis() 时间戳 |
| `lastId` | int | 最新事件 ID |

---

### 7. 获取按键统计

```
GET /api/stats
```

**响应**:
```json
{
    "total": 150,
    "keys": {
        "W": 45,
        "S": 12,
        "A": 18,
        "D": 18,
        "TL": 15,
        "TR": 15,
        "SP": 12,
        "C": 7,
        "Z": 7,
        "Idle": 1
    }
}
```

---

### 8. BLE 重新广播

```
POST /api/ble/reboot
```

断开当前 BLE 连接并重新开始广播，用于重新配对。

**响应**:
```json
{"ok": true, "msg": "已断开并重新广播"}
```

---

### 9. 获取配置槽位列表

```
GET /api/slots
```

**响应**:
```json
{
    "slots": [
        {"index": 0, "used": true, "name": "游戏配置", "config": {"version":1,"name":"游戏配置","enabled":true}},
        {"index": 1, "used": false, "name": "", "config": null},
        {"index": 2, "used": true, "name": "办公配置", "config": {"version":1,"name":"办公配置","enabled":false}},
        {"index": 3, "used": false, "name": "", "config": null},
        {"index": 4, "used": false, "name": "", "config": null}
    ],
    "active": 0
}
```

> 每个已使用槽位附带完整 `config`（JSON 格式同「导出当前配置」），供前端导入时与文件内容比对（相同则跳过）。

---

### 10. 保存配置到槽位

```
POST /api/slot/save
Content-Type: application/x-www-form-urlencoded

slot=0&name=配置名称
```

将当前自动模式配置保存到指定槽位，并设置为活动槽位。

**参数**:
- `slot` (必需): 槽位索引 0-4
- `name` (必需): 配置名称（最大 20 字符）

**响应**:
```json
{"ok": true}
```

---

### 11. 从槽位加载配置

```
POST /api/slot/load
Content-Type: application/x-www-form-urlencoded

slot=0
```

**参数**:
- `slot` (必需): 槽位索引 0-4

**响应**:
```json
{"ok": true, "name": "游戏配置"}
```

---

### 12. 删除槽位

```
POST /api/slot/delete
Content-Type: application/x-www-form-urlencoded

slot=0
```

**参数**:
- `slot` (必需): 槽位索引 0-4

**响应**:
```json
{"ok": true}
```

---

### 13. 导出槽位配置

```
POST /api/slot/export
Content-Type: application/x-www-form-urlencoded

slot=0
```

以 JSON 文件形式下载指定槽位的配置。

**参数**:
- `slot` (必需): 槽位索引 0-4

**响应**: `Content-Type: application/json` + `Content-Disposition: attachment`

---

### 14. 导入槽位配置

```
POST /api/slot/import
Content-Type: application/x-www-form-urlencoded

slot=0&json={...}
```

**参数**:
- `slot` (必需): 槽位索引 0-4
- `json` (必需): JSON 格式的配置数据

**响应**:
```json
{"ok": true}
```

---

### 15. 导出当前配置

```
GET /api/config/export
```

以 JSON 文件形式下载当前配置。

**响应**: `Content-Type: application/json` + `Content-Disposition: attachment`

---

### 16. 导入当前配置

```
POST /api/config/import
Content-Type: application/x-www-form-urlencoded

json={...}
```

**参数**:
- `json` (必需): JSON 格式的配置数据

**响应**:
```json
{"ok": true}
```

---

### 17. 获取/修改蓝牙名称

```
GET /api/ble/name
POST /api/ble/name
Content-Type: application/x-www-form-urlencoded

name=MyKeyboard
```

#### GET 响应

```json
{"name": "ESP Virtual Keyboard"}
```

#### POST

- **参数**: `name` (必需，1~24 字符)
- 校验长度后写入 NVS（`blename`），重启 BLE 使新名称生效（当前连接断开，需重新配对）
- **需登录**（启用 `ENABLE_WEB_AUTH` 时）

**响应**:
```json
{"ok": true, "msg": "已保存，请重新配对"}
```

**错误**:
```json
{"error": "invalid length"}
```

---

### 18. 获取/更新当前顺序配置

```
GET /api/seq/config
POST /api/seq/config
Content-Type: application/x-www-form-urlencoded

json={"version":1,"loop":false,"loopGapMs":1000,"steps":[{"k":"w","h":120,"g":300}]}
```

- **GET**：返回当前顺序配置（含步骤，供前端编辑/录制后应用）
- **POST**：应用编辑后的顺序配置（校验版本/键名/时长范围）；写操作需登录
- 若 `json` 解析失败返回 400 `{"error":"invalid seq JSON"}`

---

### 19. 顺序模式播放/停止

```
POST /api/seq/play
Content-Type: application/x-www-form-urlencoded

state=on   （或 state=off）
```

- 启动播放时自动停止自动模式（互斥）
- 需登录

**响应**:
```json
{"ok": true}
```

---

### 20. 顺序模式槽位管理

```
GET  /api/seq/slots                          // 列表（含每个槽位的 config 供导入比对）
POST /api/seq/slot/save  slot=0&name=名称     // 保存当前顺序配置到栏位（需登录）
POST /api/seq/slot/load  slot=0              // 加载栏位为当前顺序配置（需登录）
POST /api/seq/slot/delete slot=0             // 删除栏位（需登录）
POST /api/seq/slot/import slot=0&json={...}  // 导入到栏位（需登录）
POST /api/seq/slot/export slot=0             // 导出栏位 JSON 文件
```

与自动模式槽位接口平行，栏位索引 0-4。

---

### 21. 全部导出

```
GET /api/config/export-all
```

下载单个 JSON 文件，包含 5 个自动模式栏位 + 5 个顺序模式栏位全部预设（只读，公开）。

---

## 认证端点（仅 ENABLE_WEB_AUTH 启用时注册）

### A. 登录

```
POST /api/login
Content-Type: application/x-www-form-urlencoded

user=admin&pass=12345678
```

**成功响应**（返回会话 token）:
```json
{"ok": true, "token": "9f2c..."}
```

**失败响应**:
```json
{"error": "invalid"}
```

**锁定响应**（连续失败超过 `WEB_AUTH_LOCKOUT_THRESHOLD`）:
```json
{"error": "locked", "retry": 25}
```

### B. 登出

```
POST /api/logout
```

需携带有效 `token`。使当前会话 token 失效。

**响应**:
```json
{"ok": true}
```

### C. 修改凭据

```
POST /api/auth/change
Content-Type: application/x-www-form-urlencoded

oldUser=admin&oldPass=12345678&newUser=admin&newPass=newpass
```

校验旧凭据（与登录相同的哈希比对），通过后写入新凭据并使当前会话失效。不依赖已登录状态。

**响应**:
```json
{"ok": true}
```

**错误**:
```json
{"error": "invalid"}
{"error": "invalid length"}
{"error": "locked", "retry": 25}
```

---

## 键名映射表

以下按键名称可用于 `/api/press` 和 `/api/release` 的 `key` 参数：

| 键名 | 说明 | 键名 | 说明 |
|------|------|------|------|
| `a`-`z` | 字母键 | `0`-`9` | 数字键 |
| `enter` | 回车 | `esc` | 退出 |
| `backspace` | 退格 | `tab` | 制表符 |
| `space` | 空格 | `minus` | 减号 -_ |
| `equal` | 等号 =+ | `lbracket` | 左括号 [{ |
| `rbracket` | 右括号 ]} | `backslash` | 反斜杠 \| |
| `semicolon` | 分号 ;: | `apostrophe` | 单引号 '" |
| `grave` | 反引号 `~ | `comma` | 逗号 ,< |
| `period` | 句号 .> | `slash` | 斜杠 /? |
| `capslock` | 大写锁定 | `f1`-`f12` | 功能键 |
| `up` | 上箭头 | `down` | 下箭头 |
| `left` | 左箭头 | `right` | 右箭头 |
| `insert` | 插入 | `home` | 起始 |
| `pageup` | 上页 | `delete` | 删除 |
| `end` | 结尾 | `pagedown` | 下页 |
| `numpad0`-`numpad9` | 小键盘数字 | `numlock` | 数字锁定 |
| `numpadadd` | 小键盘 + | `numpadsub` | 小键盘 - |
| `numpadmul` | 小键盘 × | `numpaddiv` | 小键盘 ÷ |
| `numpaddot` | 小键盘 . | `numpadenter` | 小键盘 Enter |
| `lctrl` | 左 Ctrl | `rctrl` | 右 Ctrl |
| `lshift` | 左 Shift | `rshift` | 右 Shift |
| `lalt` | 左 Alt | `ralt` | 右 Alt |
| `lwin`/`lmeta` | 左 Win/Cmd | `rwin`/`rmeta` | 右 Win/Cmd |

---

# Web API Reference

## Overview

The Web control panel provides 20+ REST API endpoints (including optional auth endpoints), all returning JSON responses. The base URL is `http://<ESP32_IP>`.

> **Auth**: Only active when `ENABLE_WEB_AUTH` is enabled in `config.h`. When enabled, write endpoints require a `token` parameter (e.g. `/api/press?key=w&token=xxx`), otherwise `401` is returned; lockout returns `429`. Read-only endpoints (`/api/status`, `/api/events`, `/api/stats`, `/api/slots`, `/api/seq/slots`, `/api/config/export`, `/api/slot/export`, `/api/config/export-all`, `/api/seq/config` (GET), `/api/seq/slot/export`) are always public. See "Auth Endpoints" at the end.

## Endpoint List

### 1. Get Control Panel Page

```
GET /
```

Returns the complete control panel HTML page (with embedded CSS and JavaScript).

**Response**: `Content-Type: text/html`

---

### 2. Press Key

```
GET /api/press?key=<key>
```

**Parameters**:
- `key` (required): Key name, see key mapping table below (modifier keys `lctrl`/`lshift`/`lalt`/`rctrl`/`rshift`/`ralt` are supported and can be combined)

**Response**:
```json
{"ok": true}
```

**Errors**:
```json
{"error": "no key"}
{"error": "unknown"}
```

> Note: Only effective while BLE is connected; while disconnected the request returns `{"ok":true}` but the key is dropped (not buffered).

---

### 3. Release Key

```
GET /api/release?key=<key>
```

**Parameters**:
- `key` (required): Key name

**Response**:
```json
{"ok": true}
```

---

### 4. Get/Update Config

```
GET /api/config
POST /api/config
```

#### GET Response

```json
{
    "enabled": false,
    "minInterval": 800,
    "maxInterval": 4000,
    "minHold": 80,
    "maxHold": 600,
    "weightW": 0.30,
    "weightS": 0.08,
    "weightA": 0.12,
    "weightD": 0.12,
    "weightTL": 0.10,
    "weightTR": 0.10,
    "weightSP": 0.08,
    "weightC": 0.05,
    "weightZ": 0.05,
    "weightIdle": 0.08
}
```

#### POST Parameters

`Content-Type: application/x-www-form-urlencoded`

| Parameter | Type | Description |
|-----------|------|-------------|
| `enabled` | bool | Auto mode toggle |
| `minInterval` | int | Min key interval (ms) |
| `maxInterval` | int | Max key interval (ms) |
| `minHold` | int | Min key hold time (ms) |
| `maxHold` | int | Max key hold time (ms) |
| `weightW` | float | W forward weight |
| `weightS` | float | S back weight |
| `weightA` | float | A left weight |
| `weightD` | float | D right weight |
| `weightTL` | float | Turn left weight |
| `weightTR` | float | Turn right weight |
| `weightSP` | float | Space jump weight |
| `weightC` | float | C crouch weight |
| `weightZ` | float | Z prone weight |
| `weightIdle` | float | Idle weight |

All parameters are optional; only send fields you want to change.

---

### 5. Get System Status

```
GET /api/status
```

**Response**:
```json
{
    "bleState": "connected",
    "bleConnected": true,
    "autoMode": false,
    "seqPlaying": false,
    "currentKey": "",
    "ip": "192.168.1.100",
    "uptime": 3600,
    "epoch": 1700000000
}
```

| Field | Type | Description |
|-------|------|-------------|
| `bleState` | string | `"connected"` / `"advertising"` / `"stopped"` |
| `bleConnected` | bool | BLE connection status |
| `autoMode` | bool | Auto mode status |
| `seqPlaying` | bool | Whether sequence mode is playing |
| `currentKey` | string | Currently pressed key (auto mode) |
| `ip` | string | ESP32 IP address |
| `uptime` | int | Uptime (seconds) |
| `epoch` | int | NTP-synced Unix timestamp |

---

### 6. Get Key Events

```
GET /api/events?since=<eventId>
```

**Parameters**:
- `since` (optional): Event ID, only returns events newer than this

**Response**:
```json
{
    "events": [
        {
            "id": 1,
            "key": "w",
            "down": true,
            "t": 12345
        }
    ],
    "lastId": 1
}
```

---

### 7. Get Key Statistics

```
GET /api/stats
```

**Response**:
```json
{
    "total": 150,
    "keys": {
        "W": 45,
        "S": 12,
        "A": 18,
        "D": 18,
        "TL": 15,
        "TR": 15,
        "SP": 12,
        "C": 7,
        "Z": 7,
        "Idle": 1
    }
}
```

---

### 8. BLE Re-advertise

```
POST /api/ble/reboot
```

Disconnect current BLE connection and restart advertising for re-pairing.

**Response**:
```json
{"ok": true, "msg": "已断开并重新广播"}
```

---

### 9. Get Slot List

```
GET /api/slots
```

**Response**:
```json
{
    "slots": [
        {"index": 0, "used": true, "name": "Game Config", "config": {"version":1,"name":"Game Config","enabled":true}},
        {"index": 1, "used": false, "name": "", "config": null},
        {"index": 2, "used": true, "name": "Office Config", "config": {"version":1,"name":"Office Config","enabled":false}},
        {"index": 3, "used": false, "name": "", "config": null},
        {"index": 4, "used": false, "name": "", "config": null}
    ],
    "active": 0
}
```

> Each used slot includes its full `config` (same JSON format as "Export Current Config") for the frontend to compare against imported file contents (identical ones are skipped).

---

### 10. Save Config to Slot

```
POST /api/slot/save
Content-Type: application/x-www-form-urlencoded

slot=0&name=ConfigName
```

**Parameters**:
- `slot` (required): Slot index 0-4
- `name` (required): Config name (max 20 chars)

**Response**:
```json
{"ok": true}
```

---

### 11. Load Config from Slot

```
POST /api/slot/load
Content-Type: application/x-www-form-urlencoded

slot=0
```

**Parameters**:
- `slot` (required): Slot index 0-4

**Response**:
```json
{"ok": true, "name": "Game Config"}
```

---

### 12. Delete Slot

```
POST /api/slot/delete
Content-Type: application/x-www-form-urlencoded

slot=0
```

**Parameters**:
- `slot` (required): Slot index 0-4

**Response**:
```json
{"ok": true}
```

---

### 13. Export Slot Config

```
POST /api/slot/export
Content-Type: application/x-www-form-urlencoded

slot=0
```

**Parameters**:
- `slot` (required): Slot index 0-4

**Response**: `Content-Type: application/json` + `Content-Disposition: attachment`

---

### 14. Import Slot Config

```
POST /api/slot/import
Content-Type: application/x-www-form-urlencoded

slot=0&json={...}
```

**Parameters**:
- `slot` (required): Slot index 0-4
- `json` (required): JSON config data

**Response**:
```json
{"ok": true}
```

---

### 15. Export Current Config

```
GET /api/config/export
```

**Response**: `Content-Type: application/json` + `Content-Disposition: attachment`

---

### 16. Import Current Config

```
POST /api/config/import
Content-Type: application/x-www-form-urlencoded

json={...}
```

**Parameters**:
- `json` (required): JSON config data

**Response**:
```json
{"ok": true}
```

---

### 17. Get/Change BLE Name

```
GET /api/ble/name
POST /api/ble/name
Content-Type: application/x-www-form-urlencoded

name=MyKeyboard
```

#### GET Response

```json
{"name": "ESP Virtual Keyboard"}
```

#### POST

- **Parameters**: `name` (required, 1-24 chars)
- After length validation, writes to NVS (`blename`) and restarts BLE so the new name takes effect (current connection drops; re-pair required)
- **Auth required** (when `ENABLE_WEB_AUTH` enabled)

**Response**:
```json
{"ok": true, "msg": "已保存，请重新配对"}
```

**Errors**:
```json
{"error": "invalid length"}
```

---

### 18. Get/Update Current Sequence Config

```
GET /api/seq/config
POST /api/seq/config
Content-Type: application/x-www-form-urlencoded

json={"version":1,"loop":false,"loopGapMs":1000,"steps":[{"k":"w","h":120,"g":300}]}
```

- **GET**: Returns the current sequence config (with steps, for editing/apply after recording)
- **POST**: Applies the edited sequence config (validates version/key names/duration ranges); auth required
- Returns 400 `{"error":"invalid seq JSON"}` on parse failure

---

### 19. Sequence Playback / Stop

```
POST /api/seq/play
Content-Type: application/x-www-form-urlencoded

state=on   (or state=off)
```

- Starting playback automatically stops auto mode (mutual exclusion)
- Auth required

**Response**:
```json
{"ok": true}
```

---

### 20. Sequence Slot Management

```
GET  /api/seq/slots                          // List (each slot includes config for import comparison)
POST /api/seq/slot/save  slot=0&name=Name     // Save current sequence to slot (auth)
POST /api/seq/slot/load  slot=0              // Load slot as current sequence (auth)
POST /api/seq/slot/delete slot=0             // Delete slot (auth)
POST /api/seq/slot/import slot=0&json={...}  // Import into slot (auth)
POST /api/seq/slot/export slot=0             // Export slot JSON file
```

Parallel to the auto mode slot endpoints; slot index 0-4.

---

### 21. Export All

```
GET /api/config/export-all
```

Downloads a single JSON file containing all presets from 5 auto slots + 5 sequence slots (read-only, public).

---

## Auth Endpoints (registered only when ENABLE_WEB_AUTH is enabled)

### A. Login

```
POST /api/login
Content-Type: application/x-www-form-urlencoded

user=admin&pass=12345678
```

**Success response** (returns session token):
```json
{"ok": true, "token": "9f2c..."}
```

**Failure response**:
```json
{"error": "invalid"}
```

**Locked response** (after more than `WEB_AUTH_LOCKOUT_THRESHOLD` consecutive failures):
```json
{"error": "locked", "retry": 25}
```

### B. Logout

```
POST /api/logout
```

Requires a valid `token`. Invalidates the current session token.

**Response**:
```json
{"ok": true}
```

### C. Change Credentials

```
POST /api/auth/change
Content-Type: application/x-www-form-urlencoded

oldUser=admin&oldPass=12345678&newUser=admin&newPass=newpass
```

Verifies the old credentials (same hash comparison as login), then writes the new credentials and invalidates the current session. Does not require an existing login.

**Response**:
```json
{"ok": true}
```

**Errors**:
```json
{"error": "invalid"}
{"error": "invalid length"}
{"error": "locked", "retry": 25}
```

---

## Key Name Mapping

The following key names can be used for the `key` parameter in `/api/press` and `/api/release`:

| Key Name | Description | Key Name | Description |
|----------|-------------|----------|-------------|
| `a`-`z` | Letter keys | `0`-`9` | Number keys |
| `enter` | Enter | `esc` | Escape |
| `backspace` | Backspace | `tab` | Tab |
| `space` | Spacebar | `minus` | Minus -_ |
| `equal` | Equals =+ | `lbracket` | Left bracket [{ |
| `rbracket` | Right bracket ]} | `backslash` | Backslash \| |
| `semicolon` | Semicolon ;: | `apostrophe` | Apostrophe '" |
| `grave` | Backtick `~ | `comma` | Comma ,< |
| `period` | Period .> | `slash` | Slash /? |
| `capslock` | Caps Lock | `f1`-`f12` | Function keys |
| `up` | Up arrow | `down` | Down arrow |
| `left` | Left arrow | `right` | Right arrow |
| `insert` | Insert | `home` | Home |
| `pageup` | Page Up | `delete` | Delete |
| `end` | End | `pagedown` | Page Down |
| `numpad0`-`numpad9` | Numpad digits | `numlock` | Num Lock |
| `numpadadd` | Numpad + | `numpadsub` | Numpad - |
| `numpadmul` | Numpad × | `numpaddiv` | Numpad ÷ |
| `numpaddot` | Numpad . | `numpadenter` | Numpad Enter |
| `lctrl` | Left Ctrl | `rctrl` | Right Ctrl |
| `lshift` | Left Shift | `rshift` | Right Shift |
| `lalt` | Left Alt | `ralt` | Right Alt |
| `lwin`/`lmeta` | Left Win/Cmd | `rwin`/`rmeta` | Right Win/Cmd |