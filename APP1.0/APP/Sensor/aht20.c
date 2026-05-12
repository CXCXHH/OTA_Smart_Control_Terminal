/**
  * @brief  AHT20 温湿度传感器驱动 (软件 I2C)
  * @note   使用 IIC_WriteBuf/IIC_ReadBuf 实现寄存器读写
  *         AHT20 地址 0x38 (左移1位=0x70)
  *         温度返回单位: 度*100, 湿度返回单位: %RH*100
  */
#include "aht20.h"
#include "bsp_iic.h"
#include "bsp.h"
#include "delay.h"

static uint8_t AHT20_Read_Status(uint8_t *status)
{
    if (IIC_ReadBuf(AHT20_SLAVE_ADDRESS, status, 1))
        return 1;
    return 0;
}

static uint8_t AHT20_TrigMeasureCmd(void)
{
    uint8_t cmd[3] = {0xAC, 0x33, 0x00};

    return IIC_WriteBuf(AHT20_SLAVE_ADDRESS, cmd, 3);
}

uint8_t AHT20_Init(void)
{
    uint8_t status;

    Delay_ms(40);

    uint8_t cmd[3] = {0xBE, 0x08, 0x00};

    if (IIC_WriteBuf(AHT20_SLAVE_ADDRESS, cmd, 3))
        return 1;
    Delay_ms(10);

    if (AHT20_Read_Status(&status))
        return 2;

    if (!(status & 0x08))
    {
        /* 重新初始化 */
        if (IIC_WriteBuf(AHT20_SLAVE_ADDRESS, cmd, 3))
            return 1;
        Delay_ms(10);
    }

    return 0;
}

uint8_t AHT20_Read(uint16_t *temp_centi, uint16_t *rh_centi)
{
    uint8_t buf[6];
    uint8_t cnt = 0;
    uint8_t status;
    uint32_t raw;
    int32_t temp_calc;

    if (AHT20_TrigMeasureCmd())
        return 1;

    Delay_ms(80);

    do
    {
        if (AHT20_Read_Status(&status))
            return 2;
        if ((status & 0x80) == 0)
            break;
        Delay_ms(1);
    } while (++cnt < 100);

    if (status & 0x80)
        return 3;

    if (IIC_ReadBuf(AHT20_SLAVE_ADDRESS, buf, 6))
        return 4;

    raw = ((uint32_t)buf[1] << 12) | ((uint32_t)buf[2] << 4) | ((uint32_t)buf[3] >> 4);
    *rh_centi = (uint16_t)((raw * 625UL) >> 16);      /* %RH * 100 */

    raw = (((uint32_t)buf[3] & 0x0F) << 16) | ((uint32_t)buf[4] << 8) | buf[5];
    temp_calc = (int32_t)((raw * 625UL) >> 15) - 5000; /* degC * 100 */
    if (temp_calc < 0)
        temp_calc = 0;
    *temp_centi = (uint16_t)temp_calc;
    return 0;
}
