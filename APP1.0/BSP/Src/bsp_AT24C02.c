/**
  * @brief  AT24C02 EEPROM 驱动 (I2C)
  * @note   256 字节容量, 页面大小 16 字节
  *         写周期 < 5ms
  *         用于存储 OTA 升级标志和固件版本信息
  */
#include "bsp.h"
#include "bsp_AT24C02.h"

uint8_t AT24C02_WriteByte(uint8_t addr, uint8_t wdata)
{
    return IIC_MemWrite(AT24C02_WADDR, addr, &wdata, 1);
}

uint8_t AT24C02_WritePage(uint8_t addr, uint8_t *wdata)
{
    return IIC_MemWrite(AT24C02_WADDR, addr, wdata, 16);
}

uint8_t AT24C02_ReadData(uint8_t addr, uint8_t *rdata, uint16_t datalen)
{
    if(datalen == 0) return 0;

    return IIC_MemRead(AT24C02_WADDR, addr, rdata, datalen);
}
