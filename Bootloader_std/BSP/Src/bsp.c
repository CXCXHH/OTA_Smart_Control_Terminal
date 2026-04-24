#include "bsp.h"

void Bsp_Init(void)
{
    Delay_Init();
	Usart1_Init(921600);
    IIC_Init();
}

