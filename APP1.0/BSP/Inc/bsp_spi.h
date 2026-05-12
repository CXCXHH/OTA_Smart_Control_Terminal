#ifndef BSP_SPI_H
#define BSP_SPI_H

#include "stm32f10x.h"

/* SPI1: 主机模式, 连接 W25Q64 Flash (PA5-SCK, PA6-MISO, PA7-MOSI) */
void SPI1_Init(void);
uint8_t SPI1_ReadwriteByte(uint8_t txd);
void SPI1_Write(uint8_t *wdata, uint16_t datalen);
void SPI1_Read(uint8_t *rdata, uint16_t datalen);

#endif
