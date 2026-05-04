/**
 * @file    bsp_flash.c
 * @brief   内部 Flash 操作驱动实现
 *
 * 基于 STM32F10x 标准外设库 (stm32f10x_flash.c) 封装。
 * 核心限制：硬件只支持 half-word (16-bit) 编程，字节/字/缓冲区写
 * 均基于此原子操作实现。
 */

#include "bsp_flash.h"
#include "stm32f10x_flash.h"

/*===========================================================================
 * 控制器锁定/解锁
 *===========================================================================*/

void Flash_Unlock(void)
{
    /* 解锁 Flash 控制寄存器，允许擦除/编程 */
    FLASH_Unlock();
}

void Flash_Lock(void)
{
    /* 锁定 Flash 控制寄存器，防止误写 */
    FLASH_Lock();
}

/*===========================================================================
 * 页擦除
 *===========================================================================*/

void Flash_ErasePage(uint32_t start, uint32_t num)
{
    uint32_t i;

    FLASH_Unlock();

    for (i = 0; i < num; i++)
        FLASH_ErasePage(start + i * FLASH_PAGE_SIZE);  /* 逐页擦除 */
    FLASH_Lock();
}

/*===========================================================================
 * 原子写入：half-word
 *===========================================================================*/

uint8_t Flash_WriteHalfWord(uint32_t addr, uint16_t data)
{
    FLASH_Status s;

    FLASH_Unlock();
    s = FLASH_ProgramHalfWord(addr, data);
    FLASH_Lock();

    return (s == FLASH_COMPLETE) ? 0 : 1;
}

/*===========================================================================
 * 高层写入：word / byte / buffer
 * 均通过 Flash_WriteHalfWord 实现
 *===========================================================================*/

uint8_t Flash_WriteWord(uint32_t addr, uint32_t data)
{
    uint8_t ret;

    /* 32-bit 数据拆成两个 16-bit 写入 */
    ret  = Flash_WriteHalfWord(addr,       (uint16_t)(data & 0xFFFF));
    ret |= Flash_WriteHalfWord(addr + 2,   (uint16_t)(data >> 16));

    return ret;
}

uint8_t Flash_WriteByte(uint32_t addr, uint8_t data)
{
    uint16_t halfword;
    uint32_t base   = addr & ~0x01;   /* 对齐到 half-word 边界 */
    uint8_t  offset = addr & 0x01;    /* 0=低字节, 1=高字节 */

    /* 读出原 half-word，替换目标字节，再写回 */
    halfword = *(volatile uint16_t *)base;
    if (offset == 0)
        halfword = (halfword & 0xFF00) | data;
    else
        halfword = (halfword & 0x00FF) | ((uint16_t)data << 8);

    return Flash_WriteHalfWord(base, halfword);
}

uint8_t Flash_WriteBuffer(uint32_t addr, uint8_t *buf, uint32_t len)
{
    uint32_t i;

    /* 参数检查：地址必须在 Flash 范围内 */
    if (addr < FLASH_BASE_ADDR || addr + len > FLASH_BASE_ADDR + FLASH_TOTAL_SIZE)
        return 1;

    FLASH_Unlock();

    for (i = 0; i < len; i += 2)
    {
        uint16_t w = buf[i];
        if (i + 1 < len)
            w |= (uint16_t)buf[i + 1] << 8;

        if (FLASH_ProgramHalfWord(addr + i, w) != FLASH_COMPLETE)
        {
            FLASH_Lock();
            return 2;   /* 写入失败 */
        }
    }

    FLASH_Lock();
    return 0;
}

/*===========================================================================
 * 读取
 *===========================================================================*/

void Flash_ReadBuffer(uint32_t addr, uint8_t *buf, uint32_t len)
{
    uint32_t i;

    /* Flash 读无须解锁，直接指针读取 */
    for (i = 0; i < len; i++)
        buf[i] = *(volatile uint8_t *)(addr + i);
}
