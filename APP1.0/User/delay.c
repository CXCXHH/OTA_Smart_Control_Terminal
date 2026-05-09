#include "delay.h"

void Delay_Init(void)
{
    /* FreeRTOS 接管 SysTick 后，delay 使用 SysTick 查询模式。
       初始化仅设置时钟源，不启动定时器。 */
    SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK);
}

void Delay_us(uint16_t us)
{
    uint32_t load_backup = SysTick->LOAD;
    uint32_t val_backup  = SysTick->VAL;
    uint32_t ctrl_backup = SysTick->CTRL;

    SysTick->LOAD = (uint32_t)us * 72;
    SysTick->VAL  = 0;
    SysTick->CTRL = SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_CLKSOURCE_Msk;

    while (!(SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk));

    SysTick->CTRL = ctrl_backup;
    SysTick->LOAD = load_backup;
    SysTick->VAL  = val_backup;
}

void Delay_ms(uint16_t ms)
{
    while (ms--)
        Delay_us(1000);
}
