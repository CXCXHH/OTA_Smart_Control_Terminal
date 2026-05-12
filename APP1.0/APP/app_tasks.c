/**
  * @brief  FreeRTOS 任务定义
  * @note   创建四个独立任务：Modbus(高优先级)、传感器、CANopen、MQTT
  *         Modbus 优先级最高(10ms周期)，保证实时响应；
  *         MQTT 栈最大(384字)，因涉及 AT 命令缓冲和 JSON 解析
  */
#include "FreeRTOS.h"
#include "task.h"
#include "modbus_app.h"
#include "sensor_app.h"
#include "canopen_app.h"
#include "bsp.h"
#include "mqtt.h"
#include "wifi4g.h"
#include "oled.h"

#define MODBUS_TASK_PRIO      (tskIDLE_PRIORITY + 2)
#define SENSOR_TASK_PRIO      (tskIDLE_PRIORITY + 1)
#define CANOPEN_TASK_PRIO     (tskIDLE_PRIORITY + 1)
#define MQTT_TASK_PRIO        (tskIDLE_PRIORITY + 1)

#define MODBUS_TASK_STACK     256
#define SENSOR_TASK_STACK     256
#define CANOPEN_TASK_STACK    256
#define MQTT_TASK_STACK       384

/**
  * @brief  传感器数据采集任务 (500ms周期)
  *         读取 AHT20(温湿度) + INA226(电压/电流/功率) + MCU内部ADC，
  *         写入 REG_HOLD_BUF 共享给 Modbus/CANopen；每2次更新OLED显示
  */
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

/**
  * @brief  Modbus RTU 从机任务 (10ms周期)
  *         eMBPoll() 处理协议帧收发，Modbus_Parse() 刷新物理输出
  */
static void ModbusTask(void *pvParameters)
{
    (void)pvParameters;
    for (;;)
    {
        Modbus_Task();  /* eMBPoll + Modbus_Parse */
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
  * @brief  MQTT + WiFi 任务 (5s周期)
  *         连接 WiFi → 连接 MQTT 服务器 → 订阅下行Topic → 周期性上报传感器数据
  *         掉线时自动重连，收到下行JSON时解析并控制输出
  */
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

/**
  * @brief  CANopen 从站任务 (100ms周期)
  *         初始化 Canfestival 协议栈(节点ID=1, 预Operational)，
  *         周期发送心跳报文(COB-ID 0x701)
  */
static void CANopenTask(void *pvParameters)
{
    (void)pvParameters;
    Canopen_Init();
    U1_printf("CANopen task start\r\n");
    for (;;)
    {
        Canopen_Process();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/**
  * @brief  创建所有应用任务
  * @note   必须在 vTaskStartScheduler() 前调用，
  *         任务栈空间分配：Modbus/Sensor/CANopen=256字, MQTT=384字
  */
void App_Tasks_Init(void)
{
    xTaskCreate(ModbusTask, "Modbus", MODBUS_TASK_STACK, NULL,
                MODBUS_TASK_PRIO, NULL);
    xTaskCreate(SensorTask, "Sensor", SENSOR_TASK_STACK, NULL,
                SENSOR_TASK_PRIO, NULL);
    xTaskCreate(CANopenTask, "CANopen", CANOPEN_TASK_STACK, NULL,
                CANOPEN_TASK_PRIO, NULL);
    xTaskCreate(MQTTTask, "MQTT", MQTT_TASK_STACK, NULL,
                MQTT_TASK_PRIO, NULL);
}
