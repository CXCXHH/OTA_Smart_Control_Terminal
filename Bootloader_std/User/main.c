#include "stm32f10x.h"
#include "bsp.h"
#include "string.h"

#define TEST_PAGE_NUM  256

int main(void)
{
	uint16_t i;
	uint8_t wbuf[256];
	uint8_t rbuf[256];

	Bsp_Init();
	W25Q64_Init();

	U1_printf("=== W25Q64 Block0 Test ===\r\n");

	/* 擦除第0个64K块 */
	U1_printf("Erasing block 0...\r\n");
	W25Q64_EraseBlock64K(0);
	U1_printf("Erase done.\r\n");

	/* 第0页写0x00, 第1页写0x01 ... 第255页写0xFF */
	for (i = 0; i < TEST_PAGE_NUM; i++)
	{
		memset(wbuf, (uint8_t)i, 256);
		W25Q64_PageProgram(i * 256, wbuf, 256);
	}
	U1_printf("Write done.\r\n");

	/* 读出每页第1个字节验证 */
	for (i = 0; i < TEST_PAGE_NUM; i++)
	{
		W25Q64_Read(i * 256, rbuf, 1);
		U1_printf("page[%3d] = 0x%02x\r\n", i, rbuf[0]);
	}

	while(1)
	{

	}
}
