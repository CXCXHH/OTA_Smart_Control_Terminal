#ifndef WIFI4G_H
#define WIFI4G_H

#include <stdint.h>
#include "fifo.h"

/* AT 命令响应状态 */
enum { WIFI4G_NOT = 0, WIFI4G_OK, WIFI4G_ERROR };

extern volatile uint8_t WIFI4G_CMD_Status;
extern uint8_t Parse_Substr[32];
extern FIFO_t UART3_FIFO;

uint8_t Test_WIFI4G_CMD_Status(uint32_t timeout_ms);
uint8_t WIFI4G_Parse_Queue(void);
uint8_t ESP8266_Connect_WIFI(void);

#endif
