#ifndef __MAIN_H
#define __MAIN_H

#include "stm32f10x.h"
#include "bsp.h"
#include "bsp_flash.h"
#include "delay.h"
#include "boot.h"

#define OTA_SET_FLAG    0xAABB1122
typedef struct 
{
    uint32_t OTA_FLAG;
    uint32_t Firelen[11];  // 0号成员固定对呀OTA的大小
}OTA_InfoCB;
#define OTA_INFOCB_SIZE sizeof(OTA_InfoCB)

typedef struct 
{
    uint8_t Updatabuff[FLASH_PAGE_SIZE];
    uint32_t W25Q64_BlockNB;
}UpDataA_CB;

extern OTA_InfoCB OTA_Info;
extern UpDataA_CB UpDataA;
#endif
