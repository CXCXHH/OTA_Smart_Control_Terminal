#include "main.h"

OTA_InfoCB OTA_Info;
UpDataA_CB UpDataA;

int main(void)
{
	uint8_t i;

	Delay_Init();
	Bsp_Init();

	OTA_Info.OTA_FLAG = 0x11223344;
	for(i=0;i<11;i++)
	{
		OTA_Info.Firelen[i] = i;
	}

	AT24C02_WriteOTAInfo();
	AT24C02_ReadOTAInfo();

	U1_printf("%x\r\n", OTA_Info.OTA_FLAG);
	for(i=0;i<11;i++)
		U1_printf("%x\r\n", OTA_Info.Firelen[i]);

	BootLoader_Brance();
	while(1)
	{
		
	}
}
