#include "delay.h"

/* HCLK = 72MHz，SysTick 时钟源选择 HCLK（非 HCLK/8）*/
void Delay_Init(void)
{
    SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK);
}

/* 72 ticks/us = HCLK 72MHz / 1MHz */
void Delay_us(uint16_t us)
{
    SysTick->LOAD = us * 72;
    SysTick->VAL = 0x00;
    SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;
    while(!(SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk));
    SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
}

void Delay_ms(uint16_t ms)
{
    while(ms--)
        Delay_us(1000);
}


