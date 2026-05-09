#include "stm32f10x.h"
#include "bsp.h"
#include "FreeRTOS.h"
#include "task.h"
#include "modbus_app.h"
#include "app_tasks.h"
#include "ota.h"
#include "canfestival.h"
#include "TestSlave.h"
#include "oled.h"
#include "wifi4g.h"

int main(void)
{
    Bsp_Init();
    U1_printf("APP start\r\n");

    OLED_Init();
    OLED_Clear();
    OLED_ShowStr(0, 0, "OTA Term v1");
    OLED_ShowStr(0, 2, "I2C+AHT20+INA");
    OLED_ShowStr(0, 4, "M+C+M ready");

    FIFO_Init(&UART3_FIFO);

    OTA_CheckAndRollback();

    Modbus_Init(1);
    U1_printf("Modbus OK\r\n");

    CanFestival_Can_Init();
    setNodeId(&TestSlave_Data, 1);
    setState(&TestSlave_Data, Initialisation);
    setState(&TestSlave_Data, Operational);
    U1_printf("CANopen OK\r\n");

    App_Tasks_Init();

    U1_printf("FreeRTOS start\r\n");
    vTaskStartScheduler();

    while (1) { }
}
