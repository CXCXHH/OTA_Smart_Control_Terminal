/*******************************************************
 * FileName: boot.c
 * Description: BootLoader startup, command line and IAP logic
 ******************************************************/
#include "main.h"
#include "boot.h"

#define XMODEM_FRAME_LEN        133
#define XMODEM_DATA_LEN         128
#define XMODEM_SOH              0x01
#define XMODEM_EOT              0x04
#define XMODEM_ACK              "\x06"
#define XMODEM_NAK              "\x15"
#define W25Q64_BLOCK_SIZE       (64 * 1024)
#define W25Q64_BLOCK_MIN        '1'
#define W25Q64_BLOCK_MAX        '9'

/* 函数指针：保存用户程序复位入口，赋值后 call 即跳转 */
load_a load_A;

static void BootLoader_HandleIdleCommand(uint8_t cmd);
static void BootLoader_HandleXmodem(uint8_t *data, uint16_t datalen);
static void BootLoader_HandleVersionInput(uint8_t *data, uint16_t datalen);
static void BootLoader_HandleBlockInput(uint8_t *data, uint16_t datalen, uint8_t is_download);
static void BootLoader_StartXmodem(uint8_t to_external_flash, uint8_t block_nb);
static void BootLoader_WriteXmodemCache(uint32_t flash_offset, uint16_t len);
static void BootLoader_WriteExternalBlockData(uint32_t offset, uint8_t *data, uint16_t len);

/*
 * 启动决策：
 *   1. 等待用户按键进入命令行
 *   2. 超时后检查 OTA 标记，有则从外部 Flash 升级 A 区，
 *      无则直接跳转 A 区用户程序
 * 注意：BootLoader_Brance 不返回（跳转后 CPU 执行用户程序）
 */
void BootLoader_Brance(void)
{
    if(BootLoader_Enter(20) == 0)
    {
        if(OTA_Info.OTA_FLAG == OTA_SET_FLAG)
        {
            U1_printf("OTA YES!\r\n");
            BootStaFlag |= UPDATA_A_FLAG;
            UpDataA.W25Q64_BlockNB = 0;
        }
        else
        {
            U1_printf("OTA GO A!\r\n");
            LOAD_A(STM32_A_START_ADDR);
        }
    }

    U1_printf("进入BootLoader命令行\r\n");
    BootLoader_Info();
}

/*
 * timeout: 超时值，单位 100ms。调用传 20 表示等待 2s
 * 返回 1 进入命令行，0 超时继续正常启动
 */
uint8_t BootLoader_Enter(uint8_t timeout)
{
    U1_printf("%ds内，输入小写 w ，进入BootLoader命令行\r\n", timeout / 10);
    while(timeout--)
    {
        Delay_ms(100);
        if(USART1_RxBuf[0] == 'w')
        {
            return 1;
        }
    }
    return 0;
}

void BootLoader_Info(void)
{
    U1_printf("\r\n");
    U1_printf("[1]擦除A区\r\n");
    U1_printf("[2]串口IAP下载A区程序\r\n");
    U1_printf("[3]设置OTA版本号\r\n");
    U1_printf("[4]查询OTA版本号\r\n");
    U1_printf("[5]向外部Flash下载程序\r\n");
    U1_printf("[6]使用外部Flash内程序\r\n");
    U1_printf("[7]重启\r\n");
}

/*
 * 根据 BootStaFlag 状态将输入路由到对应处理函数。
 * 各标志互斥，按优先级顺序判断。
 */
void BootLoader_Event(uint8_t *data, uint16_t datalen)
{
    if(BootStaFlag == 0)
    {
        BootLoader_HandleIdleCommand(data[0]);
    }
    else if(BootStaFlag & IAP_XMODEMD_FLAG)
    {
        BootLoader_HandleXmodem(data, datalen);
    }
    else if(BootStaFlag & SET_VERDION_FLAG)
    {
        BootLoader_HandleVersionInput(data, datalen);
    }
    else if(BootStaFlag & CMD5_FLAG)
    {
        BootLoader_HandleBlockInput(data, datalen, 1);
    }
    else if(BootStaFlag & CMD6_FLAG)
    {
        BootLoader_HandleBlockInput(data, datalen, 0);
    }
}

/*
 * 空闲态命令行：'1'~'7' 对应擦除、IAP、版本管理、
 * 外部 Flash 下载/使用、重启
 */
static void BootLoader_HandleIdleCommand(uint8_t cmd)
{
    switch(cmd)
    {
        case '1':
            U1_printf("擦除A区\r\n");
            Flash_ErasePage(STM32_A_START_ADDR, STM32_A_PAGE_COUNT);
            break;

        case '2':
            BootLoader_StartXmodem(0, 0);
            break;

        case '3':
            U1_printf("设置版本号\r\n");
            BootStaFlag |= SET_VERDION_FLAG;
            break;

        case '4':
            U1_printf("查询版本号\r\n");
            AT24C02_ReadOTAInfo();
            U1_printf("Version:%s\r\n", OTA_Info.OTA_ver);
            BootLoader_Info();
            break;

        case '5':
            U1_printf("Download to external Flash, input block number(1~9)\r\n");
            BootStaFlag |= CMD5_FLAG;
            break;

        case '6':
            U1_printf("Use program in external Flash, input block number(1~9)\r\n");
            BootStaFlag |= CMD6_FLAG;
            break;

        case '7':
            U1_printf("重启\r\n");
            Delay_ms(100);
            NVIC_SystemReset();
            break;

        default:
            break;
    }
}

