#ifndef BSP_CAN_H
#define BSP_CAN_H

#include "stm32f10x.h"

void CAN1_Init(void);
uint8_t CAN1_SendMsg(uint32_t id, uint8_t *data, uint8_t len);

#endif /* BSP_CAN_H */
