#include "stm32f10x.h"                  
#include "bsp.h"
#include "delay.h"

int main(void)
{
	Delay_Init();
	Usart1_Init(921600);
	while(1)
	{
		U1_printf("12345");
		Delay_ms(1000);
	}
}
