/**
  * @brief  USART1 驱动 (调试串口, DMA 接收 + 空闲中断)
  * @note   PA9=TX, PA10=RX
  *         接收使用 DMA 环形缓冲 + 空闲中断帧同步
  *         U1_printf() 提供格式化打印输出
  */
#include "bsp.h"
#include "bsp_usart.h"

uint8_t USART1_RxBuf[U1_RX_SIZE];
uint8_t USART1_TxBuf[U1_TX_SIZE];
UCB_CB U0CB;

void Usart1_Init(uint32_t bandrate)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	USART_InitStructure.USART_BaudRate = bandrate;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_Init(USART1, &USART_InitStructure);

	/* 空闲中断检测帧结束 (配合 DMA 接收) */
	USART_ITConfig(USART1, USART_IT_IDLE, ENABLE);

	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

	NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
	NVIC_Init(&NVIC_InitStructure);

	USART_Cmd(USART1, ENABLE);

	Usart1_DMA_Rx_Init(USART1_RxBuf);
	U1Rx_PtrInit();
}

/**
  * @brief  初始化 USART1 DMA 接收通道
  * @param  USART1_RxBuf  DMA 目标缓冲区
  * @note   DMA1 Channel5, Normal 模式,
  *         每次空闲中断后重新配置起始地址
  */
void Usart1_DMA_Rx_Init(uint8_t *USART1_RxBuf)
{
	DMA_InitTypeDef DMA_InitStructure;
	
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
	
	DMA_DeInit(DMA1_Channel5);
	DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&USART1->DR;
	DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)USART1_RxBuf;
	DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
	DMA_InitStructure.DMA_BufferSize = U1_RX_MAX+1;
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
	DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
	DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
	DMA_InitStructure.DMA_Priority = DMA_Priority_High;
	DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
	DMA_Init(DMA1_Channel5, &DMA_InitStructure);
	
	DMA_Cmd(DMA1_Channel5, ENABLE);
	USART_DMACmd(USART1, USART_DMAReq_Rx, ENABLE);
}

void U1Rx_PtrInit(void)
{
	U0CB.URxDataIN = &U0CB.UrxDataPtr[0];
	U0CB.URxDataOUT = &U0CB.UrxDataPtr[0];
	U0CB.URxDataEND = &U0CB.UrxDataPtr[NUM-1];
	U0CB.URxDataIN->start = USART1_RxBuf;
	U0CB.URxCounter = 0;
}

/**
  * @brief  USART1 格式化打印 (类似于 printf)
  * @note   阻塞发送, 最大缓冲区 U1_TX_SIZE (2048 字节)
  */
void U1_printf(char *format, ...)
{
	uint16_t i;
	va_list listdata;
	va_start(listdata, format);
	vsprintf((char *)USART1_TxBuf, format, listdata);
	va_end(listdata);

	for(i = 0;i < strlen((const char *)USART1_TxBuf);i++)
	{
		while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) != 1);
		USART_SendData(USART1, USART1_TxBuf[i]);
	}
	while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) != 1);
}
