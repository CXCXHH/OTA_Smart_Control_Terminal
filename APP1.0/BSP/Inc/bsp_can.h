#ifndef BSP_CAN_H
#define BSP_CAN_H

#include "stm32f10x.h"

/* CAN1 初始化 (500Kbps, PA11-RX PA12-TX) */
void CAN1_Init(void);
/* 发送标准 CAN 帧, 返回 1=成功 0=超时 */
uint8_t CAN1_SendMsg(uint32_t id, uint8_t *data, uint8_t len);

#endif /* BSP_CAN_H */
