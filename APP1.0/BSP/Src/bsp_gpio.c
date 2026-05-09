#include "bsp_gpio.h"

void GPIO_Init_Outputs(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    /* 使能 GPIOB 和 GPIOC 时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC, ENABLE);

    /* LED1(PB9), LED2(PB8), RELAY(PB1), RS485_DIR(PB2) */
    GPIO_InitStructure.GPIO_Pin = LED1_PIN | LED2_PIN | RELAY_PIN | RS485_DIR_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* BEEP(PC13) */
    GPIO_InitStructure.GPIO_Pin = BEEP_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    /* 初始状态: LED 灭(高电平), BEEP 关, RELAY 关, RS485 接收模式 */
    GPIO_SetBits(GPIOB, LED1_PIN | LED2_PIN);
    GPIO_ResetBits(GPIOB, RELAY_PIN | RS485_DIR_PIN);
    GPIO_ResetBits(GPIOC, BEEP_PIN);
}

void LED1_Control(uint8_t state)
{
    /* LED 低电平点亮 */
    GPIO_WRITE(GPIOB, LED1_PIN, !state);
}

void LED2_Control(uint8_t state)
{
    GPIO_WRITE(GPIOB, LED2_PIN, !state);
}

void BEEP_Control(uint8_t state)
{
    GPIO_WRITE(GPIOC, BEEP_PIN, state);
}

void RELAY_Control(uint8_t state)
{
    GPIO_WRITE(GPIOB, RELAY_PIN, state);
}

void RS485_TxMode(void)
{
    GPIO_SetBits(GPIOB, RS485_DIR_PIN);
}

void RS485_RxMode(void)
{
    GPIO_ResetBits(GPIOB, RS485_DIR_PIN);
}

void Output_Control(uint16_t val)
{
    LED1_Control((val & LED1_CMD) ? 1 : 0);
    LED2_Control((val & LED2_CMD) ? 1 : 0);
    BEEP_Control((val & BEEP_CMD) ? 1 : 0);
    RELAY_Control((val & RELAY_CMD) ? 1 : 0);
}
