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

