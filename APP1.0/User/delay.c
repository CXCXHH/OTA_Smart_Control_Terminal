/**
  * @brief  微秒/毫秒延迟 (TIM1 查询模式)
  * @note   使用 TIM1 (APB2, 72MHz) 独立定时器实现阻塞延迟，
  *         与 FreeRTOS 的 SysTick 完全解耦，无需保存/恢复寄存器。
  *         分频 71 后 1 tick = 1us，16 位回绕由无符号减法自然处理。
  *         TIM1 初始化后一直自由运行，Delay_us 仅读取 CNT 寄存器。
  */
#include "delay.h"

void Delay_Init(void)
{
    /* 使能 TIM1 时钟 (APB2) */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);

    /* 72MHz / 72 = 1MHz → 1us/tick, 自由运行 */
    TIM1->PSC = 72 - 1;
    TIM1->ARR = 0xFFFF;
    TIM1->CR1 = TIM_CR1_CEN;
}

void Delay_us(uint16_t us)
{
    uint16_t start = TIM1->CNT;

    while ((uint16_t)(TIM1->CNT - start) < us);
}

void Delay_ms(uint16_t ms)
{
    while (ms--)
        Delay_us(1000);
}
