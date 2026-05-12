#ifndef BSP_USART2_H
#define BSP_USART2_H

#include "stm32f10x.h"

/* USART2: Modbus RTU 从机 (PA2-TX, PA3-RX) */
void Usart2_Init(uint32_t baudrate);
void USART2_IRQHandler(void);

/* FreeModbus 协议栈使用的收发原语 */
void Usart2_PutChar(uint8_t c);
uint8_t Usart2_GetChar(void);

#endif /* BSP_USART2_H */
