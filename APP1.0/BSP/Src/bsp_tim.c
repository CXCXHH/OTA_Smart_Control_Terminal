/**
  * @brief  定时器驱动
  * @note   TIM2 — CanFestival CANopen 协议栈定时器 (1us分辨率)
  *         TIM3 — FreeModbus 协议栈定时器 (50us/tick, 3.5字符超时)
  */
#include "bsp_tim.h"

/**
  * @brief  初始化 TIM2 (CanFestival 时间基准)
  * @note   72MHz/72 = 1MHz → 1us 分辨率, 周期 65535us
  *         IRQ 在 CanFestival_Can_Init() 中使能,
  *         避免在协议栈未初始化时触发 TimeDispatch()
  */
void TIM2_Init(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    TIM_TimeBaseStructure.TIM_Prescaler = 72 - 1;      /* 72MHz/72 = 1MHz = 1us */
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_Period = 0xFFFF;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    /* TIM2 IRQ starts later in CanFestival_Can_Init() to avoid calling
       TimeDispatch() before the stack is initialized */
    NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelCmd = DISABLE;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_Init(&NVIC_InitStructure);

    TIM_Cmd(TIM2, ENABLE);
}

/**
  * @brief  初始化 TIM3 (FreeModbus 3.5字符超时定时器)
  * @note   72MHz / (3599+1) = 20KHz → 50us/tick
  *         周期由 FreeModbus porttimer.c 在初始化时动态计算并设置
  */
void TIM3_Init(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

    /* 72MHz / (3599+1) = 20KHz → 50us/tick */
    TIM_TimeBaseStructure.TIM_Prescaler = 3599;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_Period = 0xFFFF;  /* 由 FreeModbus 动态设置 */
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);

    NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_Init(&NVIC_InitStructure);
}
