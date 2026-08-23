#include "seq_mode.h"
#include "ble_keyboard.h"
#include "keymap.h"

SequenceMode::SequenceMode(BleKeyboard* keyboard)
  : _keyboard(keyboard), _playing(false), _curStep(0),
    _stepStart(0), _state(SEQ_HOLD) {}

void SequenceMode::setConfig(const SeqConfig& config) {
  if (_playing) stop();
  _config = config;
  // 预计算 HID 码（空键名 = 0，表示暂停步骤）
  for (int i = 0; i < _config.stepCount; i++) {
    String k = _config.steps[i].keyName;
    _hidCodes[i] = (k.length() == 0) ? 0 : webKeyToHid(k);
  }
}

SeqConfig SequenceMode::getConfig() const {
  return _config;
}

void SequenceMode::setPlaying(bool on) {
  if (on == _playing) return;
  if (on) {
    if (_config.stepCount == 0) return;
    _playing = true;
    _curStep = 0;
    _state = SEQ_HOLD;
    _stepStart = millis();
    if (_hidCodes[0] != 0) _keyboard->press(_hidCodes[0]);
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

void SequenceMode::update() {
  if (!_playing) return;
  unsigned long now = millis();

  switch (_state) {
    case SEQ_HOLD:
      // 等待 holdMs（空步骤时即暂停时长）
      if (now - _stepStart >= _config.steps[_curStep].holdMs) {
        if (_hidCodes[_curStep] != 0) _keyboard->release(_hidCodes[_curStep]);
        if (_curStep + 1 < _config.stepCount) {
          _state = SEQ_GAP;
          _stepStart = now;
        } else if (_config.loop) {
          _state = SEQ_LOOPGAP;
          _stepStart = now;
        } else {
          stop();
        }
      }
      break;

    case SEQ_GAP:
      if (now - _stepStart >= _config.steps[_curStep].gapMs) {
        _curStep++;
        _state = SEQ_HOLD;
        _stepStart = now;
        if (_hidCodes[_curStep] != 0) _keyboard->press(_hidCodes[_curStep]);
      }
      break;

    case SEQ_LOOPGAP:
      if (now - _stepStart >= _config.loopGapMs) {
        _curStep = 0;
        _state = SEQ_HOLD;
        _stepStart = now;
        if (_hidCodes[0] != 0) _keyboard->press(_hidCodes[0]);
      }
      break;

    default:
      _state = SEQ_HOLD;
      break;
  }
}
