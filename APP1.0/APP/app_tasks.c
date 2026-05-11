#include "FreeRTOS.h"
#include "task.h"
#include "modbus_app.h"
#include "sensor_app.h"
#include "bsp.h"
#include "mqtt.h"
#include "wifi4g.h"
#include "oled.h"

#define MODBUS_TASK_PRIO      (tskIDLE_PRIORITY + 2)
#define SENSOR_TASK_PRIO      (tskIDLE_PRIORITY + 1)
#define MQTT_TASK_PRIO        (tskIDLE_PRIORITY + 1)

#define MODBUS_TASK_STACK     256
#define SENSOR_TASK_STACK     256
#define MQTT_TASK_STACK       384

static void SensorTask(void *pvParameters)
{
    (void)pvParameters;
    SensorApp_Init();
    OLED_Clear();
    for (;;)
    {
        SensorApp_Process();
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
