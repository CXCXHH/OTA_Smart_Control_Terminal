/*
 * FreeModbus Libary: BARE Port
 * Copyright (C) 2006 Christian Walter <wolti@sil.at>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * File: $Id$
 */

#include "port.h"
#include "bsp_gpio.h"
/* ----------------------- Modbus includes ----------------------------------*/
#include "mb.h"
#include "mbport.h"

/* ----------------------- static functions ---------------------------------*/
static void prvvUARTTxReadyISR( void );
static void prvvUARTRxISR( void );

/* ----------------------- Start implementation -----------------------------*/
void
vMBPortSerialEnable( BOOL xRxEnable, BOOL xTxEnable )
{
    if( xRxEnable )
    {
        /* Wait for any ongoing TX to fully complete before switching to RX.
           USART_FLAG_TC is set when TX shift register + DR are both empty. */
        if (!xTxEnable)
        {
            volatile uint32_t t = 0xFFFF;
            while (!USART_GetFlagStatus(USART2, USART_FLAG_TC) && --t);
        }
        RS485_RxMode();
        USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
    }
    else
    {
        USART_ITConfig(USART2, USART_IT_RXNE, DISABLE);
    }

    if( xTxEnable )
    {
        RS485_TxMode();
        USART_ITConfig(USART2, USART_IT_TXE, ENABLE);
    }
    else
    {
        USART_ITConfig(USART2, USART_IT_TXE, DISABLE);
    }
}

BOOL
xMBPortSerialInit( UCHAR ucPORT, ULONG ulBaudRate, UCHAR ucDataBits, eMBParity eParity )
{
    USART_InitTypeDef USART_InitStructure;

    USART_InitStructure.USART_BaudRate = ulBaudRate;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;

    switch(eParity)
    {
    case MB_PAR_ODD:
        USART_InitStructure.USART_Parity = USART_Parity_Odd;
        USART_InitStructure.USART_WordLength = USART_WordLength_9b;
        break;
    case MB_PAR_EVEN:
        USART_InitStructure.USART_Parity = USART_Parity_Even;
        USART_InitStructure.USART_WordLength = USART_WordLength_9b;
        break;
    default:
        USART_InitStructure.USART_Parity = USART_Parity_No;
        USART_InitStructure.USART_WordLength = USART_WordLength_8b;
        break;
    }

    USART_Init(USART2, &USART_InitStructure);
    USART_Cmd(USART2, ENABLE);
    return TRUE;
}

BOOL
xMBPortSerialPutByte( CHAR ucByte )
{
    USART2->DR = ucByte;
    return TRUE;
}

BOOL
xMBPortSerialGetByte( CHAR * pucByte )
{
    *pucByte = (USART2->DR & (uint16_t)0x00FF);
    return TRUE;
}

static void prvvUARTTxReadyISR( void )
{
    pxMBFrameCBTransmitterEmpty(  );
}

static void prvvUARTRxISR( void )
{
    pxMBFrameCBByteReceived(  );
}

void USART2_IRQHandler(void)
{
    if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)
    {
        USART_ClearITPendingBit(USART2, USART_IT_RXNE);
        prvvUARTRxISR();
    }

    if (USART_GetITStatus(USART2, USART_IT_TXE) != RESET)
    {
        USART_ClearITPendingBit(USART2, USART_IT_TXE);
        prvvUARTTxReadyISR();
    }
}
