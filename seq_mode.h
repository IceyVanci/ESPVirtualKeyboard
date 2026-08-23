#ifndef SEQ_MODE_H
#define SEQ_MODE_H

#include <Arduino.h>
#include "config.h"

// 顺序模式单步：keyName 为空表示"暂停步骤"（只等待 holdMs，不按键）
struct SeqStep {
  String    keyName;
  uint16_t  holdMs;   // 按键持续时间 / 暂停时长
  uint16_t  gapMs;    // 释放后到下一步的间隔
};

// 顺序模式配置（一条完整的按键序列）
struct SeqConfig {
  bool      loop;      // 是否循环播放
  uint16_t  loopGapMs; // 两次循环之间的周期
  uint8_t   stepCount; // 有效步数
  SeqStep   steps[SEQ_MAX_STEPS];

  SeqConfig() : loop(false), loopGapMs(1000), stepCount(0) {}
};

// 前向声明
class BleKeyboard;

// 顺序模式：非阻塞回放状态机
// 录制由 Web 前端完成（基于按键事件时间戳），设备负责存储/回放/承载编辑数据
class SequenceMode {
public:
  SequenceMode(BleKeyboard* keyboard);

  void setConfig(const SeqConfig& config);
  SeqConfig getConfig() const;

  void setPlaying(bool on);
  bool isPlaying() const;
  void stop();
  void update();   // 主循环调用

private:
  BleKeyboard* _keyboard;
  SeqConfig    _config;
  uint8_t      _hidCodes[SEQ_MAX_STEPS]; // 预计算的 HID 码（0=空/暂停）
  bool         _playing;
  uint8_t      _curStep;
  unsigned long _stepStart;

  enum SeqPlayState { SEQ_HOLD, SEQ_GAP, SEQ_LOOPGAP };
  SeqPlayState _state;
};

#endif // SEQ_MODE_H
