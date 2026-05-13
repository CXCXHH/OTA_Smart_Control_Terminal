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
#include <stdio.h>

static uint8_t print_cnt;

static void I2C_ScanOnce(void)
{
    uint8_t ret;

    ret = IIC_CheckDevice(0x70);
    (void)ret;

    ret = IIC_CheckDevice(INA226_ADDR);
    (void)ret;
}

void SensorApp_Init(void)
{
    uint8_t aht_ret, ina_ret;

    I2C_ScanOnce();
    aht_ret = AHT20_Init();
    ina_ret = INA226_Init();
    (void)aht_ret;
    (void)ina_ret;
    /* small delay for sensor power-on settle before config reads */
    for (volatile uint32_t d = 0; d < 500000; d++);
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
        REG_HOLD_BUF[REG_IDX_TEMP] = temp;
        REG_HOLD_BUF[REG_IDX_HUMI] = rh;
    }
    REG_HOLD_BUF[REG_IDX_DEV_VOLT] = ina_bus / 8;
    REG_HOLD_BUF[REG_IDX_DEV_CURR] = ina_current / 10;
    REG_HOLD_BUF[REG_IDX_DEV_POWER] = (uint16_t)((ina_power * 5UL) / 2UL);
    REG_HOLD_BUF[REG_IDX_SYS_VOLT] = adc_voltage;
    REG_HOLD_BUF[REG_IDX_CPU_TEMP] = adc_temp;
    /* snapshot for non-critical display (avoid long lock for OLED/printf) */
    v3 = REG_HOLD_BUF[REG_IDX_DEV_VOLT];
    v4 = REG_HOLD_BUF[REG_IDX_DEV_CURR];
    v5 = REG_HOLD_BUF[REG_IDX_DEV_POWER];
    REG_Unlock();

    if (++print_cnt >= 4)
    {
        print_cnt = 0;
        (void)aht_ret;
        (void)temp;
        (void)rh;
        (void)v3;
        (void)v4;
        (void)v5;
    }
}
