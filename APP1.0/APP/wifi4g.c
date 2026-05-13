/**
  * @brief  WiFi/4G AT 指令框架
  * @note   通过 USART3 + FIFO 实现 AT 命令的发送/响应解析
  *         支持同步等待模式 (Test_WIFI4G_CMD_Status)，
  *         响应匹配通过 Parse_Substr 字符串实现
  */
#include "wifi4g.h"
#include "bsp_usart3.h"
#include "bsp.h"
#include "mqtt.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

volatile uint8_t WIFI4G_CMD_Status = 0;
uint8_t Parse_Substr[32] = {0};
FIFO_t UART3_FIFO;

#define NSize 512
static uint8_t RecvBuf[NSize];

/* 持久化 RPC request ID，应对 topic 与 JSON payload 分两批到达的情况 */
static uint8_t  RPC_Pending    = 0;
static uint32_t RPC_RequestId  = 0;

/**
  * @brief  等待 AT 命令响应 (阻塞)
  * @param  timeout_ms  超时时间 (ms)
  * @retval WIFI4G_OK=收到匹配, WIFI4G_ERROR=收到ERROR, WIFI4G_NOT=超时
  */
uint8_t Test_WIFI4G_CMD_Status(uint32_t timeout_ms)
{
    while (WIFI4G_CMD_Status == WIFI4G_NOT)
    {
        WIFI4G_Parse_Queue();
        if (timeout_ms-- == 0) return WIFI4G_NOT;
        Delay_ms(1);
    }
    return WIFI4G_CMD_Status;
}

/**
  * @brief  解析 USART3 FIFO 接收队列
  * @note   功能:
  *         1. 将 FIFO 数据搬移到 RecvBuf 并打印
  *         2. 检测 MQTT 连接/断开状态
  *         3. 匹配命令响应 (Parse_Substr) 或 ERROR
  *         4. 检测 MQTT 下行 JSON 数据并解析
  */
uint8_t WIFI4G_Parse_Queue(void)
{
    uint8_t c;
    uint16_t i = 0;

    while (FIFO_Pop(&UART3_FIFO, &c))
    {
        RecvBuf[i++] = c;
        if (i >= NSize - 1) break;
    }
    RecvBuf[i] = '\0';

    if (MQTT_Download_Flag)
    {
        char *topicp = strstr((char *)RecvBuf, "v1/devices/me/rpc/request/");
        char *leftp  = strstr((char *)RecvBuf, "{");
        char *rightp = strrchr((char *)RecvBuf, '}');

        /* 提取 RPC request ID（提前扫描，即使 JSON payload 稍后才到） */
        if (topicp != NULL) {
            topicp += strlen("v1/devices/me/rpc/request/");
            RPC_RequestId = strtoul(topicp, NULL, 10);
            RPC_Pending = 1;
        }

        if ((leftp != NULL) && (rightp != NULL) && (rightp >= leftp)) {
            *++rightp = '\0';

            if (RPC_Pending) {
                MQTT_Parse_DeviceData((uint8_t *)leftp, RPC_RequestId);
                RPC_Pending    = 0;
                RPC_RequestId  = 0;
            } else {
                MQTT_Parse_JsonData((uint8_t *)leftp);
            }
        }
    }

    if (strstr((const char *)RecvBuf, "+MQTTDISCONNECTED"))
        MQTT_Download_Flag = 0;
    if (strstr((const char *)RecvBuf, "+MQTTCONNECTED"))
        MQTT_Download_Flag = 1;

    if (strstr((const char *)RecvBuf, (const char *)Parse_Substr))
    {
        WIFI4G_CMD_Status = WIFI4G_OK;
        return WIFI4G_OK;
    }
    if (strstr((const char *)RecvBuf, "ERROR\r\n"))
    {
        WIFI4G_CMD_Status = WIFI4G_ERROR;
        return WIFI4G_ERROR;
    }
    return WIFI4G_NOT;
}

/**
  * @brief  连接 WiFi (ESP8266)
  * @note   步骤: AT+CWMODE=1 → AT+CWJAP(检查已有连接)
  *         → 若未连接则启动 SmartConfig 配网 (超时 200s)
  */
uint8_t ESP8266_Connect_WIFI(void)
{
    uint8_t buf[64];
    uint8_t ret;

    strcpy((char *)Parse_Substr, "OK\r\n");
    WIFI4G_CMD_Status = WIFI4G_NOT;
    strcpy((char *)buf, "AT+CWMODE=1\r\n");
    Usart3_SendBuf(buf, strlen((char *)buf));
    if (Test_WIFI4G_CMD_Status(1000) == WIFI4G_ERROR) return 0;

    strcpy((char *)buf, "AT+CWJAP\r\n");
    strcpy((char *)Parse_Substr, "OK\r\n");
    WIFI4G_CMD_Status = WIFI4G_NOT;
    Usart3_SendBuf(buf, strlen((char *)buf));
    ret = Test_WIFI4G_CMD_Status(20000);
    if (ret == WIFI4G_OK) return 1;

    if (ret == WIFI4G_NOT) return 0;

    /* WiFi 连接失败，尝试 SmartConfig */
    strcpy((char *)Parse_Substr, "smartconfig connected wifi\r\n");
    WIFI4G_CMD_Status = WIFI4G_NOT;
    strcpy((char *)buf, "AT+CWSTARTSMART=3,3\r\n");
    Usart3_SendBuf(buf, strlen((char *)buf));
    ret = Test_WIFI4G_CMD_Status(200000);
    if (ret == WIFI4G_OK)
    {
        strcpy((char *)Parse_Substr, "OK\r\n");
        strcpy((char *)buf, "AT+CWSTOPSMART\r\n");
        Usart3_SendBuf(buf, strlen((char *)buf));
        if (Test_WIFI4G_CMD_Status(1000) == WIFI4G_ERROR) return 0;
        return 1;
    }
    return 0;
}