/*
 * XMODEM 传输启动：
 *   to_external_flash=0 → 下载到内部 Flash A 区（先整片擦除）
 *   to_external_flash=1 → 下载到外部 Flash 指定 Block（先擦除 64KB）
 * 启动后置 XMODEM 数据接收标志，主循环负责发送 'C' 握手。
 */
static void BootLoader_StartXmodem(uint8_t to_external_flash, uint8_t block_nb)
{
    UpDataA.XmodemTimer = 0;
    UpDataA.XmodemNB = 0;
    BootStaFlag |= (IAP_XMODEMC_FLAG | IAP_XMODEMD_FLAG);

    if(to_external_flash)
    {
        UpDataA.W25Q64_BlockNB = block_nb;
        OTA_Info.Firelen[block_nb] = 0;
        BootStaFlag |= CMD5_XMODEM_FLAG;
        BootStaFlag &= ~CMD5_FLAG;
        W25Q64_EraseBlock64K(block_nb * W25Q64_BLOCK_SIZE);
        U1_printf("XMODEM download to external Flash block %d, use bin file\r\n", block_nb);
    }
    else
    {
        U1_printf("XMODEM download to A, use bin file\r\n");
        Flash_ErasePage(STM32_A_START_ADDR, STM32_A_PAGE_COUNT);
    }
}

/*
 * XMODEM 数据/结束处理：
 *   SOH(133B) → CRC-CCITT 校验 → 缓存到 Updatabuff → 满页刷写
 *   EOT(1B)   → 刷写剩余数据，外部下载模式保存长度，否则复位
 * CRC 失败回复 NAK 不推进序号，发送方自动重发当前包。
 */
static void BootLoader_HandleXmodem(uint8_t *data, uint16_t datalen)
{
    uint16_t recv_crc;
    uint16_t remain_len;
    uint32_t flash_offset;

    if((datalen == XMODEM_FRAME_LEN) && (data[0] == XMODEM_SOH))
    {
        BootStaFlag &= ~IAP_XMODEMC_FLAG;
        UpDataA.XmodemCRC = Xmodem_CRC16(&data[3], XMODEM_DATA_LEN);
        recv_crc = ((uint16_t)data[131] << 8) | data[132];

        if(UpDataA.XmodemCRC != recv_crc)
        {
            U1_printf(XMODEM_NAK);
            return;
        }

        UpDataA.XmodemNB++;
        memcpy(&UpDataA.Updatabuff[((UpDataA.XmodemNB - 1) % (FLASH_PAGE_SIZE / XMODEM_DATA_LEN)) * XMODEM_DATA_LEN],
               &data[3],
               XMODEM_DATA_LEN);

        if((UpDataA.XmodemNB % (FLASH_PAGE_SIZE / XMODEM_DATA_LEN)) == 0)
        {
            flash_offset = ((UpDataA.XmodemNB / (FLASH_PAGE_SIZE / XMODEM_DATA_LEN)) - 1) * FLASH_PAGE_SIZE;
            BootLoader_WriteXmodemCache(flash_offset, FLASH_PAGE_SIZE);
        }

        U1_printf(XMODEM_ACK);
        return;
    }

    if((datalen == 1) && (data[0] == XMODEM_EOT))
    {
        BootStaFlag &= ~IAP_XMODEMC_FLAG;
        U1_printf(XMODEM_ACK);

        remain_len = (UpDataA.XmodemNB % (FLASH_PAGE_SIZE / XMODEM_DATA_LEN)) * XMODEM_DATA_LEN;
        if(remain_len != 0)
        {
            flash_offset = (UpDataA.XmodemNB / (FLASH_PAGE_SIZE / XMODEM_DATA_LEN)) * FLASH_PAGE_SIZE;
            BootLoader_WriteXmodemCache(flash_offset, remain_len);
        }

        BootStaFlag &= ~IAP_XMODEMD_FLAG;
        if(BootStaFlag & CMD5_XMODEM_FLAG)
        {
            OTA_Info.Firelen[UpDataA.W25Q64_BlockNB] = UpDataA.XmodemNB * XMODEM_DATA_LEN;
            AT24C02_WriteOTAInfo();
            BootStaFlag &= ~(CMD5_FLAG | CMD5_XMODEM_FLAG);
            BootLoader_Info();
        }
        else
        {
            Delay_ms(100);
            NVIC_SystemReset();
        }
    }
}

/*
 * 版本号设置：
 * 定长 26 字节，格式 VER-X.Y.Z-YYYY/MM/DD-HH:MM，
 * 校验通过后写入 AT24C02 持久化，重启后可通过 [4] 查询。
 */
