#ifndef DELAY_H
#define DELAY_H

#include "stm32f10x.h"

void Delay_Init(void);
void Delay_us(uint16_t us);
void Delay_ms(uint16_t ms);

#endif
