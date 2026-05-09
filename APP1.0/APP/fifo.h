#ifndef FIFO_H
#define FIFO_H

#include <stdint.h>

#define FIFO_BUF_SIZE  512

typedef struct {
    uint8_t buf[FIFO_BUF_SIZE];
    uint16_t head;
    uint16_t tail;
} FIFO_t;

void FIFO_Init(FIFO_t *f);
uint8_t FIFO_Push(FIFO_t *f, uint8_t c);
uint8_t FIFO_Pop(FIFO_t *f, uint8_t *c);
uint16_t FIFO_Count(FIFO_t *f);

#endif
