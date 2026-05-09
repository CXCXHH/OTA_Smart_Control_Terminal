#include "bsp_i2c_hw.h"

#define I2C_TIMEOUT  0xFFFF

void I2C1_HW_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    I2C_InitTypeDef I2C_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);

    /* PB6=SCL, PB7=SDA: AF open-drain, external 4.7k pull-up required */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    I2C_DeInit(I2C1);
    I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
    I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
    I2C_InitStructure.I2C_OwnAddress1 = 0x00;
    I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
    I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_InitStructure.I2C_ClockSpeed = 100000;
    I2C_Init(I2C1, &I2C_InitStructure);

    I2C_Cmd(I2C1, ENABLE);
}

/* ---- flag wait helpers ---- */

static uint8_t I2C1_WaitFlag(uint32_t flag, FunctionalState state, uint32_t timeout)
{
    while (timeout--)
    {
        if (I2C_GetFlagStatus(I2C1, flag) == state)
            return 0;
    }
    return 1;
}

static uint8_t I2C1_CheckEvent(uint32_t event, uint32_t timeout)
{
    while (timeout--)
    {
        if (I2C_CheckEvent(I2C1, event) == SUCCESS)
            return 0;
    }
    return 1;
}

/* ---- bus reset: clock out 9 SCL pulses to release stuck slave ---- */

static void I2C1_BusReset(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    uint8_t i;

    I2C_Cmd(I2C1, DISABLE);

    /* Reconfigure PB6/PB7 as push-pull to bit-bang recovery pulses */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_SetBits(GPIOB, GPIO_Pin_6 | GPIO_Pin_7);

    for (i = 0; i < 9; i++)
    {
        GPIO_ResetBits(GPIOB, GPIO_Pin_6);
        for (volatile uint16_t d = 0; d < 10; d++);
        GPIO_SetBits(GPIOB, GPIO_Pin_6);
        for (volatile uint16_t d = 0; d < 10; d++);
    }

    /* Generate STOP: SDA low -> SCL high -> SDA high */
    GPIO_ResetBits(GPIOB, GPIO_Pin_7);
    for (volatile uint16_t d = 0; d < 10; d++);
    GPIO_SetBits(GPIOB, GPIO_Pin_6);
    for (volatile uint16_t d = 0; d < 10; d++);
    GPIO_SetBits(GPIOB, GPIO_Pin_7);
    for (volatile uint16_t d = 0; d < 10; d++);

    /* Back to AF_OD */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    I2C_Cmd(I2C1, ENABLE);
}

/* ---- master write (addr 8-bit, no register) ---- */

uint8_t I2C1_WriteBuf(uint8_t addr, uint8_t *buf, uint16_t len)
{
    uint16_t i;

    if (len == 0) return 0;

    /* Bus busy recovery */
    if (I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY))
    {
        uint32_t t = I2C_TIMEOUT;
        while (I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY) && --t);
        if (t == 0) I2C1_BusReset();
    }

    I2C_GenerateSTART(I2C1, ENABLE);
    if (I2C1_CheckEvent(I2C_EVENT_MASTER_MODE_SELECT, I2C_TIMEOUT))
        return 1;

    I2C_Send7bitAddress(I2C1, addr, I2C_Direction_Transmitter);
    if (I2C1_WaitFlag(I2C_FLAG_AF, SET, I2C_TIMEOUT) == 0)
        goto err;
    if (I2C1_CheckEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED, I2C_TIMEOUT))
        goto err;

    for (i = 0; i < len; i++)
    {
        if (I2C1_WaitFlag(I2C_FLAG_TXE, SET, I2C_TIMEOUT))
            goto err;
        I2C_SendData(I2C1, buf[i]);
    }

    if (I2C1_WaitFlag(I2C_FLAG_BTF, SET, I2C_TIMEOUT))
        goto err;

    I2C_GenerateSTOP(I2C1, ENABLE);
    return 0;

err:
    I2C_GenerateSTOP(I2C1, ENABLE);
    return 1;
}

/* ---- master read (addr 8-bit, no register) ---- */

