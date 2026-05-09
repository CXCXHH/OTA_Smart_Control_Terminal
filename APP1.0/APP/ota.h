#ifndef OTA_H
#define OTA_H

#include <stdint.h>

/* AT24C02 OTA 信息结构 (16字节对齐) */
typedef struct {
    uint32_t ota_flag;      /* 0xAABB1122 = 有升级请求 */
    uint32_t firmware_crc;  /* APP 固件 CRC32 */
    uint32_t firmware_len;  /* APP 固件实际长度 */
    uint32_t reserved;
} OTA_Info_t;

#define OTA_FLAG_MAGIC      0xAABB1122UL
#define APP_START_ADDR      0x08005000UL
#define APP_MAX_SIZE        0x0000B000UL  /* 44KB */

void OTA_CheckAndRollback(void);

#endif
