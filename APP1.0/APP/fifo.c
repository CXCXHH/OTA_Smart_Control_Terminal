#include "fifo.h"

void FIFO_Init(FIFO_t *f)
{
    f->head = 0;
    f->tail = 0;
}

uint8_t FIFO_Push(FIFO_t *f, uint8_t c)
{
    uint16_t next = (f->head + 1) % FIFO_BUF_SIZE;
    if (next == f->tail)
        return 0;  /* 满 */
    f->buf[f->head] = c;
    f->head = next;
    return 1;
}

uint8_t FIFO_Pop(FIFO_t *f, uint8_t *c)
{
    if (f->head == f->tail)
        return 0;  /* 空 */
    *c = f->buf[f->tail];
    f->tail = (f->tail + 1) % FIFO_BUF_SIZE;
    return 1;
}

uint16_t FIFO_Count(FIFO_t *f)
{
    return (f->head + FIFO_BUF_SIZE - f->tail) % FIFO_BUF_SIZE;
}
