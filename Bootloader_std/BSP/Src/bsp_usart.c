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

	/* PA9(TX): 复用推挽输出 */
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	/* PA10(RX): 上拉输入 */
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

	/* 使用 IDLE 中断而非 RXNE：数据由 DMA 连续搬运，
	   每帧结束后 IDLE 中断通知 CPU 处理，减少单字节中断开销 */
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

void U1_printf(char *format, ...)
{
	uint16_t i;
	va_list listdata;
	va_start(listdata, format);
	vsprintf((char *)USART1_TxBuf, format, listdata);
	va_end(listdata);

	/* 逐字节等待 TXE（发送寄存器空）后写入 DR */
	for(i = 0;i < strlen((const char *)USART1_TxBuf);i++)
	{
		while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) != 1);
		USART_SendData(USART1, USART1_TxBuf[i]);
	}
	/* 等待 TC（发送完成），确保最后 1 字节移位寄存器移完再返回 */
	while(USART_GetFlagStatus(USART1, USART_FLAG_TC) != 1);
}
