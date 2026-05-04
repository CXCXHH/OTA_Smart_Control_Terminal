#include "main.h"

OTA_InfoCB OTA_Info;

int main(void)
{
	Delay_Init();
	Bsp_Init();
	M24C02_ReadOTAInfo();
	BootLoader_Brance();
	while(1)
	{

	}
}
