#ifndef BSP_GPIO_H
#define BSP_GPIO_H

#include "stm32f10x.h"

/* 输出器件引脚定义 */
#define LED1_PIN     GPIO_Pin_9   /* PB9 */
#define LED2_PIN     GPIO_Pin_8   /* PB8 */
#define RELAY_PIN    GPIO_Pin_1   /* PB1 */
#define BEEP_PIN     GPIO_Pin_13  /* PC13 */
#define RS485_DIR_PIN GPIO_Pin_2  /* PB2, HIGH=发送, LOW=接收 */

/* 输出控制位定义 (对应 REG_HOLD_BUF[0]) */
#define LED1_CMD    ((uint16_t)0x0001)
#define LED2_CMD    ((uint16_t)0x0002)
#define BEEP_CMD    ((uint16_t)0x0004)
#define RELAY_CMD   ((uint16_t)0x0008)

/* GPIO 输出宏: val=1 置位, val=0 复位 */
#define GPIO_WRITE(port, pin, val)  \
    do {                            \
        if (val)                    \
            GPIO_SetBits(port, pin);\
        else                        \
            GPIO_ResetBits(port, pin);\
    } while(0)

void GPIO_Init_Outputs(void);

/* 输出控制函数 */
void LED1_Control(uint8_t state);
void LED2_Control(uint8_t state);
void BEEP_Control(uint8_t state);
void RELAY_Control(uint8_t state);

/* RS485 方向控制 */
void RS485_TxMode(void);
void RS485_RxMode(void);

/* 批量输出控制: 根据 REG_HOLD_BUF[0] 的值设置全部输出 */
void Output_Control(uint16_t val);

#endif /* BSP_GPIO_H */
