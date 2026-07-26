# 自动模式

## 概述

`auto_mode.h/cpp` 实现了可配置的自动按键模拟功能。它通过加权随机算法选择按键，使用正态分布生成按键间隔和持续时间，使模拟行为更接近真人操作。

## 状态机

自动模式使用三状态有限状态机（FSM）管理按键生命周期：

```
                    ┌─────────────────────┐
                    │    AUTO_IDLE        │
                    │  等待下一次按键      │
                    └─────────┬───────────┘
                              │ 间隔时间到达
                              ▼
                    ┌─────────────────────┐
                    │   AUTO_PRESSING     │
                    │  正在按住按键       │
                    └─────────┬───────────┘
                              │ holdTime 到期
                              ▼
                    ┌─────────────────────┐
                    │   AUTO_RELEASING    │
                    │  按键已释放，等待间隔│
                    └─────────┬───────────┘
                              │ 间隔时间到达
                              │（回到 IDLE 重新开始）
                              ▼
                    ┌─────────────────────┐
                    │    AUTO_IDLE        │
                    └─────────────────────┘
```

### 状态说明

| 状态 | 含义 | 进入条件 | 退出条件 |
|------|------|----------|----------|
| `AUTO_IDLE` | 等待下一次按键 | 释放后 / 开机 | 已到间隔时间 |
| `AUTO_PRESSING` | 正在按住按键 | 选择了有效按键 | holdTime 到期 |
| `AUTO_RELEASING` | 释放后等待 | 释放按键后 | 间隔时间到达 |

## 加权随机算法

### 按键选择

共有 10 种可能的动作，每种有对应的权重：

| 索引 | 按键 | 默认权重 | 对应 HID 键码 |
|------|------|----------|---------------|
| 0 | W (前进) | 0.30 | 0x1A |
| 1 | S (后退) | 0.08 | 0x16 |
| 2 | A (左移) | 0.12 | 0x04 |
| 3 | D (右移) | 0.12 | 0x07 |
| 4 | ← (左转) | 0.10 | 0x50 |
| 5 | → (右转) | 0.10 | 0x4F |
| 6 | Space (跳) | 0.08 | 0x2C |
| 7 | C (蹲下) | 0.05 | 0x06 |
| 8 | Z (趴下) | 0.05 | 0x1D |
| 9 | Idle (空闲) | 0.08 | 0x00 |

### 选择算法

```cpp
// 1. 计算权重总和
float totalWeight = sum(weights[0..9]);

// 2. 生成 [0, totalWeight) 范围的随机数
float r = random(0, 10000) / 10000.0 * totalWeight;

// 3. 累加权重，选择第一个使累积和 ≥ r 的选项
float cumulative = 0;
for (int i = 0; i < 10; i++) {
    cumulative += weights[i];
    if (r <= cumulative) return keys[i];
}
```

### 权重归一化

当所有权重总和 > 1.0 时，自动等比缩放到总和为 1.0：

```cpp
if (sum > 1.0f && sum > 0.0f) {
    float scale = 1.0f / sum;
    // 每个权重 *= scale
}
```

## 时间间隔生成

使用 Box-Muller 变换生成正态分布的随机时间间隔，使按键行为更自然：

### Box-Muller 变换

```cpp
float boxMullerRandom(float mean, float stddev) {
    float u1 = random(1, 10000) / 10000.0;  // 避免 0
    float u2 = random(0, 10000) / 10000.0;
    float z = sqrt(-2.0 * log(u1)) * cos(2.0 * PI * u2);
    return mean + z * stddev;
}
```

### 间隔计算

- **均值** (mean) = (min + max) / 2
- **标准差** (stddev) = (max - min) / 6.0（确保 99.7% 的值在范围内）
- 最终结果被限制在 [min, max] 范围内

### 默认参数

| 参数 | 最小值 | 最大值 | 均值 | 典型范围 |
|------|--------|--------|------|----------|
| 按键间隔 | 800ms | 4000ms | 2400ms | 800-4000ms |
| 按键持续时间 | 80ms | 600ms | 340ms | 80-600ms |

## 按键事件日志

### 环形缓冲区

使用 20 条记录的环形缓冲区存储最近的按键事件：

```cpp
KeyEvent _events[EVENT_BUFFER_SIZE];  // 环形缓冲区
int _eventWritePos;                   // 写入位置
uint32_t _eventCounter;               // 递增事件 ID
```

### KeyEvent 结构

```cpp
struct KeyEvent {
    String keyName;        // 按键名称（如 "W", "Space"）
    uint8_t keyIndex;      // 按键索引 (0-9)
    bool pressed;          // true=按下, false=释放
    unsigned long timestamp; // millis() 时间戳
    uint32_t eventId;      // 递增事件 ID（用于增量拉取）
};
```

### 前端增量拉取

前端通过 `since` 参数增量拉取新事件，避免重复传输：

```javascript
fetch('/api/events?since=' + lastEventId)
    .then(r => r.json())
    .then(data => {
        data.events.forEach(ev => addLogLine(ev.key, ev.down, ev.t));
        lastEventId = data.lastId;
    });
```

## 按键统计

维护 10 个按键的按下次数计数器：

```cpp
uint32_t _keyCounts[10];  // W,S,A,D,TL,TR,SP,C,Z,Idle
uint32_t _totalCount;     // 总按键次数
```

