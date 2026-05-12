#ifndef MODBUS_APP_H
#define MODBUS_APP_H

#include "stm32f10x.h"

#define REG_HOLD_SIZE   10
#define REG_INPUT_SIZE  10

/* Shared register index macros for REG_HOLD_BUF */
#define REG_IDX_OUTPUT      0
#define REG_IDX_TEMP        1
#define REG_IDX_HUMI        2
#define REG_IDX_DEV_VOLT    3
#define REG_IDX_DEV_CURR    4
#define REG_IDX_DEV_POWER   5
#define REG_IDX_SYS_VOLT    6
#define REG_IDX_CPU_TEMP    7
#define REG_IDX_SLAVE_ADDR  9

extern uint16_t REG_HOLD_BUF[REG_HOLD_SIZE];
extern volatile uint8_t Modify_SlaveAdress_Flag;

void Modbus_Init(uint8_t slave_addr);
void Modbus_Task(void);
void Modbus_Parse(void);
void App_Output_ApplySnapshot(uint16_t hold0,
                              uint8_t coil0,
                              uint8_t coil1,
                              uint8_t coil2,
                              uint8_t coil3);
void App_Output_RefreshFromSharedRegs(void);

/* Shared register mutex API - protects REG_HOLD_BUF and REG_COILS_BUF */
void REG_Lock_Init(void);
void REG_Lock(void);
void REG_Unlock(void);

#endif
