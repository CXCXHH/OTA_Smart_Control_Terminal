/**
  ******************************************************************************
  * @file    Project/STM32F10x_StdPeriph_Template/stm32f10x_it.c
  * @author  MCD Application Team
  * @version V3.5.0
  * @date    08-April-2011
  * @brief   Main Interrupt Service Routines.
  *          This file provides template for all exceptions handler and
  *          peripherals interrupt service routine.
  ******************************************************************************
  * @attention
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 2011 STMicroelectronics</center></h2>
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x_it.h"
#include "bsp.h"
#include "fifo.h"
#include "canfestival.h"
#include "TestSlave.h"

/** @addtogroup STM32F10x_StdPeriph_Template
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/******************************************************************************/
/*            Cortex-M3 Processor Exceptions Handlers                         */
/******************************************************************************/

/**
  * @brief  USART1空闲中断处理函数
  * @param  None
  * @retval None
  * @note   USART1接收使用DMA搬运数据，检测到空闲中断后认为一帧数据接收结束，
  *         在这里统计本帧长度、记录本帧缓存范围，并重新配置DMA开始接收下一帧
  */
void USART1_IRQHandler(void)
{
  /* 这里只处理USART1的空闲中断。
     当串口在一个字符时间内没有继续收到数据时，会产生IDLE中断，
     这里把它作为一帧数据接收完成的标志。 */
  if(USART_GetITStatus(USART1, USART_IT_IDLE) != 0)
  {
    /* 清除空闲中断标志时，必须先读SR再读DR，
       否则USART_IT_IDLE不会被硬件正确清除 */
    (void)USART1->SR;
    (void)USART1->DR;

    /* DMA配置的总接收长度 - 当前DMA剩余长度 = 本次已经接收到的字节数
       将这一帧接收到的字节数累加到URxCounter中，得到当前写入到缓冲区的位置 */
    U0CB.URxCounter += (U1_RX_MAX + 1) - DMA_GetCurrDataCounter(DMA1_Channel5);

    /* 记录当前这一帧数据的结束地址，后续就可以通过start和end取出完整一帧。 */
    U0CB.URxDataIN->end = &USART1_RxBuf[U0CB.URxCounter - 1];

    /* 当前接收帧描述符已填写完成，切换到下一个帧描述符 */
    U0CB.URxDataIN++;

    /* 当写指针到达描述符数组尾部时，回到数组起始位置，形成环形队 */
    if(U0CB.URxDataIN == U0CB.URxDataEND)
    {
      U0CB.URxDataIN = &U0CB.UrxDataPtr[0];
    }


    /* 如果接收缓冲区尾部剩余空间足够，就让下一帧从当前偏移后继续存放 */
    if(U1_RX_SIZE - U0CB.URxCounter >= U1_RX_MAX)
    {
      U0CB.URxDataIN->start = &USART1_RxBuf[U0CB.URxCounter];
    }
    else
    {
      /* 尾部空间不足时，下一帧从缓冲区起始地址重新开始，
         同时将URxCounter清零，重新开始一轮缓冲区使用 */
      U0CB.URxDataIN->start = USART1_RxBuf;
      U0CB.URxCounter = 0;
    }

    /* 关闭DMA通道后，按照新的起始地址重新初始化接收DMA，
       这样下一帧数据就会继续写入新的缓存区域 */
    DMA_Cmd(DMA1_Channel5, DISABLE);
    Usart1_DMA_Rx_Init(U0CB.URxDataIN->start);
  }
}

/**
  * @brief  This function handles NMI exception.
  * @param  None
  * @retval None
  */
void NMI_Handler(void)
{
}

/**
  * @brief  This function handles Hard Fault exception.
  * @param  None
  * @retval None
  */
void HardFault_Handler(void)
{
  /* Go to infinite loop when Hard Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Memory Manage exception.
  * @param  None
  * @retval None
  */
void MemManage_Handler(void)
{
  /* Go to infinite loop when Memory Manage exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Bus Fault exception.
  * @param  None
  * @retval None
  */
void BusFault_Handler(void)
{
  /* Go to infinite loop when Bus Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Usage Fault exception.
  * @param  None
  * @retval None
  */
void UsageFault_Handler(void)
{
  /* Go to infinite loop when Usage Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  Debug Monitor exception.
  */
void DebugMon_Handler(void)
{
}

/******************************************************************************/
/*                 STM32F10x Peripherals Interrupt Handlers                   */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/*  file (startup_stm32f10x_xx.s).                                            */
/******************************************************************************/

/******************************************************************************/
/*                 STM32F10x Peripherals Interrupt Handlers                   */
/******************************************************************************/

/* USART2_IRQHandler 在 Protocol/FreeModbus/portserial.c 中定义 */
/* TIM3_IRQHandler    在 Protocol/FreeModbus/porttimer.c 中定义    */
void USART3_IRQHandler(void)
{
    if (USART_GetITStatus(USART3, USART_IT_RXNE) != RESET)
    {
        uint8_t c = USART_ReceiveData(USART3);
        extern FIFO_t UART3_FIFO;
        FIFO_Push(&UART3_FIFO, c);
    }
}

/**
  * @}
  */

/******************* (C) COPYRIGHT 2011 STMicroelectronics *****END OF FILE****/

/**
  * @brief  TIM2 update interrupt — drives CanFestival timer tick.
  * @note   TIM2 overflows every 65535 us (72 MHz / 72 = 1 MHz, period 0xFFFF).
  *         TimeDispatch() processes alarm queue, heartbeat, lifeguard, etc.
  */
void TIM2_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
        extern void TimeDispatch(void);
        TimeDispatch();
    }
}

void USB_LP_CAN1_RX0_IRQHandler(void)
{
	CanRxMsg RxMsg;
	extern void canDispatch(CO_Data*, Message*);
	extern CO_Data TestSlave_Data;

	if (CAN_GetITStatus(CAN1, CAN_IT_FMP0) != RESET)
	{
		Message m;
		CAN_Receive(CAN1, CAN_FIFO0, &RxMsg);
		m.cob_id = RxMsg.StdId;
		m.rtr = (RxMsg.RTR == CAN_RTR_Remote) ? 1 : 0;
		m.len = RxMsg.DLC;
		uint8_t i;
		for (i = 0; i < 8; i++) m.data[i] = RxMsg.Data[i];
		canDispatch(&TestSlave_Data, &m);
	}
}
