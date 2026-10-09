# 配置管理

## 概述

`config_manager.h/cpp` 实现了基于 ESP32 NVS（非易失性存储）的配置持久化系统。它提供 5 个自动模式槽位与 5 个顺序模式槽位，支持保存、加载、删除、导入和导出操作，以及活动槽位的自动加载。

## NVS 存储布局

使用 Preferences 库，命名空间为 `cfgmgr`：

| NVS 键 | 类型 | 说明 |
|--------|------|------|
| `s0n` ~ `s4n` | String | 槽位 0-4 的名称（最大 20 字符） |
| `s0d` ~ `s4d` | String | 槽位 0-4 的配置数据（JSON 字符串） |
| `s0u` ~ `s4u` | Bool | 槽位 0-4 的使用标记 |
| `active` | Int | 当前活动槽位索引（-1 = 默认配置） |
| `authuser` | String | Web 认证用户名（仅 `ENABLE_WEB_AUTH` 启用时写入） |
| `authhash` | String | Web 认证密码的 SHA-256 十六进制哈希（仅启用时写入） |
| `blename` | String | 蓝牙设备名称（未设置时回退 `BLE_DEVICE_NAME`） |
| `sq0n` ~ `sq4n` | String | 顺序模式槽位 0-4 的名称 |
| `sq0d` ~ `sq4d` | String | 顺序模式槽位 0-4 的数据（JSON 字符串） |
| `sq0u` ~ `sq4u` | Bool | 顺序模式槽位 0-4 的使用标记 |
| `sqactive` | Int | 顺序模式活动槽位索引（-1 = 默认） |

> 槽位配置数据使用 **JSON 字符串** 存储（`configToJson` 生成），避免结构体内存布局变更导致旧数据损坏。加载时会优先解析 JSON，若解析失败则回退读取旧版二进制格式（`sizeof(AutoModeConfig)`）并自动迁移为 JSON。

### NVS Key 生成

```cpp
String slotNameKey(int i) { return "s" + String(i) + "n"; }  // 名称
String slotDataKey(int i) { return "s" + String(i) + "d"; }  // 数据
String slotUsedKey(int i) { return "s" + String(i) + "u"; }  // 使用标记
```

## 槽位操作

### 保存槽位 (saveSlot)

```cpp
bool saveSlot(int slotIndex, const String& name, const AutoModeConfig& config);
```

1. 名称经 `sanitizeName()` 消毒并按 Unicode 码点截断到 `SLOT_NAME_MAX_LEN`（20 字符）；消毒后为空时回退为「配置N」
2. 将配置序列化为 JSON（`configToJson`）
3. 写入 NVS：名称字符串、JSON 配置数据、使用标记 true
4. 返回是否成功

### 加载槽位 (loadSlot)

```cpp
bool loadSlot(int slotIndex, AutoModeConfig& config, String& name);
```

1. 检查使用标记，未使用则返回 false
2. 读取名称字符串
3. 读取 JSON 配置数据并解析（`jsonToConfig`）
4. 若 JSON 解析失败，回退读取旧版二进制数据并自动迁移为 JSON

### 删除槽位 (deleteSlot)

```cpp
bool deleteSlot(int slotIndex);
```

1. 删除 NVS 中的名称和数据
2. 设置使用标记为 false
3. 如果删除的是当前活动槽位，重置活动槽位为 -1

### 活动槽位管理

```cpp
void setActiveSlot(int slotIndex);  // 保存活动槽位索引到 NVS
int  getActiveSlot();               // 读取活动槽位索引（默认 -1）
bool loadActiveConfig(AutoModeConfig& config);  // 加载活动槽位配置
```

启动时自动加载：

```cpp
// ESPVirtualKeyboard.ino 中
AutoModeConfig savedConfig;
if (configMgr.loadActiveConfig(savedConfig)) {
    autoMode.setConfig(savedConfig);
    Serial.print("[Config] 已加载槽位 ");
    Serial.println(configMgr.getActiveSlot());
}
```

## JSON 序列化

### 导出格式

