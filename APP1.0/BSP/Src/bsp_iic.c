#include "bsp.h"
#include "bsp_iic.h"

static uint8_t IIC_LastError;

#define IIC_DELAY() Delay_us(2)

static void IIC_SetLastError(uint8_t err)
{
    IIC_LastError = err;
}

uint8_t IIC_GetLastError(void)
{
    return IIC_LastError;
}

uint8_t IIC_GetSCL(void)
{
    return GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_6);
}

uint8_t IIC_GetSDA(void)
{
    return GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_7);
}

static void IIC_SDA_OUT(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}

static void IIC_SDA_IN(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    GPIO_SetBits(GPIOB, GPIO_Pin_7);
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}

void IIC_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    IIC_SCL_H;
    IIC_SDA_H;
    IIC_SetLastError(0);
}

uint8_t IIC_CheckDevice(uint8_t addr)
{
    uint8_t ret;

    IIC_SetLastError(0);
    IIC_Start();
    IIC_Send_Byte(addr & 0xFE);
    ret = IIC_wait_Ack(1000);
    IIC_Stop();
    if (ret)
        IIC_SetLastError(0xA0);
    return ret;
}

uint8_t IIC_WriteBuf(uint8_t addr, uint8_t *buf, uint16_t len)
{
    uint16_t i;

    IIC_SetLastError(0);

    if (len == 0)
        return IIC_CheckDevice(addr);

    IIC_Start();
    IIC_Send_Byte(addr & 0xFE);
    if (IIC_wait_Ack(1000))
    {
        IIC_SetLastError(0xA1);
        goto fail;
    }

    for (i = 0; i < len; i++)
    {
        IIC_Send_Byte(buf[i]);
        if (IIC_wait_Ack(1000))
        {
            IIC_SetLastError(0xA2);
            goto fail;
        }
    }

    IIC_Stop();
    return 0;

fail:
    IIC_Stop();
    return 1;
}

uint8_t IIC_ReadBuf(uint8_t addr, uint8_t *buf, uint16_t len)
{
    uint16_t i;

    IIC_SetLastError(0);

    if (len == 0)
        return 0;

    IIC_Start();
    IIC_Send_Byte(addr | 0x01);
    if (IIC_wait_Ack(1000))
    {
        IIC_SetLastError(0xA3);
        IIC_Stop();
        return 1;
    }

    for (i = 0; i < len; i++)
        buf[i] = IIC_Read_Byte(i != (len - 1));

    IIC_Stop();
    return 0;
}

uint8_t IIC_MemWrite(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len)
{
    uint8_t tmp[18];
    uint16_t i;

    if (len > (sizeof(tmp) - 1))
        return 1;

    tmp[0] = reg;
    for (i = 0; i < len; i++)
        tmp[i + 1] = buf[i];

    return IIC_WriteBuf(addr, tmp, len + 1);
}

uint8_t IIC_MemRead(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len)
{
    uint16_t i;

    IIC_SetLastError(0);

    if (len == 0)
        return 0;

    IIC_Start();
    IIC_Send_Byte(addr & 0xFE);
    if (IIC_wait_Ack(1000))
    {
        IIC_SetLastError(0xB1);
        IIC_Stop();
        return 1;
    }

    IIC_Send_Byte(reg);
    if (IIC_wait_Ack(1000))
    {
        IIC_SetLastError(0xB2);
        IIC_Stop();
        return 1;
    }

    IIC_Start();
    IIC_Send_Byte(addr | 0x01);
    if (IIC_wait_Ack(1000))
    {
        IIC_SetLastError(0xB3);
        IIC_Stop();
        return 1;
    }

    for (i = 0; i < len; i++)
        buf[i] = IIC_Read_Byte(i != (len - 1));

    IIC_Stop();
    return 0;
}

/* START: SCL high -> SDA falling edge */
void IIC_Start(void)
{
    IIC_SDA_OUT();
    IIC_SCL_H;
    IIC_SDA_H;
    IIC_DELAY();
    IIC_SDA_L;
    IIC_DELAY();
    IIC_SCL_L;
}

void IIC_Stop(void)
{
    IIC_SDA_OUT();
    IIC_SDA_L;
    IIC_DELAY();
    IIC_SCL_H;
    IIC_DELAY();
    IIC_SDA_H;
    IIC_DELAY();
}

void IIC_Send_Byte(uint8_t txd)
{
    int8_t i;

    IIC_SDA_OUT();
    for (i = 7; i >= 0; i--)
    {
        IIC_SCL_L;
        if (txd & BIT(i))
            IIC_SDA_H;
        else
            IIC_SDA_L;
        IIC_DELAY();
        IIC_SCL_H;
        IIC_DELAY();
    }
    IIC_SCL_L;
    IIC_SDA_H;
    IIC_SDA_IN();
}

uint8_t IIC_wait_Ack(int16_t timeout)
{
    IIC_SDA_IN();
    IIC_SCL_H;
    IIC_DELAY();
    while (READ_SDA && (timeout-- > 0))
        IIC_DELAY();
    IIC_SCL_L;
    IIC_DELAY();
    IIC_SDA_OUT();
    IIC_SDA_H;
    return (timeout > 0) ? 0 : 1;
}

uint8_t IIC_Read_Byte(uint8_t ack)
{
    int8_t i;
    uint8_t rxd;

    rxd = 0;
    IIC_SDA_IN();
    for (i = 7; i >= 0; i--)
    {
        IIC_SCL_L;
        IIC_DELAY();
        IIC_SCL_H;
        IIC_DELAY();
        if (READ_SDA)
            rxd |= BIT(i);
        IIC_DELAY();
    }
    IIC_SCL_L;
    IIC_DELAY();
    IIC_SDA_OUT();
    if (ack)
    {
        IIC_SDA_L;
        IIC_SCL_H;
        IIC_DELAY();
        IIC_SCL_L;
        IIC_SDA_H;
        IIC_DELAY();
    }
    else
    {
        IIC_SDA_H;
        IIC_SCL_H;
        IIC_DELAY();
        IIC_SCL_L;
        IIC_DELAY();
    }
    return rxd;
}
