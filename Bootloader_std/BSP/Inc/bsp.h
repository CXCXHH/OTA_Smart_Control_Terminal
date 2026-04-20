#ifndef BSP_H
#define BSP_H

#include "stm32f10x.h"
#include "stdint.h"
#include "stdarg.h"
#include "stdio.h"
#include "string.h"

#include "bsp_usart.h"

extern UCB_CB U0CB;
extern uint8_t USART1_RxBuf[U1_RX_SIZE];
#endif