```json
{
    "version": 1,
    "name": "配置名称",
    "enabled": true,
    "minIntervalMs": 800,
    "maxIntervalMs": 4000,
    "minHoldMs": 80,
    "maxHoldMs": 600,
    "weights": {
        "forward": 0.30,
        "back": 0.08,
        "left": 0.12,
        "right": 0.12,
        "turnLeft": 0.10,
        "turnRight": 0.10,
        "jump": 0.08,
        "c": 0.05,
        "z": 0.05,
        "idle": 0.08
    }
}
```

### JSON 序列化 / 反序列化（ArduinoJson 7）

项目使用 **ArduinoJson 7**（`JsonDocument`）进行序列化与反序列化：

```cpp
String ConfigManager::configToJson(const AutoModeConfig& c, const String& name);  // serializeJson
bool   ConfigManager::jsonToConfig(const String& json, AutoModeConfig& c, String& name);  // deserializeJson
String ConfigManager::seqConfigToJson(const SeqConfig& c, const String& name);
bool   ConfigManager::seqConfigToJsonChecked(const SeqConfig& c, const String& name, String& out);
SeqParseResult ConfigManager::seqJsonToConfig(const String& json, SeqConfig& c, String& name);
```

关键行为：

- 序列化时字符串字段（`name` / 步骤 `k`）由 ArduinoJson **自动转义**，杜绝引号/控制字符破坏 JSON 结构
- 所有名称（槽位名、序列名、BLE 名）在保存与读取时均经 **`sanitizeName()`** 消毒：保留可打印 ASCII 与合法 UTF-8（中文/emoji），剔除 `"` `\` `<` `>` 与控制字符、无效字节，防止 NVS 数据污染与 XSS；长度按 Unicode 码点截断（`truncateName()`，槽位名 20 字符 / BLE 名 24 字符，不会切断多字节字符）。空名兜底：「配置N」/「序列N」，保证栏位不出现空白名称
- 顺序步骤键名解析时自动 `toLowerCase()` 容错大写；仍非法（`webKeyToHid` 返回 `0xFF`）的键名降级为**暂停步骤**（空键名），不会截断整条序列
- 顺序配置 `version` **缺省视为 1**（兼容旧版「导出当前」文件），显式 1/2 接受，其它拒绝；输出恒为 2
- 顺序配置步数 > `SEQ_MAX_STEPS` 返回 `SEQ_PARSE_TOO_MANY`（HTTP 400）；解析后序列化体积 > `SEQ_JSON_MAX_BYTES` 返回 `SEQ_PARSE_TOO_LARGE`（HTTP 413）

### 导入验证

```cpp
bool ConfigManager::jsonToConfig(const String& json, AutoModeConfig& config, String& name) {
    // 1. deserializeJson 严格解析，失败直接返回 false（对应 HTTP 400）
    // 2. 验证 version 字段为 1
    // 3. 提取各个字段
    // 4. 范围验证：
    //    - minIntervalMs: [100, 30000]
    //    - maxIntervalMs: [100, 30000]
    //    - minHoldMs: [10, 5000]
    //    - maxHoldMs: [10, 5000]
    //    - 所有权重: [0, 1]
    // 5. 自动交换 min/max（如果 min > max）
    // 6. name 经 sanitizeName 消毒并按码点截断（空名由 saveSlot 回退为「配置N」）
}
```

## 使用示例

### API 调用链

```cpp
// 1. 获取当前配置
AutoModeConfig c = autoMode->getConfig();

// 2. 保存到槽位 0
configMgr->saveSlot(0, "游戏配置", c);
configMgr->setActiveSlot(0);

// 3. 从槽位 1 加载
AutoModeConfig loaded;
String name;
if (configMgr->loadSlot(1, loaded, name)) {
    autoMode->setConfig(loaded);
}

// 4. 导出为 JSON
String json = configMgr->exportConfig(c, "我的配置");

