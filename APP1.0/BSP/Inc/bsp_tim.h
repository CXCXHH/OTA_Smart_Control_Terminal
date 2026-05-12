#ifndef BSP_TIM_H
#define BSP_TIM_H

#include "stm32f10x.h"

/* TIM2 = CanFestival CANopen 定时器 (1us), TIM3 = FreeModbus 超时定时器 (50us) */
void TIM2_Init(void);
void TIM3_Init(void);

#endif /* BSP_TIM_H */
