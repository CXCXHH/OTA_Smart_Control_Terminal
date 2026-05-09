#include "mqtt.h"
#include "wifi4g.h"
#include "bsp_usart3.h"
#include "bsp.h"
#include "modbus_app.h"
#include "bsp_gpio.h"
#include <string.h>
#include <stdio.h>

volatile uint8_t MQTT_Download_Flag = 0;

static uint8_t *Get_CPUID(void)
{
    static uint8_t cpuid[32];
    uint32_t id[3];
    id[0] = *(__IO uint32_t *)(0x1FFFF7E0);
    id[1] = *(__IO uint32_t *)(0x1FFFF7EC);
    id[2] = *(__IO uint32_t *)(0x1FFFF7E8);
    sprintf((char *)cpuid, "%08X%08X%08X", id[0], id[1], id[2]);
    return cpuid;
}

#if MQTT_WIFI_4G_ENABLE
static uint8_t ESP8266_Connect_MQTTServer(void)
{
    uint8_t buf[128];
    uint8_t retry;
    static uint8_t user_cfg_done = 0;

    if (!user_cfg_done) {
        for (retry = 0; retry < 2; retry++) {
            strcpy((char *)Parse_Substr, "OK\r\n");
            WIFI4G_CMD_Status = WIFI4G_NOT;
            sprintf((char *)buf, "AT+MQTTUSERCFG=0,1,\"%s\",\"\",\"\",0,0,\"\"\r\n", Get_CPUID());
            Usart3_SendBuf(buf, strlen((char *)buf));
            if (Test_WIFI4G_CMD_Status(1000) == WIFI4G_OK)
                break;

            if (retry != 0)
                return 0;

            user_cfg_done = 0;
            strcpy((char *)Parse_Substr, "OK\r\n");
            WIFI4G_CMD_Status = WIFI4G_NOT;
            strcpy((char *)buf, "AT+MQTTCLEAN=0\r\n");
            Usart3_SendBuf(buf, strlen((char *)buf));
            (void)Test_WIFI4G_CMD_Status(5000);
            Delay_ms(200);
        }

        strcpy((char *)Parse_Substr, "OK\r\n");
        WIFI4G_CMD_Status = WIFI4G_NOT;
        strcpy((char *)buf, "AT+MQTTCONNCFG=0,120,0,\"\",\"\",0,0\r\n");
        Usart3_SendBuf(buf, strlen((char *)buf));
        if (Test_WIFI4G_CMD_Status(1000) != WIFI4G_OK)
            return 0;

        user_cfg_done = 1;
    }

    for (retry = 0; retry < 2; retry++) {
        strcpy((char *)Parse_Substr, "OK\r\n");
        WIFI4G_CMD_Status = WIFI4G_NOT;
        strcpy((char *)buf, "AT+MQTTCONN=0,\"broker.emqx.io\",1883,0\r\n");
        Usart3_SendBuf(buf, strlen((char *)buf));
        if (Test_WIFI4G_CMD_Status(5000) == WIFI4G_OK)
            break;

        if (retry != 0)
            return 0;

        user_cfg_done = 0;
        strcpy((char *)Parse_Substr, "OK\r\n");
        WIFI4G_CMD_Status = WIFI4G_NOT;
        strcpy((char *)buf, "AT+MQTTCLEAN=0\r\n");
        Usart3_SendBuf(buf, strlen((char *)buf));
        (void)Test_WIFI4G_CMD_Status(5000);
        Delay_ms(200);
    }

    WIFI4G_CMD_Status = WIFI4G_NOT;
    sprintf((char *)buf, "AT+MQTTSUB=0,\"STM32V9/DownLoad/%s\",0\r\n", Get_CPUID());
    Usart3_SendBuf(buf, strlen((char *)buf));
    if (Test_WIFI4G_CMD_Status(5000) == WIFI4G_ERROR) return 0;

    U1_printf("MQTT Server OK\r\n");
    return 1;
}
#else