// 5. 从 JSON 导入
AutoModeConfig imported;
String importName;
if (configMgr->importConfig(json, imported, importName)) {
    autoMode->setConfig(imported);
}
```

## 认证凭据与蓝牙名称

### 认证凭据（仅 ENABLE_WEB_AUTH）

```cpp
String getAuthUser();                          // 用户名（NVS，缺省回退 WEB_AUTH_USERNAME）
String getAuthPass();                          // 密码 SHA-256 哈希（NVS，缺省回退默认值哈希）
bool   hasAuthCredentials();                   // NVS 是否已初始化凭据
bool   setAuthCredentials(const String& user, const String& pass);  // 写入 NVS（用户名/密码 1~20 字符）
static String sha256Hex(const String& input);  // SHA-256 十六进制（mbedtls 实现）
```

- 首次启动（`ENABLE_WEB_AUTH` 启用时）若 NVS 无凭据，用 `config.h` 的 `WEB_AUTH_USERNAME / WEB_AUTH_PASSWORD` 初始化
- 密码仅存 SHA-256 哈希，不存明文；登录/改密通过 `handleLogin` / `handleAuthChange` 比对哈希

### 蓝牙设备名称

```cpp
String getBleName();        // 读取名称（NVS，缺省回退 BLE_DEVICE_NAME）
bool   setBleName(const String& name);  // 写入 NVS（1~24 字符），需重启 BLE 生效
```

## 顺序模式槽位

顺序模式有 5 个独立栏位，与自动模式槽位互不影响：

```cpp
bool   saveSeqSlot(int slot, const String& name, const SeqConfig& config); // 保存（JSON 字符串；写失败回滚并返回 false）
bool   loadSeqSlot(int slot, SeqConfig& config, String& name);             // 加载
bool   deleteSeqSlot(int slot);                                            // 删除
bool   isSeqSlotUsed(int slot);
String getSeqSlotName(int slot);
void   setActiveSeqSlot(int slot);   // -1 = 默认
int    getActiveSeqSlot();
bool   loadActiveSeqConfig(SeqConfig& config);   // 启动时自动加载活动顺序配置
String seqConfigToJson(const SeqConfig& config, const String& name);                     // 纯序列化（读取/导出）
bool   seqConfigToJsonChecked(const SeqConfig& config, const String& name, String& out); // 序列化 + 体积校验（写入路径）
SeqParseResult seqJsonToConfig(const String& json, SeqConfig& config, String& name);
```

顺序配置 JSON 格式 v2（每步 `k`=按键名，空表示暂停步骤；`h`=时长 ms，`g`=间隔 ms；`r`=重复次数；`rk`=随机键；`hr`/`gr`=随机时序 min–max）：

```json
{
  "version": 2,
  "name": "我的序列",
  "loop": false,
  "loopGapMs": 1000,
  "steps": [
    {"k": "w", "h": 120, "g": 300},
    {"k": "space", "h": 80, "g": 200, "r": 3},
    {"k": "w", "h": 200, "g": 300, "rk": 1},
    {"k": "a", "h": 100, "g": 100, "hr": [60, 180], "gr": [100, 400]}
  ]
}
```

解析与守卫：

- `version` 缺省=1，显式 1/2 接受，其它拒绝；输出恒为 2（`r==1`、`rk==0`、未启用的 `hr/gr` 省略；`k`/`h`/`g` 恒输出，`rk=1` 时保留 `k` 原文）
- 步数 > `SEQ_MAX_STEPS`（64）→ `SEQ_PARSE_TOO_MANY`；解析后重新序列化体积 > `SEQ_JSON_MAX_BYTES`（3000B）→ `SEQ_PARSE_TOO_LARGE`
- `hr`/`gr` 需长度 2，min>max 自动交换，值钳制 10–10000ms；`r` 钳制 1–99；hold/gap 钳制 10–10000ms；loopGap 钳制 0–10000ms
- 非空键名必须可映射到有效 HID 键码，否则降级为暂停步骤

NVS 预算：分区共 20,480B；单槽上限 3000B × 5 槽 ≈ 15KB，叠加自动模式槽位与凭据约 19KB。`saveSeqSlot` 检查 `putString` 返回值，失败时删除半截数据、回滚 `sqXu=false` 并返回 false（handler 返回 500 `storage full`）。

## 全部导出

`exportAllConfigs()` 将 5 个自动模式栏位与 5 个顺序模式栏位汇总为单个 JSON 返回：

```json
{
  "app": "ESPVirtualKeyboard",
  "version": 1,
  "auto": [ {"used": true, "name": "...", "config": {...}}, ... ],
  "seq":  [ {"used": true, "name": "...", "config": {...}}, ... ]
}
```

其中每个 `config` 内部同时写入 `name`（与条目级 `name` 一致），保证「导出 → 导入」往返不丢名称（导入时前端还会把条目级 `name` 合并进 config，兼容旧文件）。

由 `GET /api/config/export-all` 下载；导入时前端解析后与设备现有栏位逐项比对（相同跳过、不同默认分配到该模式下**下一个空槽位**，可手动调整/跳过）。

---

# Configuration Management

## Overview

The `config_manager.h/cpp` implements a configuration persistence system based on ESP32 NVS (Non-Volatile Storage). It provides 5 configuration slots supporting save, load, delete, import, and export operations, along with auto-loading of the active slot.

## NVS Storage Layout

Uses the Preferences library, namespace `cfgmgr`:

| NVS Key | Type | Description |
|---------|------|-------------|
| `s0n` ~ `s4n` | String | Slot 0-4 names (max 20 chars) |
| `s0d` ~ `s4d` | String | Slot 0-4 config data (JSON string) |
| `s0u` ~ `s4u` | Bool | Slot 0-4 used flags |
| `active` | Int | Active slot index (-1 = default config) |
| `authuser` | String | Web auth username (only written when `ENABLE_WEB_AUTH` enabled) |
| `authhash` | String | SHA-256 hex of the web auth password (only when enabled) |
| `blename` | String | BLE device name (falls back to `BLE_DEVICE_NAME` when unset) |
| `sq0n` ~ `sq4n` | String | Sequence slot 0-4 names |
| `sq0d` ~ `sq4d` | String | Sequence slot 0-4 data (JSON string) |
| `sq0u` ~ `sq4u` | Bool | Sequence slot 0-4 used flags |
| `sqactive` | Int | Active sequence slot index (-1 = default) |

> Slot config data is stored as a **JSON string** (generated by `configToJson`) to avoid corruption from struct layout changes. On load, JSON is parsed first; if parsing fails, the legacy binary format (`sizeof(AutoModeConfig)`) is read and auto-migrated to JSON.

### NVS Key Generation

```cpp
String slotNameKey(int i) { return "s" + String(i) + "n"; }  // Name
String slotDataKey(int i) { return "s" + String(i) + "d"; }  // Data
String slotUsedKey(int i) { return "s" + String(i) + "u"; }  // Used flag
```

## Slot Operations

### Save Slot (saveSlot)

```cpp
bool saveSlot(int slotIndex, const String& name, const AutoModeConfig& config);
```

1. Truncate name to `SLOT_NAME_MAX_LEN` (20 chars)
2. Serialize the config to JSON (`configToJson`)
3. Write to NVS: name string, JSON config data, used flag true
4. Return success status

### Load Slot (loadSlot)

```cpp
bool loadSlot(int slotIndex, AutoModeConfig& config, String& name);
```

1. Check used flag, return false if unused
2. Read name string
3. Read and parse the JSON config data (`jsonToConfig`)
4. If JSON parsing fails, fall back to the legacy binary data and auto-migrate it to JSON

### Delete Slot (deleteSlot)

```cpp
bool deleteSlot(int slotIndex);
```

1. Delete name and data from NVS
2. Set used flag to false
3. If deleting the active slot, reset active slot to -1

### Active Slot Management

```cpp
void setActiveSlot(int slotIndex);  // Save active slot index to NVS
int  getActiveSlot();               // Read active slot index (default -1)
bool loadActiveConfig(AutoModeConfig& config);  // Load active slot config
```

Auto-load on startup:

```cpp
// In ESPVirtualKeyboard.ino
AutoModeConfig savedConfig;
if (configMgr.loadActiveConfig(savedConfig)) {
    autoMode.setConfig(savedConfig);
    Serial.print("[Config] Loaded slot ");
    Serial.println(configMgr.getActiveSlot());
}
```

## JSON Serialization

### Export Format

```json
{
    "version": 1,
    "name": "Config Name",
    "enabled": true,
    "minIntervalMs": 800,
    "maxIntervalMs": 4000,
    "minHoldMs": 80,
    "maxHoldMs": 600,
    "weights": {
        "forward": 0.30,
        "back": 0.08,
        "left": 0.12,
        "right": 0.12,
        "turnLeft": 0.10,
        "turnRight": 0.10,
        "jump": 0.08,
        "c": 0.05,
        "z": 0.05,
        "idle": 0.08
    }
}
```

### JSON Serialization / Deserialization (ArduinoJson 7)

The project uses **ArduinoJson 7** (`JsonDocument`) for serialization and deserialization:

```cpp
String ConfigManager::configToJson(const AutoModeConfig& c, const String& name);  // serializeJson
bool   ConfigManager::jsonToConfig(const String& json, AutoModeConfig& c, String& name);  // deserializeJson
String ConfigManager::seqConfigToJson(const SeqConfig& c, const String& name);
bool   ConfigManager::seqJsonToConfig(const String& json, SeqConfig& c, String& name);
```

Key behaviors:

- String fields (`name` / step `k`) are **auto-escaped** by ArduinoJson during serialization, preventing quotes/control characters from breaking the JSON structure
- All names (slot names, sequence names, BLE name) pass through **`sanitizeName()`** on save and load: printable ASCII and valid UTF-8 (Chinese/emoji) are kept, while `"` `\` `<` `>`, control chars and invalid bytes are stripped, preventing NVS pollution and XSS; length is truncated by Unicode code points (`truncateName()`, 20 chars for slot names / 24 chars for BLE name, never splitting a multi-byte char). Empty names fall back to "配置N" / "序列N" so slots never show a blank name
- Sequence step key names are lowercased on parse (`toLowerCase()`) to tolerate uppercase; keys still invalid (`webKeyToHid` returns `0xFF`) degrade to a **pause step** (empty key) instead of truncating the whole sequence

