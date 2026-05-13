/**
  * @brief  应用层入口
  * @note   板级初始化 → OLED 显示版本 → Modbus 从机初始化
  *         → 创建 FreeRTOS 任务(传感器/Modbus/CANopen/MQTT) → 启动调度器
  */
#include "stm32f10x.h"
#include "bsp.h"
#include "FreeRTOS.h"
#include "task.h"
#include "modbus_app.h"
#include "app_tasks.h"
#include "oled.h"
#include "wifi4g.h"

int main(void)
{
    Bsp_Init();

    OLED_Init();
    OLED_Clear();
    OLED_ShowStr(0, 0, "OTA Version v1", 1);

    FIFO_Init(&UART3_FIFO);

    Modbus_Init(1);

    App_Tasks_Init();

    vTaskStartScheduler();

    while (1) { }
}
