#ifndef BSP_USART3_H
#define BSP_USART3_H

#include "stm32f10x.h"

/* USART3: WiFi/4G AT 指令通信 (PB10-TX, PB11-RX) */
void Usart3_Init(uint32_t baudrate);

void Usart3_SendByte(uint8_t c);
void Usart3_SendBuf(uint8_t *buf, uint16_t len);
uint8_t Usart3_GetByte(void);

#endif /* BSP_USART3_H */
