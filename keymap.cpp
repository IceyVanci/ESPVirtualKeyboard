#include "keymap.h"
#include "config.h"

uint8_t webKeyToHid(const String& key) {
  // 修饰键（返回哨兵值，BleKeyboard 识别并按位写入修饰字节）
  if (key=="lctrl")return HID_SENTINEL_LCTRL; if (key=="rctrl")return HID_SENTINEL_RCTRL;
  if (key=="lshift"||key=="shift")return HID_SENTINEL_LSHIFT; if (key=="rshift")return HID_SENTINEL_RSHIFT;
  if (key=="lalt")return HID_SENTINEL_LALT; if (key=="ralt")return HID_SENTINEL_RALT;
  if (key=="lwin"||key=="lmeta"||key=="lcmd")return HID_SENTINEL_LGUI;
  if (key=="rwin"||key=="rmeta")return HID_SENTINEL_RGUI;
  if (key=="a")return HID_KEY_A; if (key=="b")return HID_KEY_B; if (key=="c")return HID_KEY_C;
  if (key=="d")return HID_KEY_D; if (key=="e")return HID_KEY_E; if (key=="f")return HID_KEY_F;
  if (key=="g")return HID_KEY_G; if (key=="h")return HID_KEY_H; if (key=="i")return HID_KEY_I;
  if (key=="j")return HID_KEY_J; if (key=="k")return HID_KEY_K; if (key=="l")return HID_KEY_L;
  if (key=="m")return HID_KEY_M; if (key=="n")return HID_KEY_N; if (key=="o")return HID_KEY_O;
  if (key=="p")return HID_KEY_P; if (key=="q")return HID_KEY_Q; if (key=="r")return HID_KEY_R;
  if (key=="s")return HID_KEY_S; if (key=="t")return HID_KEY_T; if (key=="u")return HID_KEY_U;
  if (key=="v")return HID_KEY_V; if (key=="w")return HID_KEY_W; if (key=="x")return HID_KEY_X;
  if (key=="y")return HID_KEY_Y; if (key=="z")return HID_KEY_Z;
  if (key=="1")return HID_KEY_1; if (key=="2")return HID_KEY_2; if (key=="3")return HID_KEY_3;
  if (key=="4")return HID_KEY_4; if (key=="5")return HID_KEY_5; if (key=="6")return HID_KEY_6;
  if (key=="7")return HID_KEY_7; if (key=="8")return HID_KEY_8; if (key=="9")return HID_KEY_9;
  if (key=="0")return HID_KEY_0;
  if (key=="enter")return HID_KEY_ENTER; if (key=="esc")return HID_KEY_ESC;
  if (key=="backspace")return HID_KEY_BACKSPACE; if (key=="tab")return HID_KEY_TAB;
  if (key=="space")return HID_KEY_SPACE; if (key=="minus")return HID_KEY_MINUS;
  if (key=="equal")return HID_KEY_EQUAL; if (key=="lbracket")return HID_KEY_LBRACKET;
  if (key=="rbracket")return HID_KEY_RBRACKET; if (key=="backslash")return HID_KEY_BACKSLASH;
  if (key=="semicolon")return HID_KEY_SEMICOLON; if (key=="apostrophe")return HID_KEY_APOSTROPHE;
  if (key=="grave")return HID_KEY_GRAVE; if (key=="comma")return HID_KEY_COMMA;
  if (key=="period")return HID_KEY_PERIOD; if (key=="slash")return HID_KEY_SLASH;
  if (key=="capslock")return HID_KEY_CAPSLOCK;
  if (key=="f1")return HID_KEY_F1; if (key=="f2")return HID_KEY_F2; if (key=="f3")return HID_KEY_F3;
  if (key=="f4")return HID_KEY_F4; if (key=="f5")return HID_KEY_F5; if (key=="f6")return HID_KEY_F6;
  if (key=="f7")return HID_KEY_F7; if (key=="f8")return HID_KEY_F8; if (key=="f9")return HID_KEY_F9;
  if (key=="f10")return HID_KEY_F10; if (key=="f11")return HID_KEY_F11; if (key=="f12")return HID_KEY_F12;
  if (key=="up")return HID_KEY_UP_ARROW; if (key=="down")return HID_KEY_DOWN_ARROW;
  if (key=="left")return HID_KEY_LEFT_ARROW; if (key=="right")return HID_KEY_RIGHT_ARROW;
  if (key=="insert")return HID_KEY_INSERT; if (key=="home")return HID_KEY_HOME;
  if (key=="pageup")return HID_KEY_PAGEUP; if (key=="delete")return HID_KEY_DELETE;
  if (key=="end")return HID_KEY_END; if (key=="pagedown")return HID_KEY_PAGEDOWN;
  if (key=="numpad0")return HID_KEY_NUMPAD_0; if (key=="numpad1")return HID_KEY_NUMPAD_1;
  if (key=="numpad2")return HID_KEY_NUMPAD_2; if (key=="numpad3")return HID_KEY_NUMPAD_3;
  if (key=="numpad4")return HID_KEY_NUMPAD_4; if (key=="numpad5")return HID_KEY_NUMPAD_5;
  if (key=="numpad6")return HID_KEY_NUMPAD_6; if (key=="numpad7")return HID_KEY_NUMPAD_7;
  if (key=="numpad8")return HID_KEY_NUMPAD_8; if (key=="numpad9")return HID_KEY_NUMPAD_9;
  if (key=="numpadadd")return HID_KEY_NUMPAD_ADD; if (key=="numpadsub")return HID_KEY_NUMPAD_SUB;
  if (key=="numpadmul")return HID_KEY_NUMPAD_MUL; if (key=="numpaddiv")return HID_KEY_NUMPAD_DIV;
  if (key=="numpaddot")return HID_KEY_NUMPAD_DOT; if (key=="numpadenter")return HID_KEY_NUMPAD_ENTER;
  if (key=="numlock")return HID_KEY_NUMLOCK;
  return 0xFF;
}
