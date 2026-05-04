/**
 * @file    bsp_flash.h
 * @brief   内部 Flash 操作驱动 (BSP 层封装)
 *
 * 提供内部 Flash 的扇区擦除、读写接口。
 * 注意：写之前必须先擦除，不可覆写非 0xFF 的地址。
 */

#ifndef BSP_FLASH_H
#define BSP_FLASH_H

#include "stm32f10x.h"

/*===========================================================================
 * Flash 参数宏
 *---------------------------------------------------------------------------
 * 硬件限制：1KB/页，共 64 页，总计 64KB
 * 适用型号：STM32F103C8 (中等容量)
 *===========================================================================*/
#define FLASH_BASE_ADDR     0x08000000      /*!< Flash 起始地址 */
#define FLASH_TOTAL_SIZE    0x00010000      /*!< 总容量 64KB */
#define FLASH_PAGE_SIZE     1024            /*!< 单页大小 1KB */
#define FLASH_PAGE_COUNT    64              /*!< 总页数 */

/** 根据页号计算 Flash 地址 */
#define FLASH_PAGE_ADDR(page)  (FLASH_BASE_ADDR + (page) * FLASH_PAGE_SIZE)
/** 根据地址反算所在页号 */
#define FLASH_ADDR_PAGE(addr)  (((uint32_t)(addr) - FLASH_BASE_ADDR) / FLASH_PAGE_SIZE)

/*---------------------------------------------------------------------------
 * 应用区起始位置（避开 bootloader 占用）
 *---------------------------------------------------------------------------
 * 第 0~7 页 (0x08000000 ~ 0x08001FFF) 共 8KB 预留给 bootloader 代码，
 * 用户数据/固件从第 8 页开始。
 *---------------------------------------------------------------------------*/
#define APP_START_PAGE  8
#define APP_START_ADDR  FLASH_PAGE_ADDR(APP_START_PAGE)

/*===========================================================================
 * API 声明
 *===========================================================================*/

/** 解锁 Flash 控制器（擦/写前必须调用） */
void     Flash_Unlock(void);

/** 锁定 Flash 控制器（擦/写完成后建议调用） */
void     Flash_Lock(void);

/**
 * @brief  擦除从 start 开始的连续 num 页
 * @param  start  页对齐起始地址，如 FLASH_PAGE_ADDR(8) 或 0x08002000
 * @param  num    要擦除的页数
 * @note   擦除后数据变为 0xFF；start 必须与页边界对齐
 */
void     Flash_ErasePage(uint32_t start, uint32_t num);

/**
 * @brief  写 1 字节（对齐到 half-word 再写）
 * @param  addr  目标地址（任意对齐）
 * @param  data  待写入数据
 * @return 0=成功, 非0=失败
 */
uint8_t  Flash_WriteByte(uint32_t addr, uint8_t data);

/**
 * @brief  写 2 字节（硬件原生写入单位）
 * @param  addr  目标地址（建议 half-word 对齐）
 * @param  data  待写入数据
 * @return 0=成功, 非0=失败
 */
uint8_t  Flash_WriteHalfWord(uint32_t addr, uint16_t data);

/**
 * @brief  写 4 字节（拆两次 half-word 写入）
 * @param  addr  目标地址
 * @param  data  待写入数据
 * @return 0=成功, 非0=失败
 */
uint8_t  Flash_WriteWord(uint32_t addr, uint32_t data);

/**
 * @brief  写任意长度缓冲区（按 half-word 循环写入）
 * @param  addr  目标地址
 * @param  buf   源数据指针
 * @param  len   字节数
 * @return 0=成功, 1=地址越界, 2=写入失败
 */
uint8_t  Flash_WriteBuffer(uint32_t addr, uint8_t *buf, uint32_t len);

/**
 * @brief  读取任意长度数据到缓冲区（直接指针读取）
 * @param  addr  源地址（Flash 内）
 * @param  buf   目标缓冲区
 * @param  len   字节数
 */
void     Flash_ReadBuffer(uint32_t addr, uint8_t *buf, uint32_t len);

#endif /* BSP_FLASH_H */
