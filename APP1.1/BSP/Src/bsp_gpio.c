/**
  * @brief  GPIO ???? (LED2 only)
  * @note   LED2 (PB8) ?????
  */
#include "bsp_gpio.h"

void GPIO_Init_Outputs(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitStructure.GPIO_Pin = LED2_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_SetBits(GPIOB, LED2_PIN);
}

void LED2_Control(uint8_t state)
{
    GPIO_WRITE(GPIOB, LED2_PIN, !state);
}
