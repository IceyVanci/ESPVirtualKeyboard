#ifndef RANDOM_UTIL_H
#define RANDOM_UTIL_H

#include <Arduino.h>

// 10 键权重（顺序固定：W,S,A,D,Left,Right,Space,C,Z,Idle）
struct RandomKeyWeights {
  float w[10];
};

// 10 键 HID 码表（与权重索引一一对应；末位 0x00 = 空闲，不按任何键）
extern const uint8_t RANDOM_KEY_HID[10];

// 10 键键名表（与权重索引一一对应；末位空串 = 空闲）
extern const char* const RANDOM_KEY_NAME[10];

// 初始化随机数种子（基于硬件随机源；setup() 中调用一次）
void randomUtilInit();

// Box-Muller 正态范围随机：mean=(min+max)/2、stddev=(max-min)/6，结果夹取回 [min,max]
// min==max 时恒返回该值；min>max 自动交换
unsigned long gaussianInRange(unsigned long minV, unsigned long maxV);

// 10 键加权轮盘抽取：返回 HID 码，0=空闲；总权重<=0 返回 0；累计比较兜底返回首键
uint8_t pickWeightedHidKey(const RandomKeyWeights& weights);

#endif // RANDOM_UTIL_H
