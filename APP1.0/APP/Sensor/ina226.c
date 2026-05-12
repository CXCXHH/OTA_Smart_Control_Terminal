/**
  * @brief  INA226 电压/电流/功率监测芯片驱动 (软件 I2C)
  * @note   配置: 16次均值, 1.1ms转换时间, ~35ms/周期
  *         校准寄存器 5120, 电流 LSB=0.1mA, 电压 LSB=1.25mV
  *         总线地址 0x82 (即 0x41 << 1)
  */
#include "ina226.h"
#include "bsp_iic.h"
#include "delay.h"

static uint8_t INA226_LastError;

uint8_t INA226_GetLastError(void)
{
    return INA226_LastError;
}

uint8_t INA226_WriteReg(uint8_t reg, uint16_t value)
{
    uint8_t buf[2];

    buf[0] = value >> 8;
    buf[1] = value & 0xFF;
    if (IIC_MemWrite(INA226_ADDR, reg, buf, 2))
    {
        INA226_LastError = IIC_GetLastError();
        return 1;
    }
    INA226_LastError = 0;
    return 0;
}

static uint16_t INA226_ReadReg(uint8_t reg)
{
    uint8_t buf[2];
    uint8_t retry;

    for (retry = 0; retry < 3; retry++)
    {
        if (IIC_WriteBuf(INA226_ADDR, &reg, 1))
        {
            INA226_LastError = IIC_GetLastError();
            continue;
        }
        if (IIC_ReadBuf(INA226_ADDR, buf, 2))
        {
            INA226_LastError = IIC_GetLastError();
            continue;
        }

        INA226_LastError = 0;
        return (uint16_t)buf[0] << 8 | buf[1];
    }

    return 0;
}

uint8_t INA226_Init(void)
{
    if (INA226_WriteReg(CONFIG_REGISTER, CONFIG_REGISTER_INIT))
        return 1;
    Delay_ms(5);
    if (INA226_WriteReg(CALIBRATION_REGISTER, CALIBRATION_REGISTER_INIT))
        return 2;
    Delay_ms(5);
    if (INA226_Read_Calibration() != CALIBRATION_REGISTER_INIT)
        return 3;
    return 0;
}

uint16_t INA226_Read_BusVoltage(void)
{
    return INA226_ReadReg(BUS_VOLTAGE_REGISTER);
}

uint16_t INA226_Read_Current(void)
{
    return INA226_ReadReg(CURRENT_REGISTER);
}

uint16_t INA226_Read_Power(void)
{
    return INA226_ReadReg(POWER_REGISTER);
}

uint16_t INA226_Read_Config(void)
{
    return INA226_ReadReg(CONFIG_REGISTER);
}

uint16_t INA226_Read_Calibration(void)
{
    return INA226_ReadReg(CALIBRATION_REGISTER);
}

uint16_t INA226_Read_ManufacturerID(void)
{
    return INA226_ReadReg(0xFE);
}

uint16_t INA226_Read_DieID(void)
{
    return INA226_ReadReg(0xFF);
}
