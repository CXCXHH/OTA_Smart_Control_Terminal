/*******************************************************
 * FileName: boot.h
 * Description: BootLoader public interfaces
 ******************************************************/
#ifndef __BOOT_H
#define __BOOT_H

#include "stm32f10x.h"

typedef void (*load_a)(void);

void BootLoader_Brance(void);
uint8_t BootLoader_Enter(uint8_t timeout);
void BootLoader_Info(void);
void BootLoader_Event(uint8_t *data, uint16_t datalen);
void BootLoader_UpdateAFromExternalFlash(void);
uint16_t Xmodem_CRC16(uint8_t *data, uint16_t datalen);

__asm void MSR_SP(uint32_t addr);
void LOAD_A(uint32_t addr);
void BootLoader_Clear(void);

#endif
