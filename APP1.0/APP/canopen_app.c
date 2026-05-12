#include "canopen_app.h"
#include "canfestival.h"
#include "TestSlave.h"
#include "modbus_app.h"
#include "bsp_gpio.h"
#include "stm32f10x.h"

/* CANopen is primarily interrupt-driven: the CAN RX ISR calls canDispatch(),
   which routes messages to the object dictionary and protocol handlers.
   This module wraps the one-time hardware/stack init and any periodic
   maintenance (e.g. heartbeat, lifeguard) into clean APP-layer calls. */

static volatile uint8_t g_canopen_pending_control_valid;
static volatile uint8_t g_canopen_pending_control;
static volatile uint8_t g_canopen_pending_slave_addr_valid;
static volatile uint8_t g_canopen_pending_slave_addr;

void Canopen_RequestApplyControl(uint8_t control)
{
    g_canopen_pending_control = control;
    g_canopen_pending_control_valid = 1;
}

void Canopen_RequestApplySlaveAddr(uint8_t slave_addr)
{
    g_canopen_pending_slave_addr = slave_addr;
    g_canopen_pending_slave_addr_valid = 1;
}

static void Canopen_ApplyPendingRequests(void)
{
    uint8_t control_valid;
    uint8_t slave_addr_valid;
    uint8_t control;
    uint8_t slave_addr;
    uint16_t hold0 = 0;
    uint8_t need_output_refresh = 0;

    control_valid = g_canopen_pending_control_valid;
    slave_addr_valid = g_canopen_pending_slave_addr_valid;
    if (!control_valid && !slave_addr_valid)
        return;

    control = g_canopen_pending_control;
    slave_addr = g_canopen_pending_slave_addr;
    g_canopen_pending_control_valid = 0;
    g_canopen_pending_slave_addr_valid = 0;

    REG_Lock();
    if (control_valid)
    {
        hold0 = (uint16_t)((REG_HOLD_BUF[REG_IDX_OUTPUT] & 0xFF00u) | control);
        REG_HOLD_BUF[REG_IDX_OUTPUT] = hold0;
        need_output_refresh = 1;
    }
    if (slave_addr_valid)
    {
        REG_HOLD_BUF[REG_IDX_SLAVE_ADDR] =
            (uint16_t)((REG_HOLD_BUF[REG_IDX_SLAVE_ADDR] & 0xFF00u) | slave_addr);
        Modify_SlaveAdress_Flag = 1;
    }
    REG_Unlock();

    if (need_output_refresh)
        Output_Control(hold0);
}

static void Canopen_SyncDeviceInfoSnapshot(void)
{
    uint16_t hold_output;
    uint16_t hold_temp;
    uint16_t hold_humi;
    uint16_t hold_dev_volt;
    uint16_t hold_dev_curr;
    uint16_t hold_dev_power;
    uint16_t hold_sys_volt;
    uint16_t hold_cpu_temp;
    uint16_t hold_reserved;
    uint16_t hold_slave_addr;

    REG_Lock();
    hold_output = REG_HOLD_BUF[REG_IDX_OUTPUT];
    hold_temp = REG_HOLD_BUF[REG_IDX_TEMP];
    hold_humi = REG_HOLD_BUF[REG_IDX_HUMI];
    hold_dev_volt = REG_HOLD_BUF[REG_IDX_DEV_VOLT];
    hold_dev_curr = REG_HOLD_BUF[REG_IDX_DEV_CURR];
    hold_dev_power = REG_HOLD_BUF[REG_IDX_DEV_POWER];
    hold_sys_volt = REG_HOLD_BUF[REG_IDX_SYS_VOLT];
    hold_cpu_temp = REG_HOLD_BUF[REG_IDX_CPU_TEMP];
    hold_reserved = REG_HOLD_BUF[8];
    hold_slave_addr = REG_HOLD_BUF[REG_IDX_SLAVE_ADDR];
    REG_Unlock();

    DeviceInfo_Control = (UNS8)(hold_output & 0x00FFu);
    DeviceInfo_TP = (UNS16)hold_temp;
    DeviceInfo_RH = (UNS16)hold_humi;
    DeviceInfo_VL = (UNS16)hold_dev_volt;
    DeviceInfo_CU = (UNS16)hold_dev_curr;
    DeviceInfo_PW = (UNS16)hold_dev_power;
    DeviceInfo_VR = (UNS16)hold_sys_volt;
    DeviceInfo_CPU = (UNS16)hold_cpu_temp;
    DeviceInfo_RES = (UNS16)hold_reserved;
    DeviceInfo_SlaveAddress = (UNS8)(hold_slave_addr & 0x00FFu);
}

void Canopen_Init(void)
{
    CanFestival_Can_Init();
    setNodeId(&TestSlave_Data, 1);
    setState(&TestSlave_Data, Initialisation);
    setState(&TestSlave_Data, Operational);
    Canopen_SyncDeviceInfoSnapshot();
    /* Start TIM2 timer interrupt — must be after Canfestival init */
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
    NVIC_EnableIRQ(TIM2_IRQn);
}

void Canopen_Process(void)
{
    static uint8_t heartbeat_ticks = 0;

    Canopen_ApplyPendingRequests();
    Canopen_SyncDeviceInfoSnapshot();
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
