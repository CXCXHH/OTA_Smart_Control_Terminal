#ifndef CANOPEN_APP_H
#define CANOPEN_APP_H

#include "stm32f10x.h"

void Canopen_Init(void);
void Canopen_Process(void);
void Canopen_RequestApplyControl(uint8_t control);
void Canopen_RequestApplySlaveAddr(uint8_t slave_addr);

#endif /* CANOPEN_APP_H */
