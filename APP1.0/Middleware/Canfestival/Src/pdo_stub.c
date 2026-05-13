#include "pdo.h"

void _RxPDO_EventTimers_Handler(CO_Data *d, UNS32 pdoNum)
{
    (void)d;
    (void)pdoNum;
}

UNS8 buildPDO(CO_Data* d, UNS8 numPdo, Message *pdo)
{
    (void)d;
    (void)numPdo;
    (void)pdo;
    return 0xFF;
}

UNS8 sendPDOrequest(CO_Data* d, UNS16 RPDOIndex)
{
    (void)d;
    (void)RPDOIndex;
    return 0xFF;
}

UNS8 proceedPDO(CO_Data* d, Message *m)
{
    (void)d;
    (void)m;
    return 0;
}

UNS8 sendPDOevent(CO_Data* d)
{
    (void)d;
    return 0;
}

UNS8 sendOnePDOevent(CO_Data* d, UNS8 pdoNum)
{
    (void)d;
    (void)pdoNum;
    return 0;
}

UNS8 _sendPDOevent(CO_Data* d, UNS8 isSyncEvent)
{
    (void)d;
    (void)isSyncEvent;
    return 0;
}

void PDOInit(CO_Data* d)
{
    (void)d;
}

void PDOStop(CO_Data* d)
{
    (void)d;
}

void PDOEventTimerAlarm(CO_Data* d, UNS32 pdoNum)
{
    (void)d;
    (void)pdoNum;
}

void PDOInhibitTimerAlarm(CO_Data* d, UNS32 pdoNum)
{
    (void)d;
    (void)pdoNum;
}

void CopyBits(UNS8 NbBits, UNS8* SrcByteIndex, UNS8 SrcBitIndex,
              UNS8 SrcBigEndian, UNS8* DestByteIndex, UNS8 DestBitIndex,
              UNS8 DestBigEndian)
{
    (void)NbBits;
    (void)SrcByteIndex;
    (void)SrcBitIndex;
    (void)SrcBigEndian;
    (void)DestByteIndex;
    (void)DestBitIndex;
    (void)DestBigEndian;
}
