#ifndef KEYMAP_H
#define KEYMAP_H

#include <Arduino.h>

// Web 键名 → HID 键码。未知键名返回 0xFF。
uint8_t webKeyToHid(const String& key);

#endif // KEYMAP_H
