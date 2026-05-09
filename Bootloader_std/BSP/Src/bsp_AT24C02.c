#include "bsp.h"
#include "bsp_AT24C02.h"

uint8_t AT24C02_WriteByte(uint8_t addr, uint8_t wdata)
{
    uint8_t ret = 0;

    IIC_Start();
    IIC_Send_Byte(AT24C02_WADDR);
    if(IIC_wait_Ack(100) != 0) { ret = 1; goto out; }
    IIC_Send_Byte(addr);
    if(IIC_wait_Ack(100) != 0) { ret = 2; goto out; }
    IIC_Send_Byte(wdata);
    if(IIC_wait_Ack(100) != 0) { ret = 3; goto out; }

out:
    IIC_Stop();
    return ret;
}

uint8_t AT24C02_WritePage(uint8_t addr, uint8_t *wdata)
{
    uint8_t i;
    uint8_t ret = 0;

    IIC_Start();
    IIC_Send_Byte(AT24C02_WADDR);
    if(IIC_wait_Ack(100) != 0) { ret = 1; goto out; }
    IIC_Send_Byte(addr);
    if(IIC_wait_Ack(100) != 0) { ret = 2; goto out; }
    for(i = 0;i < 16;i++)
    {
        IIC_Send_Byte(wdata[i]);
        if(IIC_wait_Ack(100) != 0) { ret = 3+i; goto out; }
    }

out:
    IIC_Stop();
    return ret;
}

uint8_t AT24C02_ReadData(uint8_t addr, uint8_t *rdata, uint16_t datalen)
{
    uint16_t i;
    uint8_t ret = 0;

    if(datalen == 0) return 0;

    IIC_Start();
    IIC_Send_Byte(AT24C02_WADDR);
    if(IIC_wait_Ack(100) != 0) { ret = 1; goto out; }
    IIC_Send_Byte(addr);
    if(IIC_wait_Ack(100) != 0) { ret = 2; goto out; }
    IIC_Start();
    IIC_Send_Byte(AT24C02_RADDR);
    if(IIC_wait_Ack(100) != 0) { ret = 3; goto out; }
    for(i = 0;i < datalen-1;i++)
    {
        rdata[i] = IIC_Read_Byte(1);
    }
    rdata[datalen-1] = IIC_Read_Byte(0);

out:
    IIC_Stop();
    return ret;
}

void AT24C02_ReadOTAInfo(void)
{
    memset(&OTA_Info, 0, OTA_INFOCB_SIZE);
    AT24C02_ReadData(0, (uint8_t *)&OTA_Info, OTA_INFOCB_SIZE);
}

void AT24C02_WriteOTAInfo(void)
{
    uint8_t i;
    for(i=0;i<OTA_INFOCB_SIZE/16;i++)
    {
        AT24C02_WritePage(i*16, (uint8_t *)&OTA_Info+i*16);
        Delay_ms(5);  /* AT24C02 内部写周期 tWR max 5ms，写入后需等待 */
    }
}
