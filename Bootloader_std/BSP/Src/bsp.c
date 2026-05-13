#include "bsp.h"

void Bsp_Init(void)
{
	Usart1_Init(115200);
    IIC_Init();
	OLED_Boot_Init();
	SPI1_Init();
	W25Q64_Init();
}

