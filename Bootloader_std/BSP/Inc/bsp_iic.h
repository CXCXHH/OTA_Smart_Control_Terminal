#ifndef BSP_IIC_H
#define BSP_IIC_H

#define IIC_SCL_H GPIO_SetBits(GPIOB, GPIO_Pin_6)
#define IIC_SCL_L GPIO_ResetBits(GPIOB, GPIO_Pin_6)

#define IIC_SDA_H GPIO_SetBits(GPIOB, GPIO_Pin_7)
#define IIC_SDA_L GPIO_ResetBits(GPIOB, GPIO_Pin_7)

#define READ_SDA  GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_7)

#include "stm32f10x.h"

void IIC_Init(void);
void IIC_Start(void);
void IIC_Stop(void);
void IIC_Send_Byte(uint8_t txd);
uint8_t IIC_wait_Ack(int16_t timeout);
uint8_t IIC_Read_Byte(uint8_t ack);

#endif
