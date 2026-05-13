/*******************************************************
 * FileName: main.c
 * Description: BootLoader 主循环 — 串口命令轮询、XMODEM
 *              请求发送、外部 Flash→A 区固件搬运
 ******************************************************/
#include "main.h"

static void BootLoader_PollCommandFrame(void);
static void BootLoader_PollXmodemRequest(void);

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
static void BootLoader_PollCommandFrame(void)
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
static void BootLoader_PollXmodemRequest(void)
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
