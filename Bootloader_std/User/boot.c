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
/* W25Q64 块号输入范围（ASCII '1'~'9'） */
#define W25Q64_BLOCK_MIN        '1'
#define W25Q64_BLOCK_MAX        '9'

/* 启动等待超时：BootLoader_Enter 参数单位 100ms，传 100 即 10s */
#define BOOT_ENTER_TIMEOUT_MS100 100U

/* 串口命令进入命令行的首字节 */
#define BOOT_START_CMD          0xFF

/* 按键返回值：无操作 / ESC 选择 block1 / OK 选择 block2 */
#define BOOT_KEY_NONE           0
#define BOOT_KEY_BLOCK1         1
#define BOOT_KEY_BLOCK2         2

/* XMODEM 133 字节帧中 CRC 校验码的大端字节偏移 */
#define XMODEM_CRC_OFFSET_HIGH  131U
#define XMODEM_CRC_OFFSET_LOW   132U

/* 函数指针：保存用户程序复位入口，赋值后 call 即跳转 */
load_a load_A;

static void BootLoader_KeyInit(void);
static uint8_t BootLoader_ReadKeyBlock(void);
static void BootLoader_HandleIdleCommand(uint8_t cmd);
static void BootLoader_HandleXmodem(uint8_t *data, uint16_t datalen);
static void BootLoader_HandleVersionInput(uint8_t *data, uint16_t datalen);
static void BootLoader_HandleBlockInput(uint8_t *data, uint16_t datalen, uint8_t is_download);
static void BootLoader_StartXmodem(uint8_t to_external_flash, uint8_t block_nb);
static void BootLoader_WriteXmodemCache(uint32_t flash_offset, uint16_t len);
static void BootLoader_WriteExternalBlockData(uint32_t offset, uint8_t *data, uint16_t len);
static uint8_t BootLoader_IsExternalBlockLengthValid(uint32_t file_len);

/*
 * 启动决策（三选一）：
 *
 * 路径1 — 超时 & OTA 标记置位
 *   从外部 Flash 升级 A 区固件（走 BootLoader_UpdateAFromExternalFlash）
 *
 * 路径2 — 超时 & 无 OTA 标记
 *   直接跳转 A 区用户程序（LOAD_A），不再进入命令行
 *
 * 路径3 — 按键 (ESC/OK) 选择外部 Flash 块号
 *   设 UpDataA.W25Q64_BlockNB 并立即从该 Block 搬运固件到 A 区，
 *   完成后自动复位（BootLoader_UpdateAFromExternalFlash 内部 NVIC_SystemReset）
 *
 * 路径1/3 搬运完成后均会复位，由 BootLoader 重新决策；
 * 路径2 跳转后不再返回。
 *
 * 注意：BootLoader_Brance 后主循环才会进入命令行，
 *       因此路径 2 直接跳转的用户程序看不到 Bootloader 命令行。
 */
void BootLoader_Brance(void)
{
    uint8_t startup_select;

    startup_select = BootLoader_Enter(BOOT_ENTER_TIMEOUT_MS100);
    if(startup_select == BOOT_KEY_NONE)
    {
        /* 路径 1/2：超时未按键，检查 OTA 标记 */
        if(OTA_Info.OTA_FLAG == OTA_SET_FLAG)
        {
            U1_printf("OTA YES!\r\n");
            OLED_Boot_ShowLine2x(4, "OTA");
            BootStaFlag |= UPDATA_A_FLAG;
            UpDataA.W25Q64_BlockNB = 0;
        }
        else
        {
            U1_printf("OTA GO A!\r\n");
            OLED_Boot_ShowLine2x(4, "GO A");
            LOAD_A(STM32_A_START_ADDR);
        }
    }
    else if(startup_select != BOOT_START_CMD)
    {
        /* 路径 3：按键选择外部 Flash block，立即搬运 */
        U1_printf("KEY use W25Q64 block %d\r\n", startup_select);
        OLED_Boot_ShowLine2x(4, (startup_select == 1) ? "ESC B1" : "OK B2");
        BootStaFlag |= UPDATA_A_FLAG;
        UpDataA.W25Q64_BlockNB = startup_select;
        BootLoader_UpdateAFromExternalFlash();
    }

    /* 路径 1 走到此处（路径 2 已跳转，路径 3 已复位），进入命令行 */
    U1_printf("进入BootLoader命令行\r\n");
    BootLoader_Info();
}

