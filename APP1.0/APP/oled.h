#ifndef OLED_H
#define OLED_H

#include <stdint.h>

#define OLED_ADDR  0x78

void OLED_Init(void);
void OLED_Clear(void);
void OLED_ShowStr(uint8_t x, uint8_t y, const char ch[]);

#endif
