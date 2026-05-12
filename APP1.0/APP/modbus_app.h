#ifndef MODBUS_APP_H
#define MODBUS_APP_H

#include "stm32f10x.h"

#define REG_HOLD_SIZE   10
#define REG_INPUT_SIZE  10

extern uint16_t REG_HOLD_BUF[REG_HOLD_SIZE];
extern volatile uint8_t Modify_SlaveAdress_Flag;

void Modbus_Init(uint8_t slave_addr);
void Modbus_Task(void);
void Modbus_Parse(void);

/* Shared register mutex API - protects REG_HOLD_BUF and REG_COILS_BUF */
void REG_Lock_Init(void);
void REG_Lock(void);
void REG_Unlock(void);

#endif
