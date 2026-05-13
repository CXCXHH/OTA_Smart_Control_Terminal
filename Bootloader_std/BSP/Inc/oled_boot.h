#ifndef OLED_BOOT_H
#define OLED_BOOT_H

#include "stm32f10x.h"

void OLED_Boot_Init(void);
void OLED_Boot_Clear(void);
void OLED_Boot_ShowLine(uint8_t line, const char *str);
void OLED_Boot_ShowLine2x(uint8_t line, const char *str);

#endif
