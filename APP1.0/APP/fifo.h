#ifndef FIFO_H
#define FIFO_H

#include <stdint.h>

#define FIFO_BUF_SIZE  512

/* 环形 FIFO: head=写入位置, tail=读取位置 */
typedef struct {
    uint8_t buf[FIFO_BUF_SIZE];
    uint16_t head;
    uint16_t tail;
} FIFO_t;

void FIFO_Init(FIFO_t *f);
uint8_t FIFO_Push(FIFO_t *f, uint8_t c);   /* 返回 1=成功, 0=满 */
uint8_t FIFO_Pop(FIFO_t *f, uint8_t *c);    /* 返回 1=成功, 0=空 */
uint16_t FIFO_Count(FIFO_t *f);

#endif
