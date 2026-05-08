#include "bsp.h"
#include "bsp_spi.h"

void SPI1_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
	SPI_InitTypeDef SPI_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

	/* PA5-SCK, PA7-MOSI: 复用推挽输出 */
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	/* PA6-MISO: 浮空输入 */
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	/* 复位 SPI1 */
	SPI_I2S_DeInit(SPI1);

	/* SPI Mode 0 (CPOL=Low, CPHA=1Edge) — W25Q64 默认 SPI 模式 */
	SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
	SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
	SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
	SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
	SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
	/* 软件 NSS，由 GPIO PA4 独立控制 W25Q64 CS */
	SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
	/* APB2=72MHz，预分频 2 → SCK=36MHz，W25Q64 最大支持 80MHz */
	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_2;
	SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
	SPI_InitStructure.SPI_CRCPolynomial = 7;
	SPI_Init(SPI1, &SPI_InitStructure);

	/* 使能 SPI1 */
	SPI_Cmd(SPI1, ENABLE);
}

uint8_t SPI1_ReadwriteByte(uint8_t txd)
{
	while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET);
	SPI_I2S_SendData(SPI1, txd);
	while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET);
	return SPI_I2S_ReceiveData(SPI1);
}

void SPI1_Write(uint8_t *wdata, uint16_t datalen)
{
    uint16_t i;
    for(i=0;i<datalen;i++)
        SPI1_ReadwriteByte(wdata[i]);
}

void SPI1_Read(uint8_t *rdata, uint16_t datalen)
{
    uint16_t i;
    for(i=0;i<datalen;i++)
        rdata[i] = SPI1_ReadwriteByte(0xff);
}
