#include "main.h"

/* OTA 信息（从 AT24C02 读取/写入） */
OTA_InfoCB OTA_Info;
/* A区升级临时缓冲区 + 当前 W25Q64 块号 */
UpDataA_CB UpDataA;
/* 启动状态标记（如 UPDATA_A_FLAG） */
uint32_t BootStaFlag;

int main(void)
{
	uint8_t i;

	Delay_Init();
	Bsp_Init();
	/* 写入默认 OTA 信息到 AT24C02（第一次上电时确保 EEPROM 有数据） */
	AT24C02_WriteOTAInfo();
	/* 读回 OTA_Info 判断是否需要进入升级模式 */
	AT24C02_ReadOTAInfo();
	BootLoader_Brance();
	while(1)
	{
		if(BootStaFlag && UPDATA_A_FLAG)  /* OTA 标记有效 → 将 W25Q64 固件搬入 A 区 */
		{
			U1_printf("长度%d字节\r\n", OTA_Info.Firelen[UpDataA.W25Q64_BlockNB]);
			/* 固件长度必须 4 字节对齐（Flash 按 half-word 编程） */
			if(OTA_Info.Firelen[UpDataA.W25Q64_BlockNB] % 4 == 0)
			{
				/* 擦除 A 区全部页 */
				Flash_ErasePage(STM32_A_START_ADDR,STM32_A_PAGE_COUNT);
				/* 按 1KB/页从 W25Q64 读入 → 写入内部 Flash */
				for(i = 0;i < OTA_Info.Firelen[UpDataA.W25Q64_BlockNB] / FLASH_PAGE_SIZE;i++)
				{
					W25Q64_Read(i*1024+UpDataA.W25Q64_BlockNB*64*1024,UpDataA.Updatabuff,FLASH_PAGE_SIZE);
					Flash_WriteBuffer(i*FLASH_PAGE_SIZE+STM32_A_START_ADDR,UpDataA.Updatabuff,FLASH_PAGE_SIZE);
				}
				/* 末尾不足 1KB 的余数单独处理 */
				if(OTA_Info.Firelen[UpDataA.W25Q64_BlockNB] % 1024 != 0)
				{
					W25Q64_Read(i*1024+UpDataA.W25Q64_BlockNB*64*1024,UpDataA.Updatabuff,OTA_Info.Firelen[UpDataA.W25Q64_BlockNB] % 1024);
					Flash_WriteBuffer(i*FLASH_PAGE_SIZE+STM32_A_START_ADDR,UpDataA.Updatabuff,OTA_Info.Firelen[UpDataA.W25Q64_BlockNB] % 1024);
				}
				if(UpDataA.W25Q64_BlockNB == 0)
				{
					/* 升级完成 → 清除 OTA 标记，下次启动直接跳 A 区 */
					OTA_Info.OTA_FLAG = 0;
					AT24C02_WriteOTAInfo();
				}
				NVIC_SystemReset();  /* 复位进入用户程序 */
			}
			else
			{
				U1_printf("长度不对\r\n");
				BootStaFlag &= ~UPDATA_A_FLAG;
			}
		}
	}
}
