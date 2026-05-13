#ifndef BSP_W25Q64_H
#define BSP_W25Q64_H

#include "stm32f10x.h"

/* W25Q64 容量 */
#define W25Q64_FLASH_SIZE		0x800000  /* 8MB */
#define W25Q64_BLOCK_SIZE		(64 * 1024) /* 64KB/block (BlockErase64K 擦除单位) */
#define W25Q64_SECTOR_SIZE		4096      /* 4KB/sector */
#define W25Q64_PAGE_SIZE		256       /* 256B/page */

/* 指令集 */
#define W25X_WriteEnable		0x06
#define W25X_WriteDisable		0x04
#define W25X_ReadStatusReg1		0x05
#define W25X_WriteStatusReg1	0x01
#define W25X_ReadData			0x03
#define W25X_FastRead			0x0B
#define W25X_PageProgram		0x02
#define W25X_SectorErase		0x20
#define W25X_BlockErase32K		0x52
#define W25X_BlockErase64K		0xD8
#define W25X_ChipErase			0xC7
#define W25X_PowerDown			0xB9
#define W25X_ReleasePowerDown	0xAB
#define W25X_DeviceID			0xAB
#define W25X_ManufactDeviceID	0x90
#define W25X_JedecDeviceID		0x9F

/* 状态寄存器1 位偏移定义 */
#define W25X_SR1_BUSY			(1 << 0)
#define W25X_SR1_WEL			(1 << 1)

/* CS 引脚: PA4 */
#define W25Q64_CS_LOW()			GPIO_ResetBits(GPIOA, GPIO_Pin_4)
#define W25Q64_CS_HIGH()		GPIO_SetBits(GPIOA, GPIO_Pin_4)

uint8_t W25Q64_Init(void);
void    W25Q64_WaitBusy(void);
void    W25Q64_WriteEnable(void);
void    W25Q64_Read(uint32_t addr, uint8_t *buf, uint32_t len);
void    W25Q64_PageProgram(uint32_t addr, const uint8_t *buf, uint16_t len);
void    W25Q64_EraseBlock64K(uint32_t addr);

#endif
