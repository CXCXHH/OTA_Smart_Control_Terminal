#include "stm32f10x.h"                  
#include "bsp.h"

uint8_t rbuff[256];
uint8_t wbuff[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};

int main(void)
{
	uint16_t i;

	Bsp_Init();

	for(i = 0;i < 8;i++)
	{
		AT24C02_WriteByte(i, 7-i);
		Delay_ms(5);
	}

	// for(i = 0;i < 16;i++)
	// {
	// 	AT24C02_WritePage(i*16, wbuff);
	// 	Delay_ms(5);
	// }

	AT24C02_ReadData(0, rbuff, 256);

	for(i = 0;i < 8;i++)
	{
		U1_printf("adrss%d=%x\r\n", i, rbuff[i]);
	}

	while(1)
	{
		
	}
}
