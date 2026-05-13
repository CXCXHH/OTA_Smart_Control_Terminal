/*******************************************************
 * FileName: main.c
 * Description: BootLoader 主循环 — 串口命令轮询、XMODEM
 *              请求发送、外部 Flash→A 区固件搬运
 ******************************************************/
#include "main.h"

/* OTA 信息（从 AT24C02 读取/写入） */
OTA_InfoCB OTA_Info;
/* A区升级临时缓冲区 + 当前 W25Q64 块号 */
UpDataA_CB UpDataA;
/* 启动状态标记（如 UPDATA_A_FLAG） */
uint32_t BootStaFlag;

int main(void)
{
	Delay_Init();
	Bsp_Init();
	/* 读回 OTA_Info 判断是否需要进入升级模式，避免覆盖已保存的外部 Flash 固件长度 */
	AT24C02_ReadOTAInfo();
	BootLoader_Brance();
	while(1)
	{
		Delay_ms(10);
		BootLoader_PollCommandFrame();
		BootLoader_PollXmodemRequest();
		BootLoader_UpdateAFromExternalFlash();
	}
}

/*
 * 从环形接收队列取出下一帧数据并交给 BootLoader_Event 处理。
 * URxDataOUT/URxDataIN 为队列读写指针，回绕在队列末端处理。
 */
void BootLoader_PollCommandFrame(void)
{
	uint16_t frame_len;

	if(U0CB.URxDataOUT == U0CB.URxDataIN)
	{
		return;
	}

	frame_len = U0CB.URxDataOUT->end - U0CB.URxDataOUT->start + 1;
	BootLoader_Event(U0CB.URxDataOUT->start, frame_len);

	U0CB.URxDataOUT++;
	if(U0CB.URxDataOUT == U0CB.URxDataEND)
	{
		U0CB.URxDataOUT = &U0CB.UrxDataPtr[0];  /* 环形接收队列回绕 */
	}
}

/*
 * XMODEM 接收方定时器：约每 100 个主循环周期（~1s）发送一个 'C'
 * 通知发送方开始传输，仅在 BootStaFlag 置 IAP_XMODEMC_FLAG 时生效。
 */
void BootLoader_PollXmodemRequest(void)
{
	if((BootStaFlag & IAP_XMODEMC_FLAG) == 0)
	{
		return;
	}

	if(UpDataA.XmodemTimer >= 100)
	{
		U1_printf("C");
		UpDataA.XmodemTimer = 0;
	}
	UpDataA.XmodemTimer++;
}

/*
 * 将外部 Flash 中指定 Block 的固件搬运到 STM32 内部 Flash A 区。
 * 依赖：
 *   - file_len 已在 OTA_Info.Firelen[] 中记录
 *   - 内部 Flash 须先整片擦除
 * 搬运完成（Block 0 的 OTA 场景）后清 OTA_FLAG 并复位。
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
	block_addr = UpDataA.W25Q64_BlockNB * 64 * 1024;

	U1_printf("长度%d字节\r\n", file_len);
	if((file_len == 0) || (file_len > (STM32_A_PAGE_COUNT * FLASH_PAGE_SIZE)))
	{
		U1_printf("外部Flash block %d 无有效程序\r\n", UpDataA.W25Q64_BlockNB);
		OLED_Boot_ShowLine2x(4, "NO APP");
		BootStaFlag &= ~UPDATA_A_FLAG;
		return;
	}

	/* 内部 Flash 按 half-word 编程，固件长度必须保持偶数字节/4字节对齐。 */
	if(file_len % 4 != 0)
	{
		U1_printf("长度不对\r\n");
		OLED_Boot_ShowLine2x(4, "LEN ERR");
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
