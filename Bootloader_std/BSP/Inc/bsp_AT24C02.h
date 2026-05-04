#ifndef BSP_AT24C02_H
#define BSP_AT24C02_H

#include "stm32f10x.h"

#define AT24C02_WADDR   0xA0
#define AT24C02_RADDR   0xA1

uint8_t AT24C02_WriteByte(uint8_t addr, uint8_t wdata);
uint8_t AT24C02_WritePage(uint8_t addr, uint8_t *wdata);
uint8_t AT24C02_ReadData(uint8_t addr, uint8_t *rdata, uint16_t datalen);
void M24C02_ReadOTAInfo(void);

#endif
