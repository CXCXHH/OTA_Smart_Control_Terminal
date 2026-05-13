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
  *         [80-83] ready_magic   (uint32, 0x4F544132)
  *         [84-87] target_block  (uint32)
  *         [88-91] fw_crc32      (uint32)
  *         [92-95] fw_size       (uint32)
  *
  *         注: OTA 下载实现在 mqtt.c 中 (MQTT_Parse_OTAData / MQTT_OTA_GetFW / MQTT_OTA_Process)
  */
#include "ota.h"
#include "bsp_AT24C02.h"
#include "bsp.h"

/* EEPROM 偏移 (与 mqtt.c OTA_WriteMetadata 一致) */
#define OTA_EE_EXT_ADDR     80U

/* 从扩展区域读取 fw_crc32 + fw_size 的偏移 */
#define OTA_EE_CRC_OFF      (OTA_EE_EXT_ADDR + 8U)   /* offset 88 */

void OTA_CheckAndRollback(void)
{
    uint8_t  page[16];
    uint32_t ota_flag, expected_crc, crc;

    /* 读 OTA 标志 (uint32 at offset 0) */
    AT24C02_ReadData(0, (uint8_t *)&ota_flag, 4);

    if (ota_flag == 0) {
        U1_printf("OTA none\r\n");
        return;
    }

    /* 读取期望的 CRC32 (APP 侧元数据 offset 88) */
    AT24C02_ReadData(OTA_EE_CRC_OFF, (uint8_t *)&expected_crc, 4);

    /* 启用硬件 CRC 并计算 APP 区域 */
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_CRC, ENABLE);
    CRC_ResetDR();

    uint32_t *p = (uint32_t *)0x08005000UL;
    uint32_t words = 0x0000B000UL / 4UL;
    for (uint32_t i = 0; i < words; i++)
        CRC->DR = p[i];

    crc = CRC_GetCRC();

    /* 准备 16 字节 page buffer (WritePage 始终写满 16 字节) */
    memset(page, 0, sizeof(page));

    if (crc == expected_crc) {
        U1_printf("CRC OK\r\n");
        /* 清除 OTA_FLAG，下次不复检 */
        /* page already zeroed by memset above */
        AT24C02_WritePage(0, page);
        Delay_ms(5);
    } else {
        U1_printf("CRC FAIL\r\n");
        /* 设置回退标记 */
        ota_flag = 0xDDEEFF00UL;
        memcpy(page, &ota_flag, 4);
        AT24C02_WritePage(0, page);
        Delay_ms(5);
    }
}
