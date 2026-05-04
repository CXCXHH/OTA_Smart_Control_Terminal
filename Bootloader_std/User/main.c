#include "stm32f10x.h"
#include "bsp.h"
#include "delay.h"

uint32_t wbuff[1024];
uint32_t i;

int main(void)
{
	Bsp_Init();

	for(i = 0; i < 1024; i++)
		wbuff[i] = 0x12345678;

	/* 擦除最后 4 页 (页 60~63) */
	Flash_ErasePage(FLASH_PAGE_ADDR(60), 4);

	/* 写入 4KB 数据 */
	Flash_WriteBuffer(FLASH_PAGE_ADDR(60), (uint8_t *)wbuff, sizeof(wbuff));

	/* 读取验证 — 只打印原始数据 */
	{
		uint8_t rbuf[sizeof(wbuff)];
		uint32_t j;
		Flash_ReadBuffer(FLASH_PAGE_ADDR(60), rbuf, sizeof(rbuf));
		for (j = 0; j < sizeof(rbuf); j++)
			U1_printf("%02x", rbuf[j]);
		U1_printf("\r\n");
	}

	while(1)
	{

	}
}
