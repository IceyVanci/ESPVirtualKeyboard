#include "random_util.h"
#include "config.h"
#include <esp_system.h>

// 10 键 HID 码表（顺序与随机模式权重一致）
const uint8_t RANDOM_KEY_HID[10] = {
  HID_KEY_W, HID_KEY_S, HID_KEY_A, HID_KEY_D,
  HID_KEY_LEFT_ARROW, HID_KEY_RIGHT_ARROW, HID_KEY_SPACE,
  HID_KEY_C, HID_KEY_Z,
  0x00 // 空闲，不按任何键
};

// 10 键键名表
const char* const RANDOM_KEY_NAME[10] = {
  "w", "s", "a", "d", "left", "right", "space", "c", "z", ""
};

void randomUtilInit() {
  randomSeed(esp_random()); // 使用硬件随机源初始化随机数种子
}

unsigned long gaussianInRange(unsigned long minV, unsigned long maxV) {
  if (minV > maxV) {
    unsigned long tmp = minV;
    minV = maxV;
    maxV = tmp;
  }
  if (minV == maxV) return minV;

  // Box-Muller 变换：将均匀分布转换为正态分布（与自动模式原实现一致）
  float mean = (minV + maxV) / 2.0;
  float stddev = (maxV - minV) / 6.0;
  float u1 = (float)random(1, 10000) / 10000.0; // 避免 0
  float u2 = (float)random(0, 10000) / 10000.0;
  float z = sqrt(-2.0 * log(u1)) * cos(2.0 * PI * u2);
  float v = mean + z * stddev;

  if (v < minV) v = minV;
  if (v > maxV) v = maxV;
  return (unsigned long)v;
}

uint8_t pickWeightedHidKey(const RandomKeyWeights& weights) {
  float totalWeight = 0;
  for (int i = 0; i < 10; i++) {
    totalWeight += weights.w[i];
  }
  if (totalWeight <= 0) return 0x00;

  // 加权随机选择（与自动模式原实现一致）
  float r = (float)random(0, 10000) / 10000.0 * totalWeight;
  float cumulative = 0;
  for (int i = 0; i < 10; i++) {
    cumulative += weights.w[i];
    if (r <= cumulative) {
      return RANDOM_KEY_HID[i];
    }
  }
  return RANDOM_KEY_HID[0]; // 默认返回 W
}