static void BootLoader_HandleVersionInput(uint8_t *data, uint16_t datalen)
{
    int temp;

    if(datalen != 26)
    {
        U1_printf("版本号长度错误\r\n");
        return;
    }

    if(sscanf((char *)data, "VER-%d.%d.%d-%d/%d/%d-%d:%d",
              &temp, &temp, &temp, &temp, &temp, &temp, &temp, &temp) != 8)
    {
        U1_printf("版本号格式错误\r\n");
        return;
    }

    memset(OTA_Info.OTA_ver, 0, 32);
    memcpy(OTA_Info.OTA_ver, data, 26);
    AT24C02_WriteOTAInfo();
    U1_printf("版本号正确\r\n");
    BootStaFlag &= ~SET_VERDION_FLAG;
    BootLoader_Info();
}

/*
 * [5]/[6] 共用输入处理：
 *   is_download=1 → 启动 XMODEM 下载到外部 Flash
 *   is_download=0 → 设 UPDATA_A_FLAG，主循环搬运固件到 A 区
 */
static void BootLoader_HandleBlockInput(uint8_t *data, uint16_t datalen, uint8_t is_download)
{
    if(datalen != 1)
    {
        U1_printf("Data长度错误\r\n");
        return;
    }

    if((data[0] < W25Q64_BLOCK_MIN) || (data[0] > W25Q64_BLOCK_MAX))
    {
        U1_printf("编号错误\r\n");
        return;
    }

    UpDataA.W25Q64_BlockNB = data[0] - '0';
    if(is_download)
    {
        BootLoader_StartXmodem(1, UpDataA.W25Q64_BlockNB);
    }
    else
    {
        BootStaFlag |= UPDATA_A_FLAG;
        BootStaFlag &= ~CMD6_FLAG;
    }
}

/*
 * XMODEM 缓存刷写路由：
 *   CMD5_XMODEM_FLAG 置位 → 写入外部 Flash
 *   否则 → 写入内部 Flash A 区
 */
static void BootLoader_WriteXmodemCache(uint32_t flash_offset, uint16_t len)
{
    if(BootStaFlag & CMD5_XMODEM_FLAG)
    {
        BootLoader_WriteExternalBlockData(flash_offset, UpDataA.Updatabuff, len);
    }
    else
    {
        Flash_WriteBuffer(STM32_A_START_ADDR + flash_offset, UpDataA.Updatabuff, len);
    }
}

/*
 * 按页边界分段写入外部 Flash。
 * W25Q64 PageProgram 单次最多 256 字节，超长自动拆分轮询。
 */
static void BootLoader_WriteExternalBlockData(uint32_t offset, uint8_t *data, uint16_t len)
{
    uint16_t write_len;

    while(len)
    {
        write_len = (len > W25Q64_PAGE_SIZE) ? W25Q64_PAGE_SIZE : len;
        W25Q64_PageProgram(UpDataA.W25Q64_BlockNB * W25Q64_BLOCK_SIZE + offset, data, write_len);
        offset += write_len;
        data += write_len;
        len -= write_len;
    }
}

/*
 * XMODEM CRC-CCITT (多项式 0x1021, 初值 0x0000)
 */
uint16_t Xmodem_CRC16(uint8_t *data, uint16_t datalen)
{
    uint16_t crc = 0;
    uint16_t i;
    uint8_t j;

    for(i = 0; i < datalen; i++)
    {
        crc ^= (uint16_t)data[i] << 8;
        for(j = 0; j < 8; j++)
        {
            if(crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}

/*
 * 设置主栈指针 MSP 为应用栈顶。
 * Cortex-M3 内核指令，等效 __set_MSP()。
 * 必须在跳转应用前调用，否则中断压栈使用 Bootloader 栈空间。
 */
__asm void MSR_SP(uint32_t addr)
{
    MSR MSP, r0
    BX r14
}

/*
 * 跳转 A 区用户程序：
 *   1. 校验栈顶地址在 SRAM 范围，防止无效指针
 *   2. 切栈为用户程序栈
 *   3. 读 Reset_Handler 入口地址
 *   4. 关闭 Bootloader 用过的外设，防止中断冲突
 *   5. 跳转，不再返回
 */
void LOAD_A(uint32_t addr)
{
    if((*(uint32_t *)addr >= 0x20000000) && (*(uint32_t *)addr <= 0x20004FFF))
    {
        MSR_SP(*(uint32_t *)addr);
        load_A = (load_a)*(uint32_t *)(addr + 4);
        BootLoader_Clear();
        load_A();
    }
    else
    {
        U1_printf("Jump A faild!\r\n");
    }
}

/*
 * 跳转前必须复位 Bootloader 用过的外设和中断。
 * 否则用户程序未初始化外设时误触发中断，会从用户向量表
 * 读到错误入口地址导致 HardFault。
 */
void BootLoader_Clear(void)
{
    USART_DeInit(USART1);
    GPIO_DeInit(GPIOA);
    GPIO_DeInit(GPIOB);
}
