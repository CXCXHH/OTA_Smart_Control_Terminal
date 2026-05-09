#include "bsp.h"
#include "bsp_iic.h"

void IIC_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
    /*IIC初始化要求SCL和SDA置高*/
    IIC_SCL_H;
    IIC_SDA_H;
}

/*START条件：SCL高电平期间，SDA 产生下降沿*/
void IIC_Start(void)
{
    IIC_SCL_H;
    IIC_SDA_H;
    Delay_us(2);
    IIC_SDA_L;
    /*这里是防止在SCL高电平期间SDA发生跳变导致重复启动 起始信号*/
    Delay_us(2);
    IIC_SCL_L; //SCL拉低，准备发送数据
}

void IIC_Stop(void)
{
    IIC_SDA_L;
    Delay_us(2);
    IIC_SCL_H;
    Delay_us(2);
    IIC_SDA_H;
    Delay_us(2);
}

void IIC_Send_Byte(uint8_t txd)
{
    int8_t i;
    for(i = 7;i >= 0;i--)
    {
        /*必须要在SCL低电平的情况下才能改变SDA*/
        IIC_SCL_L;
        if(txd & BIT(i))
        {
            IIC_SDA_H;
        }
        else
        {
            IIC_SDA_L;
        }
        Delay_us(2);
        IIC_SCL_H; //SCL高，从机采样SDA
        Delay_us(2);
    }
    IIC_SCL_L;
    IIC_SDA_H;  //释放SDA，准备接收ACK
}

uint8_t IIC_wait_Ack(int16_t timeout)
{
    IIC_SCL_H;
    Delay_us(2);
    while((READ_SDA) && (timeout-- > 0))
        Delay_us(2);
    IIC_SCL_L;
    Delay_us(2);
    return (timeout > 0) ? 0 : 1;
}

uint8_t IIC_Read_Byte(uint8_t ack)
{
    int8_t i;
    uint8_t rxd;

    rxd = 0;
    for(i = 7;i >= 0;i--)
    {
        IIC_SCL_L;
        Delay_us(2);
        IIC_SCL_H;
        Delay_us(2);
        if(READ_SDA)
        {
            rxd |= BIT(i);
        }
        Delay_us(2);
    }
    IIC_SCL_L;
    Delay_us(2);
    if(ack)
    {
        IIC_SDA_L;
        IIC_SCL_H;
        Delay_us(2);
        IIC_SCL_L;
        IIC_SDA_H;
        Delay_us(2);
    }
    else
    {
        IIC_SDA_H;
        IIC_SCL_H;
        Delay_us(2);
        IIC_SCL_L;
        Delay_us(2);
    }
    return rxd;
}

