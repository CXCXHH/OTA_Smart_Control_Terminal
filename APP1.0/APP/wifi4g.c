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
static uint8_t OtaHeader[96];
static uint16_t OtaHeaderLen;
static uint16_t OtaPayloadLen;
static uint16_t OtaPayloadReceived;
static uint8_t OtaPayloadActive;

void OTA_ClearAccum(void)
{
    OtaHeaderLen = 0;
    OtaPayloadLen = 0;
    OtaPayloadReceived = 0;
    OtaPayloadActive = 0;
}

/* 持久化 RPC request ID，应对 topic 与 JSON payload 分两批到达的情况 */
static uint8_t  RPC_Pending    = 0;
static uint32_t RPC_RequestId  = 0;

static char *FindAsciiInBuf(const uint8_t *buf, uint16_t len, const char *pat)
{
    uint16_t i;
    uint16_t pat_len;

    if ((buf == NULL) || (pat == NULL))
        return NULL;

    pat_len = (uint16_t)strlen(pat);
    if ((pat_len == 0U) || (len < pat_len))
        return NULL;

    for (i = 0; i <= (uint16_t)(len - pat_len); i++) {
        if (memcmp(&buf[i], pat, pat_len) == 0)
            return (char *)&buf[i];
    }

    return NULL;
}

static void OTA_ConsumePayloadByte(uint8_t c)
{
    if (OtaPayloadReceived < sizeof(OTA_Info.recv_buf))
        OTA_Info.recv_buf[OtaPayloadReceived] = c;

    OtaPayloadReceived++;
    if (OtaPayloadReceived >= OtaPayloadLen) {
        OTA_Info.recv_flag = 1;
        if ((OTA_Info.chunk_id == 0UL) || ((OTA_Info.chunk_id % 16UL) == 0UL) ||
            (OTA_Info.request_bytes != OTA_CHUNK_SIZE)) {
            U1_printf("OTA chunk %lu len=%lu\r\n", OTA_Info.chunk_id, OTA_Info.request_bytes);
        }
        OTA_ClearAccum();
    }
}

static void OTA_ParseChunkStream(const uint8_t *buf, uint16_t len)
{
    char prefix[64];
    char *chunkp;
    uint16_t prefix_len;
    uint16_t i;

    sprintf(prefix, "v2/fw/response/%lu/chunk/%lu,",
            OTA_Info.request_id, OTA_Info.chunk_id);
    prefix_len = (uint16_t)strlen(prefix);

    for (i = 0; i < len; i++) {
        if (OtaPayloadActive) {
            OTA_ConsumePayloadByte(buf[i]);
            continue;
        }

        if (OtaHeaderLen < sizeof(OtaHeader))
            OtaHeader[OtaHeaderLen++] = buf[i];
        else {
            uint16_t keep = (prefix_len > 1U) ? (prefix_len - 1U) : 0U;
            if (keep >= sizeof(OtaHeader))
                keep = sizeof(OtaHeader) - 1U;
            memmove(OtaHeader, OtaHeader + OtaHeaderLen - keep, keep);
            OtaHeaderLen = keep;
            OtaHeader[OtaHeaderLen++] = buf[i];
        }

        chunkp = FindAsciiInBuf(OtaHeader, OtaHeaderLen, prefix);
        if (chunkp != NULL) {
            uint16_t chunk_off = (uint16_t)(chunkp - (char *)OtaHeader);
            uint16_t data_off = (uint16_t)(chunk_off + prefix_len);
            uint16_t len_pos;
            uint32_t wire_len = 0UL;
            uint16_t buffered_payload = OtaHeaderLen - data_off;
            uint16_t j;

            len_pos = data_off;
            while ((len_pos < OtaHeaderLen) && (OtaHeader[len_pos] >= '0') && (OtaHeader[len_pos] <= '9')) {
                wire_len = (wire_len * 10UL) + (uint32_t)(OtaHeader[len_pos] - '0');
                len_pos++;
            }
            if ((len_pos >= OtaHeaderLen) || (OtaHeader[len_pos] != ','))
                continue;

            data_off = (uint16_t)(len_pos + 1U);
            if ((wire_len == 0UL) || (wire_len > sizeof(OTA_Info.recv_buf)))
                continue;

            buffered_payload = OtaHeaderLen - data_off;
            OtaPayloadLen = (uint16_t)wire_len;
            OtaPayloadReceived = 0;
            OtaPayloadActive = 1;

            for (j = 0; (j < buffered_payload) && OtaPayloadActive; j++)
                OTA_ConsumePayloadByte(OtaHeader[data_off + j]);

            if (OtaPayloadActive)
                OtaHeaderLen = 0;
        }
    }
}

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
    char *topicp;
    char *leftp;
    char *rightp;

    while (FIFO_Pop(&UART3_FIFO, &c))
    {
        RecvBuf[i++] = c;
        if (i >= NSize - 1) break;
    }
    RecvBuf[i] = '\0';

    if (i == 0)
        return WIFI4G_NOT;

    if (MQTT_Download_Flag)
    {
        if (MQTT_OTA_FLAG) {
            OTA_ParseChunkStream(RecvBuf, i);
        } else {
            topicp = FindAsciiInBuf(RecvBuf, i, "v1/devices/me/rpc/request/");
            leftp  = FindAsciiInBuf(RecvBuf, i, "{");
            rightp = NULL;

            if (leftp != NULL) {
                uint16_t idx;
                for (idx = i; idx > 0U; idx--) {
                    if (RecvBuf[idx - 1U] == '}') {
                        rightp = (char *)&RecvBuf[idx - 1U];
                        break;
                    }
                }
            }

            /* 提取 RPC request ID（提前扫描，即使 JSON payload 稍后才到） */
            if (topicp != NULL) {
                topicp += strlen("v1/devices/me/rpc/request/");
                RPC_RequestId = strtoul(topicp, NULL, 10);
                RPC_Pending = 1;
            }

            if ((leftp != NULL) && (rightp != NULL) && (rightp >= leftp)) {
                *++rightp = '\0';

                if (strstr((char *)RecvBuf, "v1/devices/me/attributes") != NULL) {
                    MQTT_Parse_OTAData((uint8_t *)leftp);
                } else if (RPC_Pending) {
                    MQTT_Parse_DeviceData((uint8_t *)leftp, RPC_RequestId);
                    RPC_Pending    = 0;
                    RPC_RequestId  = 0;
                } else {
                    MQTT_Parse_JsonData((uint8_t *)leftp);
                }
            }
        }
    }

    /* ML307 实际输出 MQTT_DISCONNECT / MQTT_CONNECT（无 + 前缀，分段到达） */
    if (strstr((const char *)RecvBuf, "MQTT_DISCONNECT"))
        MQTT_Download_Flag = 0;
    if (strstr((const char *)RecvBuf, "MQTT_CONNECT:"))
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
