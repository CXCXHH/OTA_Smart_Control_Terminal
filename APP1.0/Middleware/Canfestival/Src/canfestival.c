#include "stm32f10x.h"
#include "canfestival.h"
#include "TestSlave.h"
#include <string.h>

/* 未使用的 CanFestival 模块回调存根 */
#include "emcy.h"
#include "sync.h"

void _post_TPDO(CO_Data *d) { (void)d; }
void _post_emcy(CO_Data *d, UNS8 nodeID, UNS16 errCode, UNS8 errReg) { (void)d; (void)nodeID; (void)errCode; (void)errReg; }
void _post_sync(CO_Data *d) { (void)d; }
void emergencyInit(CO_Data *d) { (void)d; }
void emergencyStop(CO_Data *d) { (void)d; }
UNS8 masterSendNMTstateChange(CO_Data *d, UNS8 nodeID, UNS8 state) { (void)d; (void)nodeID; (void)state; return 0; }
UNS8 masterSendNMTnodeguard(CO_Data *d, UNS8 nodeId) { (void)d; (void)nodeId; return 0; }
void proceedEMCY(CO_Data *d, Message *m) { (void)d; (void)m; }
UNS8 proceedSYNC(CO_Data *d) { (void)d; return 0; }
void startSYNC(CO_Data *d) { (void)d; }
void stopSYNC(CO_Data *d) { (void)d; }

uint8_t CanFestival_Can_Init(void)
{
    CAN_ITConfig(CAN1, CAN_IT_FMP0, ENABLE);
    return 0;
}

uint8_t canSend(CAN_PORT notused, Message *message)
{
    CanTxMsg TxMessage;
    uint32_t timeout = 0xFFFF;

    TxMessage.StdId = message->cob_id;
    TxMessage.ExtId = 0;
    TxMessage.IDE = CAN_Id_Standard;
    TxMessage.RTR = message->rtr ? CAN_RTR_Remote : CAN_RTR_Data;
    TxMessage.DLC = message->len > 8 ? 8 : message->len;
    memcpy(TxMessage.Data, message->data, message->len);

    uint8_t mailbox = CAN_Transmit(CAN1, &TxMessage);
    while (CAN_TransmitStatus(CAN1, mailbox) != CAN_TxStatus_Ok && timeout--);

    return (timeout > 0) ? 0 : 0xFF;
}

void setTimer(TIMEVAL value)
{
    TIM2->ARR = TIM2->CNT + value;
}

TIMEVAL getElapsedTime(void)
{
    return TIM2->CNT;
}
