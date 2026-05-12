#include "bsp_can.h"

/**
  * @brief  初始化 CAN1 (500Kbps)
  * @note   PA11=RX, PA12=TX
  *         36MHz APB1 / 预分频8 / (1+6+2) = 500Kbps
  *         过滤器: 接收所有 ID (IdMask 全 0)
  *         使能 FIFO0 消息挂起中断 (USB_LP_CAN1_RX0_IRQn)
  */
void CAN1_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    CAN_InitTypeDef CAN_InitStructure;
    CAN_FilterInitTypeDef CAN_FilterInitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    /* PA11 = CAN_RX */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* PA12 = CAN_TX */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    CAN_DeInit(CAN1);

    CAN_InitStructure.CAN_TTCM = DISABLE;
    CAN_InitStructure.CAN_ABOM = DISABLE;
    CAN_InitStructure.CAN_AWUM = DISABLE;
    CAN_InitStructure.CAN_NART = DISABLE;
    CAN_InitStructure.CAN_RFLM = DISABLE;
    CAN_InitStructure.CAN_TXFP = DISABLE;
    CAN_InitStructure.CAN_Mode = CAN_Mode_Normal;
    CAN_InitStructure.CAN_SJW = CAN_SJW_1tq;
    CAN_InitStructure.CAN_BS1 = CAN_BS1_6tq;
    CAN_InitStructure.CAN_BS2 = CAN_BS2_2tq;
    CAN_InitStructure.CAN_Prescaler = 8;
    /* 36MHz APB1 / 8 / (1+6+2) = 500Kbps */
    CAN_Init(CAN1, &CAN_InitStructure);

    /* 过滤器: 接收所有 ID */
    CAN_FilterInitStructure.CAN_FilterNumber = 0;
    CAN_FilterInitStructure.CAN_FilterMode = CAN_FilterMode_IdMask;
    CAN_FilterInitStructure.CAN_FilterScale = CAN_FilterScale_32bit;
    CAN_FilterInitStructure.CAN_FilterIdHigh = 0;
    CAN_FilterInitStructure.CAN_FilterIdLow = 0;
    CAN_FilterInitStructure.CAN_FilterMaskIdHigh = 0;
    CAN_FilterInitStructure.CAN_FilterMaskIdLow = 0;
    CAN_FilterInitStructure.CAN_FilterFIFOAssignment = CAN_Filter_FIFO0;
    CAN_FilterInitStructure.CAN_FilterActivation = ENABLE;
    CAN_FilterInit(&CAN_FilterInitStructure);

    /* FIFO0 消息挂起中断 */
    CAN_ITConfig(CAN1, CAN_IT_FMP0, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel = USB_LP_CAN1_RX0_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_Init(&NVIC_InitStructure);
}

/**
  * @brief  发送 CAN 标准帧
  * @param  id  标准 ID (11bit)
  * @param  data  数据指针
  * @param  len  数据长度 (最大 8)
  * @retval 1=发送成功, 0=超时
  */
uint8_t CAN1_SendMsg(uint32_t id, uint8_t *data, uint8_t len)
{
    CanTxMsg TxMessage;
    uint8_t mailbox;
    uint32_t timeout = 0xFFFF;
    uint8_t i;

    TxMessage.StdId = id;
    TxMessage.ExtId = 0;
    TxMessage.IDE = CAN_Id_Standard;
    TxMessage.RTR = CAN_RTR_Data;
    TxMessage.DLC = len > 8 ? 8 : len;
    for (i = 0; i < TxMessage.DLC; i++)
        TxMessage.Data[i] = data[i];

    mailbox = CAN_Transmit(CAN1, &TxMessage);
    while (CAN_TransmitStatus(CAN1, mailbox) != CAN_TxStatus_Ok && timeout--)
        ;

    return (timeout > 0) ? 1 : 0;
}