/*
 * timeout: 超时值，单位 100ms。调用传 BOOT_ENTER_TIMEOUT_MS100 表示等待 10s。
 * 返回 0=超时正常启动，0xFF=串口 'w' 进入命令行，
 *      1=ESC (PA8) 选择 block1，2=OK (PB12) 选择 block2。
 *
 * 按键采用两次读取消抖，第 1 次下降沿触发后延时 20ms 确认。
 * 串口 'w' 检测在主循环定时查询 USART1_RxBuf[0]，非中断触发。
 */
uint8_t BootLoader_Enter(uint8_t timeout)
{
    BootLoader_KeyInit();
    OLED_Boot_ShowLine2x(0, "BOOT");
    OLED_Boot_ShowLine2x(2, "ESC B1");
    OLED_Boot_ShowLine2x(4, "OK B2");
    U1_printf("%ds内，w进入命令行，ESC B1，OK B2\r\n", timeout / 10);
    while(timeout--)
    {
        uint8_t key_block;

        Delay_ms(100);
        key_block = BootLoader_ReadKeyBlock();
        if(key_block != BOOT_KEY_NONE)
        {
            return key_block;
        }
        if(USART1_RxBuf[0] == 'w')
        {
            return BOOT_START_CMD;
        }
    }
    return BOOT_KEY_NONE;
}

static void BootLoader_KeyInit(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);

    /* 按键接地触发，Bootloader 内部上拉；ESC(PA8)=block1，OK(PB12)=block2。 */
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}

static uint8_t BootLoader_ReadKeyBlock(void)
{
    if(GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_8) == Bit_RESET)
    {
        Delay_ms(20);
        if(GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_8) == Bit_RESET)
            return BOOT_KEY_BLOCK1;
    }

    if(GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_12) == Bit_RESET)
    {
        Delay_ms(20);
        if(GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_12) == Bit_RESET)
            return BOOT_KEY_BLOCK2;
    }

    return BOOT_KEY_NONE;
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
            Delay_ms(100);  /* 等待 TX 发送完成，避免复位瞬间丢最后几个字符 */
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
 *
 * 缓存策略：Updatabuff（大小 1KB = 1 个 Flash 页）做接收缓冲。
 *           每收到 8 个 XMODEM 包（8 * 128 = 1024 字节）触发一次页写入，
 *           减少内部 Flash 写入次数。
 *
 *   SOH(133B) → CRC-CCITT 校验 → 缓存到 Updatabuff → 满页刷写
 *   EOT(1B)   → 刷写剩余数据，外部下载模式保存长度，否则复位
 *
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
        recv_crc = ((uint16_t)data[XMODEM_CRC_OFFSET_HIGH] << 8)
                   | data[XMODEM_CRC_OFFSET_LOW];

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

    memset(OTA_Info.OTA_ver, 0, sizeof(OTA_Info.OTA_ver));
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
 * 页写入前需确保目标页已被擦除（调用方在 BootLoader_StartXmodem 中
 * 已按 64KB Block 整体擦除）。
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
 * 将外部 Flash 中指定 Block 的固件搬运到 STM32 内部 Flash A 区。
 *
 * 依赖：
 *   - file_len 已在 OTA_Info.Firelen[] 中记录（XMODEM 下载阶段写入）
 *   - BootStaFlag 已置 UPDATA_A_FLAG，且 UpDataA.W25Q64_BlockNB 已给出块号
 *
 * 搬运完成后：
 *   - Block 0（OTA 场景）→ 清 OTA_FLAG 并复位，避免下次启动重复升级
 *   - 其他 Block（按键选择）→ 直接复位，下次 BootLoader 启动走正常跳转
 *
 * Flash 约束：
 *   - A 区总容量 = STM32_A_PAGE_COUNT * FLASH_PAGE_SIZE = 44KB
 *   - Flash_WriteBuffer 按 half-word 编程，固件长度必须满足 4 字节对齐
 */
