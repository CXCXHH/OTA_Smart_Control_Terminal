#include "stm32f10x.h"                  
#include "bsp.h"

uint8_t rbuff[256];
uint8_t wbuff[16] = {15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0};

int main(void)
{
	uint16_t i;

	Bsp_Init();

	AT24C02_WritePage(0, wbuff);
	Delay_ms(5);

	AT24C02_ReadData(0, rbuff, 16);

	for(i = 0;i < 16;i++)
	{
		U1_printf("adrss%d=%x\r\n", i, rbuff[i]);
	}

	while(1)
	{
		
	}
}