- 每次 `logKeyEvent()` 被调用且为按下事件时递增对应计数器
- `getKeyCount(index)` 获取单个按键次数
- `getTotalCount()` 获取总次数

---

# Auto Mode

## Overview

The `auto_mode.h/cpp` implements a configurable automatic key simulation feature. It uses weighted random algorithms for key selection and normal distribution for timing generation, making the simulation behavior more natural.

## State Machine

The auto mode uses a three-state finite state machine (FSM) to manage the key lifecycle:

```
                    ┌─────────────────────┐
                    │     AUTO_IDLE       │
                    │  Waiting for next   │
                    └─────────┬───────────┘
                              │ Interval reached
                              ▼
                    ┌─────────────────────┐
                    │   AUTO_PRESSING     │
                    │  Key is held down   │
                    └─────────┬───────────┘
                              │ holdTime expired
                              ▼
                    ┌─────────────────────┐
                    │   AUTO_RELEASING    │
                    │  Released, waiting  │
                    └─────────┬───────────┘
                              │ Interval reached
                              │ (back to IDLE)
                              ▼
                    ┌─────────────────────┐
                    │     AUTO_IDLE       │
                    └─────────────────────┘
```

### State Descriptions

| State | Meaning | Entry Condition | Exit Condition |
|-------|---------|-----------------|----------------|
| `AUTO_IDLE` | Waiting for next key | After release / startup | Interval reached |
| `AUTO_PRESSING` | Key is held down | Valid key selected | holdTime expired |
| `AUTO_RELEASING` | Released, waiting | After key release | Interval reached |

## Weighted Random Algorithm

### Key Selection

10 possible actions with corresponding weights:

| Index | Key | Default Weight | HID Keycode |
|-------|-----|---------------|-------------|
| 0 | W (Forward) | 0.30 | 0x1A |
| 1 | S (Back) | 0.08 | 0x16 |
| 2 | A (Left) | 0.12 | 0x04 |
| 3 | D (Right) | 0.12 | 0x07 |
| 4 | ← (Turn Left) | 0.10 | 0x50 |
| 5 | → (Turn Right) | 0.10 | 0x4F |
| 6 | Space (Jump) | 0.08 | 0x2C |
| 7 | C (Crouch) | 0.05 | 0x06 |
| 8 | Z (Prone) | 0.05 | 0x1D |
| 9 | Idle | 0.08 | 0x00 |

### Selection Algorithm

```cpp
// 1. Calculate total weight
float totalWeight = sum(weights[0..9]);

// 2. Generate random number in [0, totalWeight)
float r = random(0, 10000) / 10000.0 * totalWeight;

// 3. Accumulate weights, select first where cumulative ≥ r
float cumulative = 0;
for (int i = 0; i < 10; i++) {
    cumulative += weights[i];
    if (r <= cumulative) return keys[i];
}
```

### Weight Normalization

When total weight sum > 1.0, automatically scales down proportionally to sum = 1.0:

```cpp
if (sum > 1.0f && sum > 0.0f) {
    float scale = 1.0f / sum;
    // Each weight *= scale
}
```

## Timing Generation

Uses Box-Muller transform to generate normally distributed random intervals for more natural behavior:

### Box-Muller Transform

```cpp
float boxMullerRandom(float mean, float stddev) {
    float u1 = random(1, 10000) / 10000.0;  // Avoid 0
    float u2 = random(0, 10000) / 10000.0;
    float z = sqrt(-2.0 * log(u1)) * cos(2.0 * PI * u2);
    return mean + z * stddev;
}
```

### Interval Calculation

- **Mean** = (min + max) / 2
- **Standard deviation** = (max - min) / 6.0 (ensures 99.7% of values within range)
- Result is clamped to [min, max] range

### Default Parameters

| Parameter | Min | Max | Mean | Typical Range |
|-----------|-----|-----|------|---------------|
| Key Interval | 800ms | 4000ms | 2400ms | 800-4000ms |
| Key Hold Time | 80ms | 600ms | 340ms | 80-600ms |

## Key Event Log

### Ring Buffer

Uses a 20-entry ring buffer to store recent key events:

```cpp
KeyEvent _events[EVENT_BUFFER_SIZE];  // Ring buffer
int _eventWritePos;                   // Write position
uint32_t _eventCounter;               // Incremental event ID
```

### KeyEvent Structure

```cpp
struct KeyEvent {
    String keyName;        // Key name (e.g., "W", "Space")
    uint8_t keyIndex;      // Key index (0-9)
    bool pressed;          // true=pressed, false=released
    unsigned long timestamp; // millis() timestamp
    uint32_t eventId;      // Incremental event ID (for incremental polling)
};
```

### Frontend Incremental Polling

The frontend uses the `since` parameter to incrementally fetch new events:

```javascript
fetch('/api/events?since=' + lastEventId)
    .then(r => r.json())
    .then(data => {
        data.events.forEach(ev => addLogLine(ev.key, ev.down, ev.t));
        lastEventId = data.lastId;
    });
```

## Key Statistics

Maintains press counters for 10 keys:

```cpp
uint32_t _keyCounts[10];  // W,S,A,D,TL,TR,SP,C,Z,Idle
uint32_t _totalCount;     // Total key presses
```

- Each call to `logKeyEvent()` with a press event increments the corresponding counter
- `getKeyCount(index)` gets individual key press count
- `getTotalCount()` gets total count