void BootLoader_UpdateAFromExternalFlash(void)
{
    uint32_t file_len;
    uint32_t block_addr;
    uint32_t page_index;
    uint32_t full_page_count;
    uint32_t remain_len;

    if((BootStaFlag & UPDATA_A_FLAG) == 0)
    {
        return;
    }

    file_len = OTA_Info.Firelen[UpDataA.W25Q64_BlockNB];
    block_addr = UpDataA.W25Q64_BlockNB * W25Q64_BLOCK_SIZE;

    U1_printf("长度%d字节\r\n", file_len);
    if(BootLoader_IsExternalBlockLengthValid(file_len) == 0)
    {
        BootStaFlag &= ~UPDATA_A_FLAG;
        return;
    }

    Flash_ErasePage(STM32_A_START_ADDR, STM32_A_PAGE_COUNT);

    full_page_count = file_len / FLASH_PAGE_SIZE;
    remain_len = file_len % FLASH_PAGE_SIZE;

    for(page_index = 0; page_index < full_page_count; page_index++)
    {
        W25Q64_Read(block_addr + page_index * FLASH_PAGE_SIZE, UpDataA.Updatabuff, FLASH_PAGE_SIZE);
        Flash_WriteBuffer(STM32_A_START_ADDR + page_index * FLASH_PAGE_SIZE, UpDataA.Updatabuff, FLASH_PAGE_SIZE);
    }

    if(remain_len != 0)
    {
        W25Q64_Read(block_addr + page_index * FLASH_PAGE_SIZE, UpDataA.Updatabuff, remain_len);
        Flash_WriteBuffer(STM32_A_START_ADDR + page_index * FLASH_PAGE_SIZE, UpDataA.Updatabuff, remain_len);
    }

    if(UpDataA.W25Q64_BlockNB == 0)
    {
        /* 0号 block 是 OTA 下载区，搬运成功后清标志，避免下次启动重复升级。 */
        OTA_Info.OTA_FLAG = 0;
        AT24C02_WriteOTAInfo();
    }

    U1_printf("升级完成\r\n");
    OLED_Boot_ShowLine2x(4, "UP OK");
    NVIC_SystemReset();
}

static uint8_t BootLoader_IsExternalBlockLengthValid(uint32_t file_len)
{
    if((file_len == 0) || (file_len > (STM32_A_PAGE_COUNT * FLASH_PAGE_SIZE)))
    {
        U1_printf("外部Flash block %d 无有效程序\r\n", UpDataA.W25Q64_BlockNB);
        OLED_Boot_ShowLine2x(4, "NO APP");
        return 0;
    }

    if(file_len % 4 != 0)
    {
        U1_printf("长度不对\r\n");
        OLED_Boot_ShowLine2x(4, "LEN ERR");
        return 0;
    }

    return 1;
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
 * 跳转 A 区用户程序（跳转后不再返回）：
 *
 * 前置校验：栈顶地址必须在 SRAM 范围（0x20000000~0x20004FFF），
 *           防止无效指针导致 HardFault。
 *
 * 跳转步骤：
 *   1. MSR_SP — 切栈为用户程序栈顶（必须在切换 VTOR 之前完成）
 *   2. 读取用户 Reset_Handler 入口地址（向量表偏移 4）
 *   3. BootLoader_Clear — 复位 Bootloader 用过的 USART1/GPIOA/GPIOB
 *      外设，避免用户程序未初始化前，残留外设中断从用户向量表
 *      读到错误入口地址导致 HardFault
 *   4. 调用 load_A() 跳转
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