uint8_t I2C1_ReadBuf(uint8_t addr, uint8_t *buf, uint16_t len)
{
    uint16_t i;

    if (len == 0) return 0;

    if (I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY))
    {
        uint32_t t = I2C_TIMEOUT;
        while (I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY) && --t);
        if (t == 0) I2C1_BusReset();
    }

    I2C_GenerateSTART(I2C1, ENABLE);
    if (I2C1_CheckEvent(I2C_EVENT_MASTER_MODE_SELECT, I2C_TIMEOUT))
        return 1;

    I2C_Send7bitAddress(I2C1, addr, I2C_Direction_Receiver);
    if (I2C1_WaitFlag(I2C_FLAG_AF, SET, I2C_TIMEOUT) == 0)
        goto err;

    /* ACK/NACK control before clearing ADDR */
    if (len == 1)
    {
        I2C_AcknowledgeConfig(I2C1, DISABLE);
        if (I2C1_CheckEvent(I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED, I2C_TIMEOUT))
            goto err;
        I2C_GenerateSTOP(I2C1, ENABLE);
        if (I2C1_WaitFlag(I2C_FLAG_RXNE, SET, I2C_TIMEOUT))
            goto err;
        buf[0] = I2C_ReceiveData(I2C1);
    }
    else if (len == 2)
    {
        /* POS=1 + ACK=0: both bytes received before STOP, then read DRx2 */
        I2C_AcknowledgeConfig(I2C1, DISABLE);
        I2C1->CR1 |= I2C_CR1_POS;
        if (I2C1_CheckEvent(I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED, I2C_TIMEOUT))
            goto err;
        if (I2C1_WaitFlag(I2C_FLAG_BTF, SET, I2C_TIMEOUT))
            goto err;
        I2C_GenerateSTOP(I2C1, ENABLE);
        buf[0] = I2C_ReceiveData(I2C1);
        buf[1] = I2C_ReceiveData(I2C1);
        I2C1->CR1 &= ~I2C_CR1_POS;
    }
    else /* len >= 3 */
    {
        if (I2C1_CheckEvent(I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED, I2C_TIMEOUT))
            goto err;

        /* Read bytes 0 .. len-4 */
        for (i = 0; i < len - 3; i++)
        {
            if (I2C1_WaitFlag(I2C_FLAG_RXNE, SET, I2C_TIMEOUT))
                goto err;
            buf[i] = I2C_ReceiveData(I2C1);
        }

        /* Byte len-3: wait, then disable ACK + POS for last two */
        if (I2C1_WaitFlag(I2C_FLAG_BTF, SET, I2C_TIMEOUT))
            goto err;
        I2C_AcknowledgeConfig(I2C1, DISABLE);

        /* Byte len-3 */
        buf[len - 3] = I2C_ReceiveData(I2C1);

        /* Byte len-2 */
        if (I2C1_WaitFlag(I2C_FLAG_BTF, SET, I2C_TIMEOUT))
            goto err;
        I2C_GenerateSTOP(I2C1, ENABLE);
        buf[len - 2] = I2C_ReceiveData(I2C1);

        /* Byte len-1 */
        if (I2C1_WaitFlag(I2C_FLAG_RXNE, SET, I2C_TIMEOUT))
            goto err;
        buf[len - 1] = I2C_ReceiveData(I2C1);
    }
    return 0;

err:
    I2C_GenerateSTOP(I2C1, ENABLE);
    return 1;
}

/* ---- mem write: START, addr+W, reg, data, STOP ---- */

uint8_t I2C1_MemWrite(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len)
{
    uint16_t i;

    if (len == 0) return 0;

    if (I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY))
    {
        uint32_t t = I2C_TIMEOUT;
        while (I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY) && --t);
        if (t == 0) I2C1_BusReset();
    }

    I2C_GenerateSTART(I2C1, ENABLE);
    if (I2C1_CheckEvent(I2C_EVENT_MASTER_MODE_SELECT, I2C_TIMEOUT))
        return 1;

    I2C_Send7bitAddress(I2C1, addr, I2C_Direction_Transmitter);
    if (I2C1_WaitFlag(I2C_FLAG_AF, SET, I2C_TIMEOUT) == 0)
        goto err;
    if (I2C1_CheckEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED, I2C_TIMEOUT))
        goto err;

    /* Send register address */
    if (I2C1_WaitFlag(I2C_FLAG_TXE, SET, I2C_TIMEOUT))
        goto err;
    I2C_SendData(I2C1, reg);

    /* Send data */
    for (i = 0; i < len; i++)
    {
        if (I2C1_WaitFlag(I2C_FLAG_TXE, SET, I2C_TIMEOUT))
            goto err;
        I2C_SendData(I2C1, buf[i]);
    }

    if (I2C1_WaitFlag(I2C_FLAG_BTF, SET, I2C_TIMEOUT))
        goto err;

    I2C_GenerateSTOP(I2C1, ENABLE);
    return 0;

