#include "main.h"
#include "boot.h"


// 函数指针变量：保存用户程序复位入口，赋值后 call 即跳转
load_a load_A;

static void W25Q64_WriteUpdateData(uint32_t offset, uint8_t *data, uint16_t len)
{
    uint16_t write_len;

    while(len)
    {
        write_len = (len > W25Q64_PAGE_SIZE) ? W25Q64_PAGE_SIZE : len;
        W25Q64_PageProgram(UpDataA.W25Q64_BlockNB * 64 * 1024 + offset, data, write_len);
        offset += write_len;
        data += write_len;
        len -= write_len;
    }
}

//===========================================================================
// 启动决策：检查 OTA 标记，决定进入升级模式或跳转用户程序
//===========================================================================
void BootLoader_Brance(void)
{
    if(BootLoader_Enter(20) == 0)
    {
        if(OTA_Info.OTA_FLAG == OTA_SET_FLAG)
        {
            U1_printf("OTA YES!\r\n");       // 有标记 → 待实现 OTA 接收
            BootStaFlag |= UPDATA_A_FLAG;
            UpDataA.W25Q64_BlockNB = 0;
        }
        else
        {
            U1_printf("OTA GO A!\r\n");
            LOAD_A(STM32_A_START_ADDR);      // 无标记 → 直接跳 A 区
        }
    }
    U1_printf("进入BootLoader命令行\r\n");
    BootLoader_Info();
}

