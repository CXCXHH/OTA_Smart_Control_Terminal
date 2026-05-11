#include "canopen_app.h"
#include "canfestival.h"
#include "TestSlave.h"
#include "stm32f10x.h"

/* CANopen is primarily interrupt-driven: the CAN RX ISR calls canDispatch(),
   which routes messages to the object dictionary and protocol handlers.
   This module wraps the one-time hardware/stack init and any periodic
   maintenance (e.g. heartbeat, lifeguard) into clean APP-layer calls. */

void Canopen_Init(void)
{
    CanFestival_Can_Init();
    setNodeId(&TestSlave_Data, 1);
    setState(&TestSlave_Data, Initialisation);
    setState(&TestSlave_Data, Operational);
    /* Start TIM2 timer interrupt — must be after Canfestival init */
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
    NVIC_EnableIRQ(TIM2_IRQn);
}

void Canopen_Process(void)
{
    static uint8_t heartbeat_ticks = 0;

    heartbeat_ticks++;
    if (heartbeat_ticks >= 10)
    {
        Message heartbeat = Message_Initializer;

        heartbeat_ticks = 0;
        heartbeat.cob_id = 0x700 + getNodeId(&TestSlave_Data);
        heartbeat.len = 1;
        heartbeat.data[0] = getState(&TestSlave_Data);
        canSend(TestSlave_Data.canHandle, &heartbeat);
    }
}
