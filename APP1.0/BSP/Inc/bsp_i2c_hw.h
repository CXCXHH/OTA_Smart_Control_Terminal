#ifndef BSP_I2C_HW_H
#define BSP_I2C_HW_H

#include "stm32f10x.h"

void I2C1_HW_Init(void);
uint8_t I2C1_WriteBuf(uint8_t addr, uint8_t *buf, uint16_t len);
uint8_t I2C1_ReadBuf(uint8_t addr, uint8_t *buf, uint16_t len);
uint8_t I2C1_MemWrite(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len);
uint8_t I2C1_MemRead(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len);

#endif
