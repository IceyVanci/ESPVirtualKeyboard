# 配置管理

## 概述

`config_manager.h/cpp` 实现了基于 ESP32 NVS（非易失性存储）的配置持久化系统。它提供 5 个配置槽位，支持保存、加载、删除、导入和导出操作，以及活动槽位的自动加载。

## NVS 存储布局

使用 Preferences 库，命名空间为 `cfgmgr`：

| NVS 键 | 类型 | 说明 |
|--------|------|------|
| `s0n` ~ `s4n` | String | 槽位 0-4 的名称（最大 20 字符） |
| `s0d` ~ `s4d` | Bytes | 槽位 0-4 的二进制配置数据（sizeof AutoModeConfig） |
| `s0u` ~ `s4u` | Bool | 槽位 0-4 的使用标记 |
| `active` | Int | 当前活动槽位索引（-1 = 默认配置） |

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

1. 截断名称到 `SLOT_NAME_MAX_LEN`（20 字符）
2. 写入 NVS：名称字符串、二进制配置数据、使用标记 true
3. 返回是否成功

### 加载槽位 (loadSlot)

```cpp
bool loadSlot(int slotIndex, AutoModeConfig& config, String& name);
```

1. 检查使用标记，未使用则返回 false
2. 读取名称字符串
3. 读取二进制配置数据（验证长度等于 sizeof(AutoModeConfig)）

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

### 手写 JSON 解析

为避免依赖 `ArduinoJson` 库，项目实现了简易的 JSON 解析器：

```cpp
// 提取数值
static float jsonGetFloat(const String& json, const String& key, float defaultVal);
static long jsonGetLong(const String& json, const String& key, long defaultVal);
static bool jsonGetBool(const String& json, const String& key, bool defaultVal);
static String jsonGetString(const String& json, const String& key, const String& defaultVal);
```

解析过程：
1. 在 JSON 字符串中查找 `"key"`
2. 找到冒号 `:` 后的值
3. 提取到下一个逗号 `,` 或右花括号 `}`
4. 转换为对应类型

### 导入验证

```cpp
bool ConfigManager::jsonToConfig(const String& json, AutoModeConfig& config, String& name) {
    // 1. 验证 version 字段为 1
    // 2. 提取各个字段
    // 3. 范围验证：
    //    - minIntervalMs: [100, 30000]
    //    - maxIntervalMs: [100, 30000]
    //    - minHoldMs: [10, 5000]
    //    - maxHoldMs: [10, 5000]
    //    - 所有权重: [0, 1]
    // 4. 自动交换 min/max（如果 min > max）
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

---

# Configuration Management

## Overview

The `config_manager.h/cpp` implements a configuration persistence system based on ESP32 NVS (Non-Volatile Storage). It provides 5 configuration slots supporting save, load, delete, import, and export operations, along with auto-loading of the active slot.

## NVS Storage Layout

Uses the Preferences library, namespace `cfgmgr`:

| NVS Key | Type | Description |
|---------|------|-------------|
| `s0n` ~ `s4n` | String | Slot 0-4 names (max 20 chars) |
| `s0d` ~ `s4d` | Bytes | Slot 0-4 binary config data (sizeof AutoModeConfig) |
| `s0u` ~ `s4u` | Bool | Slot 0-4 used flags |
| `active` | Int | Active slot index (-1 = default config) |

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
2. Write to NVS: name string, binary config data, used flag true
3. Return success status

### Load Slot (loadSlot)

```cpp
bool loadSlot(int slotIndex, AutoModeConfig& config, String& name);
```

1. Check used flag, return false if unused
2. Read name string
3. Read binary config data (verify length equals sizeof(AutoModeConfig))

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

### Hand-written JSON Parsing

To avoid depending on the `ArduinoJson` library, the project implements a simple JSON parser:

```cpp
// Extract numeric values
static float jsonGetFloat(const String& json, const String& key, float defaultVal);
static long jsonGetLong(const String& json, const String& key, long defaultVal);
static bool jsonGetBool(const String& json, const String& key, bool defaultVal);
static String jsonGetString(const String& json, const String& key, const String& defaultVal);
```

Parsing process:
1. Search for `"key"` in JSON string
2. Find value after colon `:`
3. Extract until next comma `,` or closing brace `}`
4. Convert to target type

### Import Validation

```cpp
bool ConfigManager::jsonToConfig(const String& json, AutoModeConfig& config, String& name) {
    // 1. Verify version field is 1
    // 2. Extract each field
    // 3. Range validation:
    //    - minIntervalMs: [100, 30000]
    //    - maxIntervalMs: [100, 30000]
    //    - minHoldMs: [10, 5000]
    //    - maxHoldMs: [10, 5000]
    //    - All weights: [0, 1]
    // 4. Auto-swap min/max (if min > max)
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