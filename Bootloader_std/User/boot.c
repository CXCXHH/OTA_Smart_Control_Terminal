#include "boot.h"
#include "main.h"

// 函数指针变量：保存用户程序复位入口，赋值后 call 即跳转
load_a load_A;

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
    else
    {
        U1_printf("进入BootLoader命令行\r\n");
        BootLoader_Info();
    }
    
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
