#ifndef __MAIN_H
#define __MAIN_H

#include "stm32f10x.h"
#include "bsp.h"
#include "delay.h"
#include "boot.h"

#define OTA_SET_FLAG    0xAABB1122
typedef struct 
{
    uint32_t OTA_FLAG;
}OTA_InfoCB;
#define OTA_INFOCB_SIZE sizeof(OTA_InfoCB)

extern OTA_InfoCB OTA_Info;

#endif
