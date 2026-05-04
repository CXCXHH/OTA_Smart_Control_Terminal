#include "bsp.h"
#include "bsp_AT24C02.h"

uint8_t AT24C02_WriteByte(uint8_t addr, uint8_t wdata)
{
    IIC_Start();
    IIC_Send_Byte(AT24C02_WADDR);
    if(IIC_wait_Ack(100) != 0) return 1;
    IIC_Send_Byte(addr);
    if(IIC_wait_Ack(100) != 0) return 2;
    IIC_Send_Byte(wdata);
    if(IIC_wait_Ack(100) != 0) return 3;
    IIC_Stop();
    return 0;
}

uint8_t AT24C02_WritePage(uint8_t addr, uint8_t *wdata)
{
    uint8_t i;
    IIC_Start();
    IIC_Send_Byte(AT24C02_WADDR);
    if(IIC_wait_Ack(100) != 0) return 1;
    IIC_Send_Byte(addr);
    if(IIC_wait_Ack(100) != 0) return 2;
    for(i = 0;i < 16;i++)
    {
        IIC_Send_Byte(wdata[i]);
        if(IIC_wait_Ack(100) != 0) return 3+i;
    }
    IIC_Stop();
    return 0;
}

uint8_t AT24C02_ReadData(uint8_t addr, uint8_t *rdata, uint16_t datalen)
{
    uint16_t i;
    if(datalen == 0) return 0;

    IIC_Start();
    IIC_Send_Byte(AT24C02_WADDR);
    if(IIC_wait_Ack(100) != 0) return 1;
    IIC_Send_Byte(addr);
    if(IIC_wait_Ack(100) != 0) return 2;
    IIC_Start();
    IIC_Send_Byte(AT24C02_RADDR);
    if(IIC_wait_Ack(100) != 0) return 1;
    for(i = 0;i < datalen-1;i++)
    {
        rdata[i] = IIC_Read_Byte(1);
    }
    rdata[datalen-1] = IIC_Read_Byte(0);
    IIC_Stop();
    return 0;
}

void M24C02_ReadOTAInfo(void)
{
    memset(&OTA_Info, 0, OTA_INFOCB_SIZE);
    AT24C02_ReadData(0, (uint8_t *)&OTA_Info, OTA_INFOCB_SIZE);
}
