#include "bsp.h"

void Bsp_Init(void)
{
    Delay_Init();
	Usart1_Init(115200);
    IIC_Init();
	SPI1_Init();
}

