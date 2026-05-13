#ifndef BSP_H
#define BSP_H

#define BIT(x)          ((uint32_t)((uint32_t)0x01U<<(x)))

#include "stm32f10x.h"
#include "stdint.h"
#include "stdarg.h"
#include "stdio.h"
#include "string.h"

#include "main.h"
#include "delay.h"

#include "bsp_usart.h"
#include "bsp_iic.h"
#include "bsp_spi.h"
#include "bsp_w25q64.h"
#include "bsp_AT24C02.h"
#include "bsp_flash.h"
#include "oled_boot.h"

void Bsp_Init(void);

extern UCB_CB U0CB;
extern uint8_t USART1_RxBuf[U1_RX_SIZE];
#endif