uint8_t BootLoader_Enter(uint8_t timeout)
{
    U1_printf("%ds内，输入小写 w ，进入BootLoader命令行\r\n", timeout/10);
    while(timeout--)
    {
        Delay_ms(100);
        if(USART1_RxBuf[0] == 'w')
        {
            return 1;       /* 进入BootLoader命令行 */
        }
    }
    return 0;       /* 未进入BootLoader命令行 */
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

void BootLoader_Event(uint8_t *data, uint16_t datalen)
{
    int temp;
    uint32_t flash_offset;
    uint16_t remain_len;

    if(BootStaFlag == 0)
    {
        switch(data[0])
        {
            case '1':
                U1_printf("擦除A区\r\n");
                Flash_ErasePage(STM32_A_START_ADDR,STM32_A_PAGE_COUNT);
                break;

            case '2':
                U1_printf("XMODEM download to A, use bin file\r\n");
                Flash_ErasePage(STM32_A_START_ADDR,STM32_A_PAGE_COUNT);
                BootStaFlag |= (IAP_XMODEMC_FLAG | IAP_XMODEMD_FLAG);
                UpDataA.XmodemTimer = 0;
                UpDataA.XmodemNB = 0;
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
        }
    }
    else if(BootStaFlag & IAP_XMODEMD_FLAG)
    {
        if((datalen == 133) && (data[0] == 0x01))
        {
            BootStaFlag &= ~IAP_XMODEMC_FLAG;
            UpDataA.XmodemCRC = Xmodem_CRC16(&data[3], 128);
            if(UpDataA.XmodemCRC == ((uint16_t)data[131] << 8) | data[132])
            {
                UpDataA.XmodemNB++;
                memcpy(&UpDataA.Updatabuff[((UpDataA.XmodemNB-1) % (FLASH_PAGE_SIZE/128))*128], &data[3], 128);
                if((UpDataA.XmodemNB % (FLASH_PAGE_SIZE/128)) == 0)
                {
                    if(BootStaFlag & CMD5_XMODEM_FLAG)
                    {
                        flash_offset = ((UpDataA.XmodemNB / (FLASH_PAGE_SIZE/128)) - 1) * FLASH_PAGE_SIZE;
                        W25Q64_WriteUpdateData(flash_offset, UpDataA.Updatabuff, FLASH_PAGE_SIZE);
                    }
                    else
                    {
                        Flash_WriteBuffer(((UpDataA.XmodemNB / (FLASH_PAGE_SIZE/128))-1)*FLASH_PAGE_SIZE+STM32_A_START_ADDR,
                                            UpDataA.Updatabuff,
                                            FLASH_PAGE_SIZE);
                    }
                }
                U1_printf("\x06");
            }
            else
            {
                U1_printf("\x15");
            }
        }
        /* EOT：单独的外层分支，不能嵌套在 datalen==133 里面 */
        if((datalen == 1) && (data[0] == 0x04))
        {
            BootStaFlag &= ~IAP_XMODEMC_FLAG;
            U1_printf("\x06");
            if((UpDataA.XmodemNB % (FLASH_PAGE_SIZE/128)) != 0)
            {
                remain_len = (UpDataA.XmodemNB % (FLASH_PAGE_SIZE/128)) * 128;
                if(BootStaFlag & CMD5_XMODEM_FLAG)
                {
                    flash_offset = (UpDataA.XmodemNB / (FLASH_PAGE_SIZE/128)) * FLASH_PAGE_SIZE;
                    W25Q64_WriteUpdateData(flash_offset, UpDataA.Updatabuff, remain_len);
                }
                else
                {
                    Flash_WriteBuffer(((UpDataA.XmodemNB / (FLASH_PAGE_SIZE/128)))*FLASH_PAGE_SIZE+STM32_A_START_ADDR,
                                    UpDataA.Updatabuff,
                                    remain_len);
                }
                
            }
            BootStaFlag &= ~IAP_XMODEMD_FLAG;
             if(BootStaFlag & CMD5_XMODEM_FLAG)
             {
                OTA_Info.Firelen[UpDataA.W25Q64_BlockNB] = UpDataA.XmodemNB * 128;
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
    else if(BootStaFlag & SET_VERDION_FLAG)
    {
        if((datalen == 26))
        {
            /*版本号例，VER-1.0.0-2026/05/08-17:00*/
            if(sscanf((char *)data,"VER-%d.%d.%d-%d/%d/%d-%d:%d",&temp,&temp,&temp,&temp,&temp,&temp,&temp,&temp) == 8)
            {
                memset(OTA_Info.OTA_ver,0,32);
                memcpy(OTA_Info.OTA_ver,data,26);
                AT24C02_WriteOTAInfo();
                U1_printf("版本号正确\r\n");
                BootStaFlag &= ~SET_VERDION_FLAG;
                BootLoader_Info();
            }
            else
            {
                U1_printf("版本号格式错误\r\n");
            }
        }
        else
        {
            U1_printf("版本号长度错误\r\n");
        }
    }
    else if(BootStaFlag & CMD5_FLAG)
    {
        if(datalen==1)
        {
            if((data[0] >= 0x31) && (data[0] <= 0x39))
            {
                UpDataA.W25Q64_BlockNB = data[0] - 0x30;
                BootStaFlag |= (IAP_XMODEMC_FLAG | IAP_XMODEMD_FLAG | CMD5_XMODEM_FLAG);
                UpDataA.XmodemTimer = 0;
                UpDataA.XmodemNB = 0;
                OTA_Info.Firelen[UpDataA.W25Q64_BlockNB] = 0;
                W25Q64_EraseBlock64K(UpDataA.W25Q64_BlockNB * 64 * 1024);
                U1_printf("XMODEM download to external Flash block %d, use bin file\r\n",UpDataA.W25Q64_BlockNB);
                BootStaFlag &= ~CMD5_FLAG;
            }
            else
            {
                U1_printf("编号错误\r\n");
            }
        }
        else
        {
            U1_printf("Data长度错误\r\n");
        }
    }
    else if(BootStaFlag & CMD6_FLAG)
    {
        if(datalen==1)
        {
            if((data[0] >= 0x31) && (data[0] <= 0x39))
            {
                UpDataA.W25Q64_BlockNB = data[0] - 0x30;
                BootStaFlag |= UPDATA_A_FLAG;
                BootStaFlag &= ~CMD6_FLAG;
            }
            else
            {
                U1_printf("编号错误\r\n");
            }
        }
        else
        {
            U1_printf("Data长度错误\r\n");
        }
    }
}

//===========================================================================
// XMODEM CRC-CCITT (多项式 0x1021, 初值 0x0000)
//===========================================================================
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

//===========================================================================
// 设置栈顶指针 (汇编)
//   addr: 用户程序向量表第一个字 — 即用户程序的栈顶地址
//
//   MSR MSP, r0    : 将参数(r0)写入主栈指针 MSP
//   BX r14         : 返回调用者 (r14 = LR)
//===========================================================================
__asm void MSR_SP(uint32_t addr)
{
    MSR MSP, r0
    BX r14
}

//===========================================================================
// 跳转到用户程序
//   ARM 向量表布局 (每项 4 字节):
//     [addr+0]  栈顶指针 (MSP)          — CPU 复位后从此取值初始化 SP
//     [addr+4]  复位中断入口 (Reset_Handler) — 复位后从此取值开始执行
//
//   步骤:
//     1. 校验栈顶指针在 SRAM 范围内 (防无效地址)
//     2. 切换栈指针为用户程序的栈
//     3. 读取复位入口地址 → 赋值给函数指针 load_A
//     4. 关闭 Bootloader 用过的外设/中断
//     5. 跳转执行 load_A() — 进入用户程序的 Reset_Handler，不再返回
//===========================================================================
void LOAD_A(uint32_t addr)
{
    // 向量表首字(栈顶)必须在 SRAM 范围: 0x20000000 ~ 0x20004FFF (20KB)
    if((*(uint32_t *)addr >= 0x20000000) && (*(uint32_t *)addr <= 0x20004FFF))
    {
        MSR_SP(*(uint32_t *)addr);             // 切到用户栈

        load_A = (load_a)*(uint32_t *)(addr + 4); // 读 Reset_Handler 地址

        BootLoader_Clear();                    // 复位外设，避免中断冲突

        load_A();                              // 跳转，此后再不返回
    }
    else
    {
        U1_printf("Jump A faild!\r\n");
    }
}

//===========================================================================
// 清理 Bootloader 外设
//   跳转前必须复位所有使能的外设和中断，
//   否则用户程序未初始化时意外触发中断 → 查向量表错乱 → 跑飞
//===========================================================================
void BootLoader_Clear(void)
{
    USART_DeInit(USART1);    // 复位 USART1 (含 DMA 和中断)
    GPIO_DeInit(GPIOA);      // 复位 GPIOA
    GPIO_DeInit(GPIOB);      // 复位 GPIOB
}
