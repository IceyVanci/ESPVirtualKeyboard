#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include "auto_mode.h"
#include "seq_mode.h"

#define SLOT_COUNT 5
#define SLOT_NAME_MAX_LEN 20

struct SlotSummary {
  int index;
  bool used;
  String name;
};

class ConfigManager {
public:
  ConfigManager();

  void begin();  // 初始化 Preferences

  // 槽位操作
  bool saveSlot(int slotIndex, const String& name, const AutoModeConfig& config);
  bool loadSlot(int slotIndex, AutoModeConfig& config, String& name);
  bool overwriteSlot(int slotIndex, const String& name, const AutoModeConfig& config);
  bool deleteSlot(int slotIndex);
  bool isSlotUsed(int slotIndex);
  String getSlotName(int slotIndex);

  // 批量查询
  SlotSummary getSlotSummary(int slotIndex);

  // 活动槽位管理（启动时自动加载）
  void setActiveSlot(int slotIndex);   // -1 = 默认配置
  int  getActiveSlot();
  bool loadActiveConfig(AutoModeConfig& config);  // 加载活动配置，返回 false 表示用默认

  // JSON 导入导出
  String exportConfig(const AutoModeConfig& config, const String& name);
  String exportSlot(int slotIndex);
  bool importConfig(const String& json, AutoModeConfig& config, String& name);
  bool importToSlot(int slotIndex, const String& json);

  // 当前配置导出/导入便捷方法
  String exportCurrentConfig(const AutoModeConfig& config);
  bool importToCurrentConfig(const String& json, AutoModeConfig& config);

  // 认证凭据（NVS 持久化，默认值来自 config.h）
  String getAuthUser();
  String getAuthPass();                      // 返回密码 SHA-256 哈希
  bool hasAuthCredentials();
  bool setAuthCredentials(const String& user, const String& pass);
  static String sha256Hex(const String& input);  // SHA-256 十六进制串

  // 蓝牙设备名称（NVS 持久化，默认值来自 config.h）
  String getBleName();
  bool setBleName(const String& name);       // 1~24 字符，返回是否成功

  // 顺序模式槽位（5 个独立栏位，与自动模式槽位互不影响）
  bool saveSeqSlot(int slotIndex, const String& name, const SeqConfig& config);
  bool loadSeqSlot(int slotIndex, SeqConfig& config, String& name);
  bool deleteSeqSlot(int slotIndex);
  bool isSeqSlotUsed(int slotIndex);
  String getSeqSlotName(int slotIndex);
  void setActiveSeqSlot(int slotIndex);      // -1 = 默认
  int  getActiveSeqSlot();
  bool loadActiveSeqConfig(SeqConfig& config);
  String seqConfigToJson(const SeqConfig& config, const String& name);
  bool seqJsonToConfig(const String& json, SeqConfig& config, String& name);

  // 全部导出：5 自动 + 5 顺序槽位汇总为单个 JSON
  String exportAllConfigs();

private:
  Preferences _prefs;

  String slotNameKey(int slotIndex);
  String slotDataKey(int slotIndex);
  String slotUsedKey(int slotIndex);

  // JSON 简易生成（避免依赖 ArduinoJson）
  String configToJson(const AutoModeConfig& config, const String& name);
  bool jsonToConfig(const String& json, AutoModeConfig& config, String& name);
};

#endif // CONFIG_MANAGER_H