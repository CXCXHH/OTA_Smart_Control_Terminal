#include "main.h"

OTA_InfoCB OTA_Info;
UpDataA_CB UpDataA;
uint32_t BootStaFlag;

int main(void)
{
	uint8_t i;

	Delay_Init();
	Bsp_Init();
	AT24C02_WriteOTAInfo();
	AT24C02_ReadOTAInfo();
	BootLoader_Brance();
	while(1)
	{
		if(BootStaFlag && UPDATA_A_FLAG)//更新A区
		{
			U1_printf("长度%d字节\r\n", OTA_Info.Firelen[UpDataA.W25Q64_BlockNB]);
			if(OTA_Info.Firelen[UpDataA.W25Q64_BlockNB] % 4 == 0)
			{
				Flash_ErasePage(STM32_A_START_ADDR,STM32_A_PAGE_COUNT);
				for(i = 0;i < OTA_Info.Firelen[UpDataA.W25Q64_BlockNB] / FLASH_PAGE_SIZE;i++)
				{
					W25Q64_Read(i*1024+UpDataA.W25Q64_BlockNB*64*1024,UpDataA.Updatabuff,FLASH_PAGE_SIZE);
					Flash_WriteBuffer(i*FLASH_PAGE_SIZE+STM32_A_START_ADDR,UpDataA.Updatabuff,FLASH_PAGE_SIZE);
				}
				if(OTA_Info.Firelen[UpDataA.W25Q64_BlockNB] % 1024 != 0)
				{
					W25Q64_Read(i*1024+UpDataA.W25Q64_BlockNB*64*1024,UpDataA.Updatabuff,OTA_Info.Firelen[UpDataA.W25Q64_BlockNB] % 1024);
					Flash_WriteBuffer(i*FLASH_PAGE_SIZE+STM32_A_START_ADDR,UpDataA.Updatabuff,OTA_Info.Firelen[UpDataA.W25Q64_BlockNB] % 1024);
				}
				if(UpDataA.W25Q64_BlockNB == 0)
				{
					OTA_Info.OTA_FLAG = 0;
					AT24C02_WriteOTAInfo();
				}
				NVIC_SystemReset();
			}
			else
			{
				U1_printf("长度不对\r\n");
				BootStaFlag &= ~UPDATA_A_FLAG;
			}
		}
	}
}
