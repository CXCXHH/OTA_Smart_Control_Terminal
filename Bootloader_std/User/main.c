#include "stm32f10x.h"                  
#include "bsp.h"

uint16_t i;

int main(void)
{
	Usart1_Init(921600);
	U1_printf("%d %c %x", 0x30, 0x30, 0x30);
	while(1)
	{
		if(U0CB.URxDataOUT != U0CB.URxDataIN)
		{
			U1_printf("%d\r\n", U0CB.URxDataOUT->end - U0CB.URxDataOUT->start + 1);
			for(i = 0;i < U0CB.URxDataOUT->end - U0CB.URxDataOUT->start + 1;i++)
			{
				U1_printf("%c", U0CB.URxDataOUT->start[i]);
			}
			U1_printf("\r\n\r\n");
			U0CB.URxDataOUT++;
			if(U0CB.URxDataOUT == U0CB.URxDataEND)
			{
				U0CB.URxDataOUT = &U0CB.UrxDataPtr[0];
			}
		}
	}
}
