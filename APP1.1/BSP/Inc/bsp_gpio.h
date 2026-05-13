#ifndef BSP_GPIO_H
#define BSP_GPIO_H

#include "stm32f10x.h"

#define LED2_PIN     GPIO_Pin_8

#define GPIO_WRITE(port, pin, val)  \
    do {                            \
        if (val)                    \
            GPIO_SetBits(port, pin);\
        else                        \
            GPIO_ResetBits(port, pin);\
    } while(0)

void GPIO_Init_Outputs(void);
void LED2_Control(uint8_t state);

#endif /* BSP_GPIO_H */
