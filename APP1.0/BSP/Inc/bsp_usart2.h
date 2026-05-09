#ifndef BSP_USART2_H
#define BSP_USART2_H

#include "stm32f10x.h"

void Usart2_Init(uint32_t baudrate);
void USART2_IRQHandler(void);

void Usart2_PutChar(uint8_t c);
uint8_t Usart2_GetChar(void);

#endif /* BSP_USART2_H */