static uint8_t ML307_Connect_MQTTServer(void)
{
    uint8_t buf[128];

    strcpy((char *)Parse_Substr, "OK\r\n");
    WIFI4G_CMD_Status = WIFI4G_NOT;
    sprintf((char *)buf, "AT+DTUTASK=\"1\",\"20\"\r\n");
    Usart3_SendBuf(buf, strlen((char *)buf));
    if (Test_WIFI4G_CMD_Status(1000) == WIFI4G_ERROR) return 0;

    WIFI4G_CMD_Status = WIFI4G_NOT;
    sprintf((char *)buf, "AT+MQTT=\"%s\"\r\n", Get_CPUID());
    Usart3_SendBuf(buf, strlen((char *)buf));
    if (Test_WIFI4G_CMD_Status(1000) == WIFI4G_ERROR) return 0;

    WIFI4G_CMD_Status = WIFI4G_NOT;
    strcpy((char *)buf, "AT+MQTTIP=\"broker.emqx.io\",\"1883\"\r\n");
    Usart3_SendBuf(buf, strlen((char *)buf));
    if (Test_WIFI4G_CMD_Status(1000) == WIFI4G_ERROR) return 0;

    WIFI4G_CMD_Status = WIFI4G_NOT;
    sprintf((char *)buf, "AT+MQSUB=\"1\",\"1\",\"4\",\"STM32V9/DownLoad/%s\"\r\n", Get_CPUID());
    Usart3_SendBuf(buf, strlen((char *)buf));
    if (Test_WIFI4G_CMD_Status(1000) == WIFI4G_ERROR) return 0;

    WIFI4G_CMD_Status = WIFI4G_NOT;
    sprintf((char *)buf, "AT+MQPUB=\"1\",\"1\",\"4\",\"STM32V9/UPLoad/%s\"\r\n", Get_CPUID());
    Usart3_SendBuf(buf, strlen((char *)buf));
    if (Test_WIFI4G_CMD_Status(1000) == WIFI4G_ERROR) return 0;

    WIFI4G_CMD_Status = WIFI4G_NOT;
    strcpy((char *)buf, "AT+MQMULTEN=0\r\n");
    Usart3_SendBuf(buf, strlen((char *)buf));
    if (Test_WIFI4G_CMD_Status(1000) == WIFI4G_ERROR) return 0;

    WIFI4G_CMD_Status = WIFI4G_NOT;
    strcpy((char *)Parse_Substr, "MQTT_CONNECT:");
    strcpy((char *)buf, "AT+REST\r\n");
    Usart3_SendBuf(buf, strlen((char *)buf));
    if (Test_WIFI4G_CMD_Status(180000) == WIFI4G_ERROR) return 0;

    U1_printf("MQTT Server OK\r\n");
    return 1;
}
#endif

uint8_t MQTT_Connect_Server(void)
{
    uint8_t ok;

#if MQTT_WIFI_4G_ENABLE
    ok = ESP8266_Connect_MQTTServer();
#else
    ok = ML307_Connect_MQTTServer();
#endif
    MQTT_Download_Flag = ok ? 1 : 0;
    return ok;
}

uint8_t MQTT_SendData(void)
{
#if MQTT_WIFI_4G_ENABLE
    uint8_t cmd[128];
    uint8_t payload[128];
    uint16_t len;

    sprintf((char *)payload,
        "{\"TP\":%d,\"RH\":%d,\"VO\":%d,\"CU\":%d,\"PW\":%d,\"VR\":%d,\"CPU\":%d}",
        REG_HOLD_BUF[1], REG_HOLD_BUF[2], REG_HOLD_BUF[3],
        REG_HOLD_BUF[4], REG_HOLD_BUF[5], REG_HOLD_BUF[6], REG_HOLD_BUF[7]);
    len = strlen((char *)payload);

    sprintf((char *)cmd,
        "AT+MQTTPUBRAW=0,\"STM32V9/UPLoad/%s\",%d,0,0\r\n",
        Get_CPUID(), len);
    strcpy((char *)Parse_Substr, ">");
    WIFI4G_CMD_Status = WIFI4G_NOT;
    Usart3_SendBuf(cmd, strlen((char *)cmd));
    if (Test_WIFI4G_CMD_Status(2000) != WIFI4G_OK)
        return 0;

    strcpy((char *)Parse_Substr, "OK\r\n");
    WIFI4G_CMD_Status = WIFI4G_NOT;
    Usart3_SendBuf(payload, len);
    if (Test_WIFI4G_CMD_Status(5000) != WIFI4G_OK)
        return 0;
#else
    uint8_t buf[256];

    sprintf((char *)buf,
        "{\"TP\":%d,\"RH\":%d,\"VO\":%d,\"CU\":%d,\"PW\":%d,\"VR\":%d,\"CPU\":%d}",
        REG_HOLD_BUF[1], REG_HOLD_BUF[2], REG_HOLD_BUF[3],
        REG_HOLD_BUF[4], REG_HOLD_BUF[5], REG_HOLD_BUF[6], REG_HOLD_BUF[7]);
    Usart3_SendBuf(buf, strlen((char *)buf));
#endif
    return 1;
}

uint8_t MQTT_Parse_JsonData(uint8_t *json)
{
    static const struct {
        const char *key;
        uint16_t mask;
    } cmds[] = {
        {"\"LED1\"", LED1_CMD},
        {"\"LED2\"", LED2_CMD},
        {"\"BEEP\"", BEEP_CMD},
        {"\"RELAY\"", RELAY_CMD},
    };
    char *p;
    uint8_t i;
    uint8_t changed = 0;

    for (i = 0; i < (sizeof(cmds) / sizeof(cmds[0])); i++) {
        p = strstr((char *)json, cmds[i].key);
        if (p == NULL)
            continue;
        p = strchr(p, ':');
        if (p == NULL)
            continue;
        p++;
        while ((*p == ' ') || (*p == '\t'))
            p++;
        if ((*p == '1') || (strncmp(p, "true", 4) == 0))
            REG_HOLD_BUF[0] |= cmds[i].mask;
        else if ((*p == '0') || (strncmp(p, "false", 5) == 0))
            REG_HOLD_BUF[0] &= (uint16_t)~cmds[i].mask;
        else
            continue;
        changed = 1;
    }

    if (changed)
        Output_Control(REG_HOLD_BUF[0]);
    return changed;
}
