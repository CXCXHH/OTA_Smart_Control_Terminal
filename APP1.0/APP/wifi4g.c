#include "wifi4g.h"
#include "bsp_usart3.h"
#include "bsp.h"
#include "mqtt.h"
#include <string.h>
#include <stdio.h>

volatile uint8_t WIFI4G_CMD_Status = 0;
uint8_t Parse_Substr[32] = {0};
FIFO_t UART3_FIFO;

#define NSize 512
static uint8_t RecvBuf[NSize];

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

uint8_t WIFI4G_Parse_Queue(void)
{
    uint8_t c;
    uint16_t i = 0;

    while (FIFO_Pop(&UART3_FIFO, &c))
    {
        RecvBuf[i++] = c;
        U1_printf("%c", c);
        if (i >= NSize - 1) break;
    }
    RecvBuf[i] = '\0';

    if (MQTT_Download_Flag)
    {
        char *leftp = strstr((char *)RecvBuf, "{");
        if (leftp)
            MQTT_Parse_JsonData((uint8_t *)leftp);
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
    U1_printf("WiFi not connected, starting SmartConfig...\r\n");
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
