#ifndef CONFIG_H
#define CONFIG_H

// ========== WiFi 配置 ==========
// 请修改为你家的 WiFi 信息
#define WIFI_SSID     "YOUR_WIFI_NAME"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// ========== BLE 配置 ==========
#define BLE_DEVICE_NAME "ESP Virtual Keyboard"

// ========== Web 服务器配置 ==========
#define WEB_SERVER_PORT 80

// ========== 串口配置 ==========
#define SERIAL_BAUD_RATE 115200

// ========== HID 键码定义 (USB HID Usage Table) ==========
// 字母键
#define HID_KEY_A      0x04
#define HID_KEY_B      0x05
#define HID_KEY_C      0x06
#define HID_KEY_D      0x07
#define HID_KEY_E      0x08
#define HID_KEY_F      0x09
#define HID_KEY_G      0x0A
#define HID_KEY_H      0x0B
#define HID_KEY_I      0x0C
#define HID_KEY_J      0x0D
#define HID_KEY_K      0x0E
#define HID_KEY_L      0x0F
#define HID_KEY_M      0x10
#define HID_KEY_N      0x11
#define HID_KEY_O      0x12
#define HID_KEY_P      0x13
#define HID_KEY_Q      0x14
#define HID_KEY_R      0x15
#define HID_KEY_S      0x16
#define HID_KEY_T      0x17
#define HID_KEY_U      0x18
#define HID_KEY_V      0x19
#define HID_KEY_W      0x1A
#define HID_KEY_X      0x1B
#define HID_KEY_Y      0x1C
#define HID_KEY_Z      0x1D

// 数字键
#define HID_KEY_1      0x1E
#define HID_KEY_2      0x1F
#define HID_KEY_3      0x20
#define HID_KEY_4      0x21
#define HID_KEY_5      0x22
#define HID_KEY_6      0x23
#define HID_KEY_7      0x24
#define HID_KEY_8      0x25
#define HID_KEY_9      0x26
#define HID_KEY_0      0x27

// 功能键
#define HID_KEY_ENTER      0x28
#define HID_KEY_ESC        0x29
#define HID_KEY_BACKSPACE  0x2A
#define HID_KEY_TAB        0x2B
#define HID_KEY_SPACE      0x2C
#define HID_KEY_MINUS      0x2D
#define HID_KEY_EQUAL      0x2E
#define HID_KEY_LBRACKET   0x2F
#define HID_KEY_RBRACKET   0x30
#define HID_KEY_BACKSLASH  0x31
#define HID_KEY_SEMICOLON  0x33
#define HID_KEY_APOSTROPHE 0x34
#define HID_KEY_GRAVE      0x35
#define HID_KEY_COMMA      0x36
#define HID_KEY_PERIOD     0x37
#define HID_KEY_SLASH      0x38
#define HID_KEY_CAPSLOCK   0x39

// F 功能键
#define HID_KEY_F1     0x3A
#define HID_KEY_F2     0x3B
#define HID_KEY_F3     0x3C
#define HID_KEY_F4     0x3D
#define HID_KEY_F5     0x3E
#define HID_KEY_F6     0x3F
#define HID_KEY_F7     0x40
#define HID_KEY_F8     0x41
#define HID_KEY_F9     0x42
#define HID_KEY_F10    0x43
#define HID_KEY_F11    0x44
#define HID_KEY_F12    0x45

// 控制键
#define HID_KEY_INSERT       0x49
#define HID_KEY_HOME         0x4A
#define HID_KEY_PAGEUP       0x4B
#define HID_KEY_DELETE       0x4C
#define HID_KEY_END          0x4D
#define HID_KEY_PAGEDOWN     0x4E
#define HID_KEY_RIGHT_ARROW  0x4F
#define HID_KEY_LEFT_ARROW   0x50
#define HID_KEY_DOWN_ARROW   0x51
#define HID_KEY_UP_ARROW     0x52
#define HID_KEY_NUMLOCK      0x53

// 小键盘键
#define HID_KEY_NUMPAD_DIV   0x54
#define HID_KEY_NUMPAD_MUL   0x55
#define HID_KEY_NUMPAD_SUB   0x56
#define HID_KEY_NUMPAD_ADD   0x57
#define HID_KEY_NUMPAD_ENTER 0x58
#define HID_KEY_NUMPAD_1     0x59
#define HID_KEY_NUMPAD_2     0x5A
#define HID_KEY_NUMPAD_3     0x5B
#define HID_KEY_NUMPAD_4     0x5C
#define HID_KEY_NUMPAD_5     0x5D
#define HID_KEY_NUMPAD_6     0x5E
#define HID_KEY_NUMPAD_7     0x5F
#define HID_KEY_NUMPAD_8     0x60
#define HID_KEY_NUMPAD_9     0x61
#define HID_KEY_NUMPAD_0     0x62
#define HID_KEY_NUMPAD_DOT   0x63

// 修饰键 (用于 press/release，不放在普通键报告中)
#define HID_MOD_LEFT_CTRL    0x01
#define HID_MOD_LEFT_SHIFT   0x02
#define HID_MOD_LEFT_ALT     0x04
#define HID_MOD_LEFT_GUI     0x08
#define HID_MOD_RIGHT_CTRL   0x10
#define HID_MOD_RIGHT_SHIFT  0x20
#define HID_MOD_RIGHT_ALT    0x40
#define HID_MOD_RIGHT_GUI    0x80

// ========== LED 配置 ==========
#define LED_D4              12     // GPIO12 - 按键指示灯
#define LED_D5              13     // GPIO13 - 状态指示灯
#define LED_BLINK_FAST_MS   200    // 快闪间隔（开机初始化）
#define LED_BLINK_SLOW_MS   1000   // 慢闪间隔（自动模式运行中）
#define LED_BLINK_ADVERT_MS 500    // 广播中闪烁间隔
#define LED_KEY_FLASH_MS    50     // 按键闪烁持续时间

// ========== NTP 时间同步配置 ==========
#define NTP_SERVER "ntp.aliyun.com"
#define GMT_OFFSET_SEC 28800    // UTC+8 (中国)
#define DAYLIGHT_OFFSET_SEC 0

// ========== 自动模式默认配置 ==========
#define DEFAULT_MIN_INTERVAL_MS   800    // 最小按键间隔
#define DEFAULT_MAX_INTERVAL_MS   4000   // 最大按键间隔
#define DEFAULT_MIN_HOLD_MS       80     // 最小按键持续时间
#define DEFAULT_MAX_HOLD_MS       600    // 最大按键持续时间

// 权重默认值 (总和不必为1，会自动归一化)
#define DEFAULT_WEIGHT_FORWARD    0.30   // W 前进
#define DEFAULT_WEIGHT_BACK       0.08   // S 后退
#define DEFAULT_WEIGHT_LEFT       0.12   // A 左移
#define DEFAULT_WEIGHT_RIGHT      0.12   // D 右移
#define DEFAULT_WEIGHT_TURN_LEFT  0.10   // 左转
#define DEFAULT_WEIGHT_TURN_RIGHT 0.10   // 右转
#define DEFAULT_WEIGHT_JUMP       0.08   // 空格跳跃
#define DEFAULT_WEIGHT_C          0.05   // C 键（蹲下）
#define DEFAULT_WEIGHT_Z          0.05   // Z 键（趴下）
#define DEFAULT_WEIGHT_IDLE       0.08   // 空闲

#endif // CONFIG_H