/**
  * @brief  传感器应用层：周期性采集并分发数据
  * @note   包含 AHT20(温湿度) + INA226(电流/电压/功率) + MCU内部ADC，
  *         数据写入 REG_HOLD_BUF 经 Modbus/CANopen/MQTT 三协议共享
  */
#include "sensor_app.h"
#include "aht20.h"
#include "ina226.h"
#include "bsp_adc.h"
#include "bsp_iic.h"
#include "bsp.h"
#include "modbus_app.h"
#include "oled.h"
#include <stdio.h>

static uint8_t print_cnt;

static void I2C_ScanOnce(void)
{
    uint8_t ret;

    ret = IIC_CheckDevice(0x70);
    U1_printf("I2C check AHT20(38): %s err=%02X\r\n",
              ret == 0 ? "OK" : "FAIL", IIC_GetLastError());

    ret = IIC_CheckDevice(INA226_ADDR);
    U1_printf("I2C check INA226(41): %s err=%02X\r\n",
              ret == 0 ? "OK" : "FAIL", IIC_GetLastError());
}

void SensorApp_Init(void)
{
    uint8_t aht_ret, ina_ret;

    I2C_ScanOnce();
    aht_ret = AHT20_Init();
    U1_printf("AHT20 init ret=%d i2c_err=%02X\r\n", aht_ret, IIC_GetLastError());
    ina_ret = INA226_Init();
    U1_printf("INA226 init ret=%d ina_err=%02X i2c_err=%02X\r\n",
              ina_ret, INA226_GetLastError(), IIC_GetLastError());
    /* small delay for sensor power-on settle before config reads */
    for (volatile uint32_t d = 0; d < 500000; d++);
    U1_printf("INA226 CFG=%04X CAL=%04X MID=%04X DID=%04X err=%02X\r\n",
              INA226_Read_Config(), INA226_Read_Calibration(),
              INA226_Read_ManufacturerID(), INA226_Read_DieID(),
              IIC_GetLastError());
    U1_printf("Sensors OK\r\n");
}

/**
  * @brief  周期性传感器采集 (500ms)
  *         REG_HOLD_BUF 映射:
  *           [1]=温度*100, [2]=湿度*100, [3]=电压*100,
  *           [4]=电流mA, [5]=功率mW, [6]=MCU电压*100, [7]=MCU温度*100
  *         每2次(约1s)更新一次 OLED 和串口日志
  */
void SensorApp_Process(void)
{
    uint16_t temp, rh;
    uint16_t ina_bus, ina_current, ina_power;
    uint8_t aht_ret;
    uint16_t v3, v4, v5;       /* local snapshots for display */
    uint16_t adc_voltage, adc_temp;

    aht_ret = AHT20_Read(&temp, &rh);

    ina_bus = INA226_Read_BusVoltage();
    ina_current = INA226_Read_Current();
    ina_power = INA226_Read_Power();
    adc_voltage = ADC_GetVoltage();
    adc_temp = ADC_GetTemperature();

    REG_Lock();
    if (aht_ret == 0)
    {
        REG_HOLD_BUF[1] = temp;
        REG_HOLD_BUF[2] = rh;
    }
    REG_HOLD_BUF[3] = ina_bus / 8;
    REG_HOLD_BUF[4] = ina_current / 10;
    REG_HOLD_BUF[5] = (uint16_t)((ina_power * 5UL) / 2UL);
    REG_HOLD_BUF[6] = adc_voltage;
    REG_HOLD_BUF[7] = adc_temp;
    /* snapshot for non-critical display (avoid long lock for OLED/printf) */
    v3 = REG_HOLD_BUF[3];
    v4 = REG_HOLD_BUF[4];
    v5 = REG_HOLD_BUF[5];
    REG_Unlock();

    if (++print_cnt >= 4)
    {
        print_cnt = 0;

        if (aht_ret == 0)
        {
            U1_printf("AHT20 T=%d.%02dC RH=%d.%02d%%\r\n",
                      temp / 100, temp % 100, rh / 100, rh % 100);
            char line[22];
            int len = snprintf(line, sizeof(line), "T=%.1fC RH=%.1f%%",
                               temp / 100.0f, rh / 100.0f);
            while (len < 21) line[len++] = ' ';
            line[21] = '\0';
            OLED_ShowStr(0, 0, line, 0);
        }
        else
        {
            U1_printf("AHT20 read err=%d\r\n", aht_ret);
            U1_printf("I2C err=%02X\r\n", IIC_GetLastError());
        }

        U1_printf("INA226 BUS=%d.%02dV CUR=%dmA PWR=%dmW raw=%04X/%04X/%04X err=%02X\r\n",
                  v3 / 100, v3 % 100, v4, v5,
                  ina_bus, ina_current, ina_power, IIC_GetLastError());

        {
            char line[22];
            int len;
            len = snprintf(line, sizeof(line), "V=%d.%02dV I=%dmA",
                           v3 / 100, v3 % 100, v4);
            while (len < 21) line[len++] = ' ';
            line[21] = '\0';
            OLED_ShowStr(0, 2, line, 0);

            len = snprintf(line, sizeof(line), "P=%dmW", v5);
            while (len < 21) line[len++] = ' ';
            line[21] = '\0';
            OLED_ShowStr(0, 4, line, 0);

            len = snprintf(line, sizeof(line), "M+C+M v1.0");
            while (len < 21) line[len++] = ' ';
            line[21] = '\0';
            OLED_ShowStr(0, 6, line, 0);
        }
    }
}