### Import Validation

```cpp
bool ConfigManager::jsonToConfig(const String& json, AutoModeConfig& config, String& name) {
    // 1. Strict deserializeJson; fail (HTTP 400) on parse errors
    // 2. Verify version field is 1
    // 3. Extract each field
    // 4. Range validation:
    //    - minIntervalMs: [100, 30000]
    //    - maxIntervalMs: [100, 30000]
    //    - minHoldMs: [10, 5000]
    //    - maxHoldMs: [10, 5000]
    //    - All weights: [0, 1]
    // 5. Auto-swap min/max (if min > max)
    // 6. name sanitized via sanitizeName and truncated by code points (empty falls back to "配置N" in saveSlot)
}
```

## Usage Examples

### API Call Chain

```cpp
// 1. Get current config
AutoModeConfig c = autoMode->getConfig();

// 2. Save to slot 0
configMgr->saveSlot(0, "Game Config", c);
configMgr->setActiveSlot(0);

// 3. Load from slot 1
AutoModeConfig loaded;
String name;
if (configMgr->loadSlot(1, loaded, name)) {
    autoMode->setConfig(loaded);
}

// 4. Export as JSON
String json = configMgr->exportConfig(c, "My Config");

// 5. Import from JSON
AutoModeConfig imported;
String importName;
if (configMgr->importConfig(json, imported, importName)) {
    autoMode->setConfig(imported);
}
```

