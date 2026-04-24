#include "stm32f10x.h"                  
#include "bsp.h"

uint8_t rbuff[256];

int main(void)
{
	uint16_t i;

	Bsp_Init();

	for(i = 0;i < 256;i++)
	{
		AT24C02_WriteByte(i, i);
		Delay_ms(5);
	}

	AT24C02_ReadData(0, rbuff, 256);

	for(i = 0;i < 256;i++)
	{
		U1_printf("adrss%d=%x\r\n", i, rbuff[i]);
	}

	while(1)
	{
		
	}
}
