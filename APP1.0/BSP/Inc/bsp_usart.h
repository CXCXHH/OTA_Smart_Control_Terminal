#ifndef BSP_USART_H
#define BSP_USART_H

#define U1_RX_SIZE 2048
#define U1_TX_SIZE 2048
#define U1_RX_MAX  256
#define NUM 	   10

#include "bsp.h"

/* USART1 接收缓冲区中一帧数据的起止指针 */
typedef struct
{
    uint8_t *start;   /* 帧起始地址 */
    uint8_t *end;     /* 帧结束地址 */
} UCB_URxBuffptr;

/* USART1 DMA + 空闲中断 帧接收控制块 */
typedef struct
{
    uint16_t URxCounter;            /* 当前接收写入位置 */
    UCB_URxBuffptr UrxDataPtr[NUM]; /* 帧描述符环形队列 */
    UCB_URxBuffptr *URxDataIN;      /* 写入指针 */
    UCB_URxBuffptr *URxDataOUT;     /* 读取指针 */
    UCB_URxBuffptr *URxDataEND;     /* 队列尾指针 */
} UCB_CB;


void Usart1_Init(uint32_t bandrate);
void U1Rx_PtrInit(void);
void Usart1_DMA_Rx_Init(uint8_t *USART1_RxBuf);
void U1_printf(char *format, ...);

#endif
