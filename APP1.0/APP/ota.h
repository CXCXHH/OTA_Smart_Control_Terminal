/**
  * @brief  OTA 升级校验与回退 (APP 侧启动检查)
  * @note   Bootloader 将固件写入 APP 分区后置 OTA 元数据，
  *         本函数在 APP 首次运行时校验 CRC32：
  *         - CRC 正确 → 清除标记，正常启动
  *         - CRC 错误 → 置回退标记，下次 Bootloader 回滚旧版本
  *         EEPROM 布局与 mqtt.c 中的 OTA_WriteMetadata 一致:
  *         [0-3]   ota_flag      (uint32)
  *         [4-47]  firelen[11]   (uint32[11])
  *         [48-79] ota_ver[32]   (uint8_t[32])
  *         [80-83] ready_magic   (uint32, 0x4F544132 = "OTA2")
  *         [84-87] target_block  (uint32)
  *         [88-91] fw_crc32      (uint32)
  *         [92-95] fw_size       (uint32)
  */
#ifndef OTA_H
#define OTA_H

#include <stdint.h>

/* OTA 下载函数在 mqtt.c 中实现 (MQTT_Parse_OTAData / MQTT_OTA_GetFW / MQTT_OTA_Process) */
/* 本文件只提供启动时 CRC 检查与回退 */

/* 启动时 CRC 检查与回退 */
void OTA_CheckAndRollback(void);

#endif /* OTA_H */