## Auth Credentials & BLE Name

### Auth Credentials (ENABLE_WEB_AUTH only)

```cpp
String getAuthUser();                          // Username (NVS, falls back to WEB_AUTH_USERNAME)
String getAuthPass();                          // Password SHA-256 hash (NVS, falls back to default hash)
bool   hasAuthCredentials();                   // Whether NVS credentials are initialized
bool   setAuthCredentials(const String& user, const String& pass);  // Write to NVS (1-20 chars each)
static String sha256Hex(const String& input);  // SHA-256 hex (mbedtls implementation)
```

- On first boot (with `ENABLE_WEB_AUTH` enabled), credentials are seeded from `WEB_AUTH_USERNAME / WEB_AUTH_PASSWORD` in `config.h` if NVS has none
- Only the SHA-256 hash is stored, never plaintext; login/change verify via `handleLogin` / `handleAuthChange`

### BLE Device Name

```cpp
String getBleName();        // Read the name (NVS, falls back to BLE_DEVICE_NAME)
bool   setBleName(const String& name);  // Write to NVS (1-24 chars); BLE restart required to apply
```

## Sequence Mode Slots

Sequence mode has 5 independent slots, separate from the auto mode slots:

```cpp
bool   saveSeqSlot(int slot, const String& name, const SeqConfig& config); // Save (JSON string; rolls back on write failure)
bool   loadSeqSlot(int slot, SeqConfig& config, String& name);             // Load
bool   deleteSeqSlot(int slot);                                            // Delete
bool   isSeqSlotUsed(int slot);
String getSeqSlotName(int slot);
void   setActiveSeqSlot(int slot);   // -1 = default
int    getActiveSeqSlot();
bool   loadActiveSeqConfig(SeqConfig& config);   // Auto-load active seq config on startup
String seqConfigToJson(const SeqConfig& config, const String& name);                     // Pure serialization (read/export)
bool   seqConfigToJsonChecked(const SeqConfig& config, const String& name, String& out); // Serialize + size guard (write path)
SeqParseResult seqJsonToConfig(const String& json, SeqConfig& config, String& name);
```