err:
    I2C_GenerateSTOP(I2C1, ENABLE);
    return 1;
}

/* ---- mem read: START, addr+W, reg, repeated-START, addr+R, data, STOP ---- */

uint8_t I2C1_MemRead(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len)
{
    uint16_t i;

    if (len == 0) return 0;

    if (I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY))
    {
        uint32_t t = I2C_TIMEOUT;
        while (I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY) && --t);
        if (t == 0) I2C1_BusReset();
    }

    /* Phase 1: write register address */
    I2C_GenerateSTART(I2C1, ENABLE);
    if (I2C1_CheckEvent(I2C_EVENT_MASTER_MODE_SELECT, I2C_TIMEOUT))
        return 1;

    I2C_Send7bitAddress(I2C1, addr, I2C_Direction_Transmitter);
    if (I2C1_WaitFlag(I2C_FLAG_AF, SET, I2C_TIMEOUT) == 0)
        goto err;
    if (I2C1_CheckEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED, I2C_TIMEOUT))
        goto err;

    if (I2C1_WaitFlag(I2C_FLAG_TXE, SET, I2C_TIMEOUT))
        goto err;
    I2C_SendData(I2C1, reg);

    if (I2C1_WaitFlag(I2C_FLAG_BTF, SET, I2C_TIMEOUT))
        goto err;

    /* Phase 2: repeated START + read */
    I2C_GenerateSTART(I2C1, ENABLE);
    if (I2C1_CheckEvent(I2C_EVENT_MASTER_MODE_SELECT, I2C_TIMEOUT))
        goto err;

    I2C_Send7bitAddress(I2C1, addr, I2C_Direction_Receiver);
    if (I2C1_WaitFlag(I2C_FLAG_AF, SET, I2C_TIMEOUT) == 0)
        goto err;

    if (len == 1)
    {
        I2C_AcknowledgeConfig(I2C1, DISABLE);
        if (I2C1_CheckEvent(I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED, I2C_TIMEOUT))
            goto err;
        I2C_GenerateSTOP(I2C1, ENABLE);
        if (I2C1_WaitFlag(I2C_FLAG_RXNE, SET, I2C_TIMEOUT))
            goto err;
        buf[0] = I2C_ReceiveData(I2C1);
    }
    else if (len == 2)
    {
        I2C_AcknowledgeConfig(I2C1, DISABLE);
        I2C1->CR1 |= I2C_CR1_POS;
        if (I2C1_CheckEvent(I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED, I2C_TIMEOUT))
            goto err;
        if (I2C1_WaitFlag(I2C_FLAG_BTF, SET, I2C_TIMEOUT))
            goto err;
        I2C_GenerateSTOP(I2C1, ENABLE);
        buf[0] = I2C_ReceiveData(I2C1);
        buf[1] = I2C_ReceiveData(I2C1);
        I2C1->CR1 &= ~I2C_CR1_POS;
    }
    else /* len >= 3 */
    {
        if (I2C1_CheckEvent(I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED, I2C_TIMEOUT))
            goto err;

        for (i = 0; i < len - 3; i++)
        {
            if (I2C1_WaitFlag(I2C_FLAG_RXNE, SET, I2C_TIMEOUT))
                goto err;
            buf[i] = I2C_ReceiveData(I2C1);
        }

        if (I2C1_WaitFlag(I2C_FLAG_BTF, SET, I2C_TIMEOUT))
            goto err;
        I2C_AcknowledgeConfig(I2C1, DISABLE);
        buf[len - 3] = I2C_ReceiveData(I2C1);

        if (I2C1_WaitFlag(I2C_FLAG_BTF, SET, I2C_TIMEOUT))
            goto err;
        I2C_GenerateSTOP(I2C1, ENABLE);
        buf[len - 2] = I2C_ReceiveData(I2C1);

        if (I2C1_WaitFlag(I2C_FLAG_RXNE, SET, I2C_TIMEOUT))
            goto err;
        buf[len - 1] = I2C_ReceiveData(I2C1);
    }
    return 0;

err:
    I2C_GenerateSTOP(I2C1, ENABLE);
    return 1;
}
