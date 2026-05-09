#include "FreeRTOS.h"
#include "task.h"
#include "modbus_app.h"
#include "aht20.h"
#include "ina226.h"
#include "bsp_adc.h"
#include "bsp_gpio.h"
#include "bsp.h"
#include "mqtt.h"
#include "wifi4g.h"
#include "bsp_iic.h"
#include "oled.h"
#include <stdio.h>

#define MODBUS_TASK_PRIO      (tskIDLE_PRIORITY + 2)
#define SENSOR_TASK_PRIO      (tskIDLE_PRIORITY + 1)
#define MQTT_TASK_PRIO        (tskIDLE_PRIORITY + 1)

#define MODBUS_TASK_STACK     256
#define SENSOR_TASK_STACK     256
#define MQTT_TASK_STACK       384

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

static void SensorTask(void *pvParameters)
{
    (void)pvParameters;
    uint16_t temp, rh;
    uint16_t ina_bus;
    uint16_t ina_current;
    uint16_t ina_power;
    uint8_t aht_ret;
    uint8_t ina_ret;
    uint8_t print_cnt = 0;

    I2C_ScanOnce();
    aht_ret = AHT20_Init();
    U1_printf("AHT20 init ret=%d i2c_err=%02X\r\n", aht_ret, IIC_GetLastError());
    ina_ret = INA226_Init();
    U1_printf("INA226 init ret=%d ina_err=%02X i2c_err=%02X\r\n",
              ina_ret, INA226_GetLastError(), IIC_GetLastError());
    vTaskDelay(pdMS_TO_TICKS(10));
    U1_printf("INA226 CFG=%04X CAL=%04X MID=%04X DID=%04X err=%02X\r\n",
              INA226_Read_Config(), INA226_Read_Calibration(),
              INA226_Read_ManufacturerID(), INA226_Read_DieID(),
              IIC_GetLastError());
    U1_printf("Sensors OK\r\n");

    for (;;)
    {
        aht_ret = AHT20_Read(&temp, &rh);
        if (aht_ret == 0)
        {
            REG_HOLD_BUF[1] = temp;           /* HOLD[2] = T*100 */
            REG_HOLD_BUF[2] = rh;             /* HOLD[3] = RH*100 */
        }

        ina_bus = INA226_Read_BusVoltage();
        ina_current = INA226_Read_Current();
        ina_power = INA226_Read_Power();
        REG_HOLD_BUF[3] = ina_bus / 8;       /* HOLD[4] = V*100 */
        REG_HOLD_BUF[4] = ina_current / 10;  /* HOLD[5] = mA */
        REG_HOLD_BUF[5] = (uint16_t)((ina_power * 5UL) / 2UL); /* HOLD[6] */

        REG_HOLD_BUF[6] = ADC_GetVoltage();   /* HOLD[7] */
        REG_HOLD_BUF[7] = ADC_GetTemperature(); /* HOLD[8] */

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
                OLED_ShowStr(0, 0, line);
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
                OLED_ShowStr(0, 2, line);
                len = snprintf(line, sizeof(line), "P=%dmW", REG_HOLD_BUF[5]);
                while (len < 21) line[len++] = ' ';
                line[21] = '\0';
                OLED_ShowStr(0, 4, line);
                len = snprintf(line, sizeof(line), "M+C+M v1.0");
                while (len < 21) line[len++] = ' ';
                line[21] = '\0';
                OLED_ShowStr(0, 6, line);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

static void ModbusTask(void *pvParameters)
{
    (void)pvParameters;
    for (;;)
    {
        Modbus_Task();  /* eMBPoll + Modbus_Parse */
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void MQTTTask(void *pvParameters)
{
    (void)pvParameters;

    FIFO_Init(&UART3_FIFO);
    U1_printf("MQTT task start\r\n");

    if (ESP8266_Connect_WIFI())
        U1_printf("WiFi OK\r\n");
    else
        U1_printf("WiFi FAIL\r\n");

    if (MQTT_Connect_Server())
        U1_printf("MQTT connected\r\n");
    else
        U1_printf("MQTT FAIL\r\n");

    for (;;)
    {
        WIFI4G_Parse_Queue();
        if (!MQTT_Download_Flag)
            MQTT_Connect_Server();
        else
            MQTT_SendData();
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void App_Tasks_Init(void)
{
    xTaskCreate(ModbusTask, "Modbus", MODBUS_TASK_STACK, NULL,
                MODBUS_TASK_PRIO, NULL);
    xTaskCreate(SensorTask, "Sensor", SENSOR_TASK_STACK, NULL,
                SENSOR_TASK_PRIO, NULL);
    xTaskCreate(MQTTTask, "MQTT", MQTT_TASK_STACK, NULL,
                MQTT_TASK_PRIO, NULL);
}
