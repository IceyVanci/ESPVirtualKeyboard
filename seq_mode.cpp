#include "seq_mode.h"
#include "ble_keyboard.h"
#include "keymap.h"

SequenceMode::SequenceMode(BleKeyboard* keyboard)
  : _keyboard(keyboard), _playing(false), _curStep(0), _repeatDone(0),
    _curKey(0), _curHold(0), _curGap(0), _stepStart(0), _state(SEQ_HOLD) {
  memset(_weights.w, 0, sizeof(_weights.w));
}

void SequenceMode::setConfig(const SeqConfig& config) {
  if (_playing) stop();
  _config = config;
  // 预计算 HID 码：空键名/随机键 = 0（不直接按键）；非法键名降级为暂停，杜绝 0xFF
  for (int i = 0; i < _config.stepCount; i++) {
    if (_config.steps[i].randomKey) { _hidCodes[i] = 0; continue; }
    String k = _config.steps[i].keyName;
    _hidCodes[i] = (k.length() == 0) ? 0 : webKeyToHid(k);
    if (_hidCodes[i] == 0xFF) {
      _hidCodes[i] = 0;
      _config.steps[i].keyName = "";
    }
  }
}

SeqConfig SequenceMode::getConfig() const {
  return _config;
}

void SequenceMode::setRandomWeights(const RandomKeyWeights& weights) {
  _weights = weights;
}

void SequenceMode::setPlaying(bool on) {
  if (on == _playing) return;
  if (on) {
    if (_config.stepCount == 0) return;
    _playing = true;
    _curStep = 0;
    _repeatDone = 0;
    _state = SEQ_HOLD;
    beginStep();
  } else {
    stop();
  }
}

void SequenceMode::stop() {
  _playing = false;
  _keyboard->releaseAll();
}

bool SequenceMode::isPlaying() const {
  return _playing;
}

// 开始当前步：抽随机键/随机时序，按下按键（空闲/暂停不按）
void SequenceMode::beginStep() {
  const SeqStep& s = _config.steps[_curStep];
  _curKey = s.randomKey ? pickWeightedHidKey(_weights) : _hidCodes[_curStep];
  _curHold = (s.holdMinMs > 0) ? (uint16_t)gaussianInRange(s.holdMinMs, s.holdMaxMs) : s.holdMs;
  _curGap  = (s.gapMinMs > 0)  ? (uint16_t)gaussianInRange(s.gapMinMs,  s.gapMaxMs)  : s.gapMs;
  _state = SEQ_HOLD;
  _stepStart = millis();
  if (_curKey != 0) _keyboard->press(_curKey);
}

void SequenceMode::update() {
  if (!_playing) return;
  unsigned long now = millis();

  switch (_state) {
    case SEQ_HOLD:
      // 等待 holdMs（空闲/暂停时即等待时长）
      if (now - _stepStart >= _curHold) {
        if (_curKey != 0) _keyboard->release(_curKey);
        _state = SEQ_GAP;
        _stepStart = now;
      }
      break;

    case SEQ_GAP:
      // 每步（含最后一步、含重复）都走 GAP 后再重复/前进/循环/停止
      if (now - _stepStart >= _curGap) {
        _repeatDone++;
        if (_repeatDone < _config.steps[_curStep].repeat) {
          beginStep();                       // 重复：重新抽随机键/时序
        } else if (_curStep + 1 < _config.stepCount) {
          _curStep++;
          _repeatDone = 0;
          beginStep();
        } else if (_config.loop) {
          _state = SEQ_LOOPGAP;
          _stepStart = now;
        } else {
          stop();
        }
      }
      break;

    case SEQ_LOOPGAP:
      if (now - _stepStart >= _config.loopGapMs) {
        _curStep = 0;
        _repeatDone = 0;
        beginStep();
      }
      break;

    default:
      _state = SEQ_HOLD;
      break;
  }
}
