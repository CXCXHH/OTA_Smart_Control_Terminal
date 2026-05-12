/**
  * @brief  板级支持包：初始化全部外设
  * @note   初始化顺序依赖:
  *         Delay → USART1(日志) → I2C(传感器/EEPROM) → SPI(W25Q64)
  *         → GPIO/LED/继电器 → ADC → USART2(Modbus) → USART3(MQTT AT)
  *         → TIM2(Canfestival) → TIM3(FreeModbus) → CAN1(CANopen)
  */
#include "bsp.h"

void Bsp_Init(void)
{
    Delay_Init();
    Usart1_Init(115200);
    IIC_Init();
    SPI1_Init();
    W25Q64_Init();

    GPIO_Init_Outputs();
    ADC1_Init();
    Usart2_Init(115200);
    Usart3_Init(115200);
    TIM2_Init();
    TIM3_Init();
    CAN1_Init();
}

