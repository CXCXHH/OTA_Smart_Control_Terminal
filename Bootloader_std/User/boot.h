#ifndef __BOOT_H
#define __BOOT_H

#include "stm32f10x.h"

typedef void (*load_a)(void);

void BootLoader_Brance(void);
__asm void MSR_SP(uint32_t addr);
void LOAD_A(uint32_t addr);
void BootLoader_Clear(void);
void BootLoader_Info(void);
uint8_t BootLoader_Enter(uint8_t timeout);

#endif
