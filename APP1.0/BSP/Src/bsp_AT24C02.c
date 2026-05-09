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
