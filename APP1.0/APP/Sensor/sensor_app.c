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

void SensorApp_Process(void)
{
    uint16_t temp, rh;
    uint16_t ina_bus, ina_current, ina_power;
    uint8_t aht_ret;

    aht_ret = AHT20_Read(&temp, &rh);
    if (aht_ret == 0)
    {
        REG_HOLD_BUF[1] = temp;
        REG_HOLD_BUF[2] = rh;
    }

    ina_bus = INA226_Read_BusVoltage();
    ina_current = INA226_Read_Current();
    ina_power = INA226_Read_Power();
    REG_HOLD_BUF[3] = ina_bus / 8;
    REG_HOLD_BUF[4] = ina_current / 10;
    REG_HOLD_BUF[5] = (uint16_t)((ina_power * 5UL) / 2UL);

    REG_HOLD_BUF[6] = ADC_GetVoltage();
    REG_HOLD_BUF[7] = ADC_GetTemperature();

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
                  REG_HOLD_BUF[3] / 100, REG_HOLD_BUF[3] % 100,
                  REG_HOLD_BUF[4], REG_HOLD_BUF[5],
                  ina_bus, ina_current, ina_power, IIC_GetLastError());

        {
            char line[22];
            int len;
            len = snprintf(line, sizeof(line), "V=%d.%02dV I=%dmA",
                           REG_HOLD_BUF[3] / 100, REG_HOLD_BUF[3] % 100,
                           REG_HOLD_BUF[4]);
            while (len < 21) line[len++] = ' ';
            line[21] = '\0';
            OLED_ShowStr(0, 2, line, 0);

            len = snprintf(line, sizeof(line), "P=%dmW", REG_HOLD_BUF[5]);
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
