#include "ota.h"
#include "bsp_AT24C02.h"
#include "bsp.h"

/* AT24C02 OTA 区域布局 (与 Bootloader 兼容):
 *  [0-3]   OTA_FLAG (4B)
 *  [4-47]  Firelen[11] (44B)
 *  [48-79] OTA_ver[32] (32B)
 *  总计 80 字节
 */
#define OTA_FLAG_ADDR  0
#define OTA_CRC_ADDR   80   /* 紧随 Bootloader 的 OTA 结构之后 */

void OTA_CheckAndRollback(void)
{
    uint32_t ota_flag, expected_crc, crc;

    /* 读 OTA 标志 */
    AT24C02_ReadData(OTA_FLAG_ADDR, (uint8_t *)&ota_flag, 4);

    if (ota_flag != OTA_FLAG_MAGIC) {
        U1_printf("OTA none\r\n");
        return;
    }

    /* 验证通过后读取期望的 CRC32 */
    AT24C02_ReadData(OTA_CRC_ADDR, (uint8_t *)&expected_crc, 4);

    /* 启用硬件 CRC 并计算 APP 区域 */
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_CRC, ENABLE);
    CRC_ResetDR();

    uint32_t *p = (uint32_t *)APP_START_ADDR;
    uint32_t words = APP_MAX_SIZE / 4;
    for (uint32_t i = 0; i < words; i++)
        CRC->DR = p[i];

    crc = CRC_GetCRC();

    if (crc == expected_crc) {
        U1_printf("CRC OK\r\n");
        /* 清除 OTA_FLAG，下次不复检 */
        ota_flag = 0;
        AT24C02_WritePage(OTA_FLAG_ADDR, (uint8_t *)&ota_flag);
        Delay_ms(5);
    } else {
        U1_printf("CRC FAIL\r\n");
        /* 设置回退标记 */
        ota_flag = 0xDDEEFF00;
        AT24C02_WritePage(OTA_FLAG_ADDR, (uint8_t *)&ota_flag);
        Delay_ms(5);
    }
}