Sequence config JSON format v2 (per step: `k`=key name, empty means a pause step; `h`=hold ms, `g`=gap ms; `r`=repeat count; `rk`=random key; `hr`/`gr`=random timing min–max):

```json
{
  "version": 2,
  "name": "My Sequence",
  "loop": false,
  "loopGapMs": 1000,
  "steps": [
    {"k": "w", "h": 120, "g": 300},
    {"k": "space", "h": 80, "g": 200, "r": 3},
    {"k": "w", "h": 200, "g": 300, "rk": 1},
    {"k": "a", "h": 100, "g": 100, "hr": [60, 180], "gr": [100, 400]}
  ]
}
```

Parse & guards:

- `version` defaults to 1 when absent (compat with old "export current" files); explicit 1/2 accepted, others rejected; output is always 2 (`r==1`, `rk==0`, and disabled `hr`/`gr` are omitted; `k`/`h`/`g` always emitted, `k` preserved when `rk=1`)
- More than `SEQ_MAX_STEPS` (64) steps → `SEQ_PARSE_TOO_MANY`; re-serialized size over `SEQ_JSON_MAX_BYTES` (3000B) → `SEQ_PARSE_TOO_LARGE`
- `hr`/`gr` must have length 2; min>max is swapped; values clamped to 10–10000ms; `r` clamped to 1–99; hold/gap clamped to 10–10000ms; loopGap clamped to 0–10000ms
- Non-empty key names must map to valid HID keycodes, otherwise they degrade to a pause step

NVS budget: 20,480B total; 3000B × 5 slots ≈ 15KB plus auto slots/credentials ≈ 19KB. `saveSeqSlot` checks the `putString` return value; on failure it removes the partial data, rolls back `sqXu=false`, and returns false (handler responds 500 `storage full`).

## Export All

`exportAllConfigs()` merges all 5 auto slots and 5 sequence slots into a single JSON:

```json
{
  "app": "ESPVirtualKeyboard",
  "version": 1,
  "auto": [ {"used": true, "name": "...", "config": {...}}, ... ],
  "seq":  [ {"used": true, "name": "...", "config": {...}}, ... ]
}
```

Each `config` object also carries `name` (mirroring the item-level `name`) so that export → import round-trips never lose names (the frontend additionally merges the item-level `name` into the config on import, for backward compatibility with older files).

Downloaded via `GET /api/config/export-all`. On import, the frontend parses the file and compares each entry with the device's existing slots (identical ones are skipped; differing ones default to the **next empty slot** for their mode, auto-advancing as items are imported — adjustable/skippable).