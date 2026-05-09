/*******************************************************
 * FileName: main.h
 * Description: 全局类型定义、状态标志、外部变量声明
 ******************************************************/
#ifndef __MAIN_H
#define __MAIN_H

#include "string.h"

#include "stm32f10x.h"
#include "bsp.h"
#include "bsp_flash.h"
#include "delay.h"
#include "boot.h"

#define OTA_SET_FLAG       0xAABB1122
#define UPDATA_A_FLAG      0x00000001
#define IAP_XMODEMC_FLAG   0x00000002
#define IAP_XMODEMD_FLAG   0x00000004
#define SET_VERDION_FLAG   0x00000008
#define CMD5_FLAG          0x00000010
#define CMD5_XMODEM_FLAG   0x00000020
#define CMD6_FLAG          0x00000040

typedef struct 
{
    uint32_t OTA_FLAG;
    uint32_t Firelen[11];  /* 0号成员固定对应OTA总体大小 */
    uint8_t  OTA_ver[32];
}OTA_InfoCB;
#define OTA_INFOCB_SIZE sizeof(OTA_InfoCB)

typedef struct 
{
    uint8_t Updatabuff[FLASH_PAGE_SIZE];
    uint32_t W25Q64_BlockNB;
    uint32_t XmodemTimer;
    uint32_t XmodemNB;
    uint32_t XmodemCRC;
}UpDataA_CB;

extern OTA_InfoCB OTA_Info;
extern UpDataA_CB UpDataA;
extern uint32_t BootStaFlag;

/* Internal main loop handlers (defined in main.c) */
void BootLoader_PollCommandFrame(void);
void BootLoader_PollXmodemRequest(void);
void BootLoader_UpdateAFromExternalFlash(void);

#endif
