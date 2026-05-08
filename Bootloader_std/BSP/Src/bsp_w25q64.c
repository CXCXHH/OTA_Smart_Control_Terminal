#include "bsp.h"
#include "bsp_w25q64.h"

/* CS 引脚初始化 */
static void W25Q64_CS_GPIO_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	W25Q64_CS_HIGH();
}

/* 读写一个字节 */
static uint8_t W25Q64_SPI_RW(uint8_t data)
{
	return SPI1_ReadwriteByte(data);
}

/* 读取状态寄存器 */
static uint8_t W25Q64_ReadSR(void)
{
	uint8_t byte = 0;

	W25Q64_CS_LOW();
	W25Q64_SPI_RW(W25X_ReadStatusReg1);
	byte = W25Q64_SPI_RW(0xFF);
	W25Q64_CS_HIGH();

	return byte;
}

/* 等待 BUSY 位清除 */
void W25Q64_WaitBusy(void)
{
	while (W25Q64_ReadSR() & W25X_SR1_BUSY);
}

/* 写使能 */
void W25Q64_WriteEnable(void)
{
	W25Q64_CS_LOW();
	W25Q64_SPI_RW(W25X_WriteEnable);
	W25Q64_CS_HIGH();
}

/* 初始化 W25Q64 */
uint8_t W25Q64_Init(void)
{
	SPI1_Init();
	W25Q64_CS_GPIO_Init();

	/* 等待 W25Q64 上电就绪 */
	Delay_ms(10);

	return 0;
}

/* 读取数据 */
void W25Q64_Read(uint32_t addr, uint8_t *buf, uint32_t len)
{
	uint32_t i;

	W25Q64_CS_LOW();
	W25Q64_SPI_RW(W25X_ReadData);
	W25Q64_SPI_RW((uint8_t)(addr >> 16));
	W25Q64_SPI_RW((uint8_t)(addr >> 8));
	W25Q64_SPI_RW((uint8_t)(addr));
	for (i = 0; i < len; i++)
		buf[i] = W25Q64_SPI_RW(0xFF);
	W25Q64_CS_HIGH();
}

/* 页写入 (一页 256B) */
void W25Q64_PageProgram(uint32_t addr, const uint8_t *buf, uint16_t len)
{
	uint16_t i;

	W25Q64_WriteEnable();
	W25Q64_CS_LOW();
	W25Q64_SPI_RW(W25X_PageProgram);
	W25Q64_SPI_RW((uint8_t)(addr >> 16));
	W25Q64_SPI_RW((uint8_t)(addr >> 8));
	W25Q64_SPI_RW((uint8_t)(addr));
	for (i = 0; i < len; i++)
		W25Q64_SPI_RW(buf[i]);
	W25Q64_CS_HIGH();
	W25Q64_WaitBusy();
}

/* 擦除一个 64KB 块（W25Q64 最小擦除单位之一，另有 4KB 扇区擦除可用） */
void W25Q64_EraseBlock64K(uint32_t addr)
{
	W25Q64_WriteEnable();
	W25Q64_CS_LOW();
	W25Q64_SPI_RW(W25X_BlockErase64K);
	W25Q64_SPI_RW((uint8_t)(addr >> 16));
	W25Q64_SPI_RW((uint8_t)(addr >> 8));
	W25Q64_SPI_RW((uint8_t)(addr));
	W25Q64_CS_HIGH();
	W25Q64_WaitBusy();
}
