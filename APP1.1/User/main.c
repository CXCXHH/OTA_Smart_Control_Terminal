/**
  * @brief  APP1.1 OTA ????
  * @note   ??????????1????? LED2 (PB8)
  */
#include "stm32f10x.h"
#include "bsp.h"

int main(void)
{
    uint8_t led_on = 0;

    Bsp_Init();

    while (1)
    {
        led_on ^= 1U;
        LED2_Control(led_on);
        Delay_ms(1000);
    }
}
