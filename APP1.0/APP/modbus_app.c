#include "modbus_app.h"
#include "mb.h"
#include "mbport.h"
#include "bsp_gpio.h"
#include "bsp.h"

uint16_t REG_HOLD_BUF[REG_HOLD_SIZE];
volatile uint8_t Modify_SlaveAdress_Flag = 0;

/* Coils: 0=LED1, 1=LED2, 2=BEEP, 3=RELAY */
#define REG_COILS_SIZE  10
static uint8_t REG_COILS_BUF[REG_COILS_SIZE];

static uint8_t ucSlaveAddr;

/* ------- Coils (FC1/5/15) - primary output control, matches reference ------- */
eMBErrorCode eMBRegCoilsCB(UCHAR *pucRegBuffer, USHORT usAddress,
                            USHORT usNCoils, eMBRegisterMode eMode)
{
    USHORT usRegIndex = usAddress - 1;
    UCHAR ucBits, ucState, ucLoops;

    if((usRegIndex + usNCoils) > REG_COILS_SIZE)
        return MB_ENOREG;

    if(eMode == MB_REG_WRITE)
    {
        ucLoops = (usNCoils - 1) / 8 + 1;
        while(ucLoops != 0)
        {
            ucState = *pucRegBuffer++;
            ucBits  = 0;
            while(usNCoils != 0 && ucBits < 8)
            {
                REG_COILS_BUF[usRegIndex] = (ucState >> ucBits) & 0x01;
                U1_printf("MB WR COIL[%d]=%d\r\n", usRegIndex + 1, REG_COILS_BUF[usRegIndex]);
                usRegIndex++;
                usNCoils--;
                ucBits++;
            }
            ucLoops--;
        }
    }
    else
    {
        ucLoops = (usNCoils - 1) / 8 + 1;
        while(ucLoops != 0)
        {
            ucState = 0;
            ucBits  = 0;
            while(usNCoils != 0 && ucBits < 8)
            {
                if(REG_COILS_BUF[usRegIndex])
                    ucState |= (1 << ucBits);
                usNCoils--;
                usRegIndex++;
                ucBits++;
            }
            *pucRegBuffer++ = ucState;
            ucLoops--;
        }
    }
    return MB_ENOERR;
}

eMBErrorCode eMBRegDiscreteCB(UCHAR *pucRegBuffer, USHORT usAddress,
                               USHORT usNDiscrete)
{ return MB_ENOREG; }

eMBErrorCode eMBRegInputCB(UCHAR *pucRegBuffer, USHORT usAddress,
                            USHORT usNRegs)
{
    USHORT usRegIndex = usAddress - 1;
    if((usRegIndex + usNRegs) > REG_INPUT_SIZE) return MB_ENOREG;
    while(usNRegs > 0)
    {
        *pucRegBuffer++ = (UCHAR)(REG_HOLD_BUF[usRegIndex] >> 8);
        *pucRegBuffer++ = (UCHAR)(REG_HOLD_BUF[usRegIndex] & 0xFF);
        usRegIndex++; usNRegs--;
    }
    return MB_ENOERR;
}

/* Holding Registers: [0]=output bits, [1]=T*100, [2]=RH*100, [3]=V*100,
   [4]=I, [5]=P, [6]=AV, [7]=AT, [9]=SlaveAddr */
eMBErrorCode eMBRegHoldingCB(UCHAR *pucRegBuffer, USHORT usAddress,
                              USHORT usNRegs, eMBRegisterMode eMode)
{
    USHORT usRegIndex = usAddress - 1;

    if((usRegIndex + usNRegs) > REG_HOLD_SIZE)
        return MB_ENOREG;

    if(eMode == MB_REG_WRITE)
    {
        while(usNRegs > 0)
        {
            REG_HOLD_BUF[usRegIndex] = (pucRegBuffer[0] << 8) | pucRegBuffer[1];
            U1_printf("MB WR HOLD[%d]=%04X\r\n", usRegIndex + 1, REG_HOLD_BUF[usRegIndex]);
            pucRegBuffer += 2;
            usRegIndex++;
            usNRegs--;
        }
        if(usAddress == 10) Modify_SlaveAdress_Flag = 1;
    }
    else
    {
        while(usNRegs > 0)
        {
            *pucRegBuffer++ = (UCHAR)(REG_HOLD_BUF[usRegIndex] >> 8);
            *pucRegBuffer++ = (UCHAR)(REG_HOLD_BUF[usRegIndex] & 0xFF);
            usRegIndex++;
            usNRegs--;
        }
    }
    return MB_ENOERR;
}

void Modbus_Init(uint8_t slave_addr)
{
    ucSlaveAddr = slave_addr;
    eMBInit(MB_RTU, ucSlaveAddr, 0, 115200, MB_PAR_NONE);
    eMBEnable();
}

/* Match reference: poll outputs from REG_HOLD_BUF[0] bits (secondary)
   AND coils REG_COILS_BUF (primary). Coils override holding bits. */
void Modbus_Parse(void)
{
    /* Coils control individual outputs (primary, FC5) */
    LED1_Control(REG_COILS_BUF[0] || (REG_HOLD_BUF[0] & LED1_CMD));
    LED2_Control(REG_COILS_BUF[1] || (REG_HOLD_BUF[0] & LED2_CMD));
    BEEP_Control(REG_COILS_BUF[2]  || (REG_HOLD_BUF[0] & BEEP_CMD));
    RELAY_Control(REG_COILS_BUF[3] || (REG_HOLD_BUF[0] & RELAY_CMD));
}

void Modbus_Task(void)
{
    eMBPoll();
    Modbus_Parse();
}
