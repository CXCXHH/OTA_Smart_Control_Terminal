#ifndef BSP_ADC_H
#define BSP_ADC_H

#include "stm32f10x.h"

void ADC1_Init(void);
uint16_t ADC_GetVoltage(void);       /* 返回 PA1 电压值*100 */
uint16_t ADC_GetTemperature(void);   /* 返回 CPU 温度值*100 */

#endif /* BSP_ADC_H */
