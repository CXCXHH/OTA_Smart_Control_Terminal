/**
  * @brief  MQTT 客户端 (ESP8266 AT / ML307 4G)
  * @note   通过 USART3 AT 指令驱动 WiFi/4G 模块，
  *         支持 MQTT publish(上报传感器数据) / subscribe(接收下行控制)
  *         使用 CPUID 作为客户端标识，构建独立 Topic
  */
#include "mqtt.h"
#include "wifi4g.h"
#include "bsp_usart3.h"
#include "bsp.h"
#include "modbus_app.h"
#include "bsp_gpio.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>

volatile uint8_t MQTT_Download_Flag = 0;

/** 读取 STM32 唯一 ID(96位) 作为 MQTT 客户端标识 */
#if MQTT_WIFI_4G_ENABLE
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
#endif

/** ESP8266 (WiFi) 方式连接 MQTT 服务器 */
#if MQTT_WIFI_4G_ENABLE
/**
  * @brief  通过 ESP8266 AT 指令连接 MQTT (broker.emqx.io:1883)
  * @note   步骤: MQTTUSERCFG → MQTTCONNCFG → MQTTCONN → MQTTSUB
  *         客户端 ID 使用 CPUID 确保唯一性
  *         订阅 Topic: STM32V9/DownLoad/{CPUID} (QoS 0)
  */
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
/**
  * @brief  通过 ML307 连接 xthings.cloud (教程方式)
  * @note   MQMULTEN → MQTTFILTER → MQSUBM → MQPUBM → REST
  */
static uint8_t ML307_Connect_MQTTServer(void)
{
    uint8_t buf[128];

    strcpy((char *)Parse_Substr, "OK\r\n");
    WIFI4G_CMD_Status = WIFI4G_NOT;
    strcpy((char *)buf, "AT+MQMULTEN=1\r\n");
    Usart3_SendBuf(buf, strlen((char *)buf));
    if (Test_WIFI4G_CMD_Status(1000) == WIFI4G_ERROR) return 0;

    WIFI4G_CMD_Status = WIFI4G_NOT;
    strcpy((char *)buf, "AT+MQTTFILTER=0\r\n");
    Usart3_SendBuf(buf, strlen((char *)buf));
    if (Test_WIFI4G_CMD_Status(1000) == WIFI4G_ERROR) return 0;

    WIFI4G_CMD_Status = WIFI4G_NOT;
    sprintf((char *)buf, "AT+MQSUBM=0,1,0,4,\"v1/devices/me/attributes\"\r\n");
    Usart3_SendBuf(buf, strlen((char *)buf));
    if (Test_WIFI4G_CMD_Status(1000) == WIFI4G_ERROR) return 0;

    WIFI4G_CMD_Status = WIFI4G_NOT;
    sprintf((char *)buf, "AT+MQSUBM=1,1,0,4,\"v1/devices/me/rpc/request/+\"\r\n");
    Usart3_SendBuf(buf, strlen((char *)buf));
    if (Test_WIFI4G_CMD_Status(1000) == WIFI4G_ERROR) return 0;

    WIFI4G_CMD_Status = WIFI4G_NOT;
    strcpy((char *)Parse_Substr, "MQTT_CONNECT:");
    strcpy((char *)buf, "AT+REST\r\n");
    Usart3_SendBuf(buf, strlen((char *)buf));
    { uint8_t _i; const char _s[] = "MQTT Server OK\r\n"; for (_i = 0; _s[_i]; _i++) { while(!(USART1->SR & 0x80)); USART1->DR = _s[_i]; } }
    return 1;
}
#endif

/**
  * @brief  MQTT 连接入口 (自动选择 ESP8266 或 ML307)
  * @retval 1=连接成功, 0=失败
  */
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

/**
  * @brief  上报传感器数据 (JSON 格式)
  * @note   ESP8266 使用 MQTTPUBRAW 发送原始数据
  *         ML307 直接通过 USART3 发送 JSON 字符串
  *         Topic: STM32V9/UPLoad/{CPUID}
  *         数据: {"TP":温度, "RH":湿度, "VO":电压, "CU":电流,
  *                "PW":功率, "VR":MCU电压, "CPU":MCU温度}
  */
uint8_t MQTT_SendData(void)
{
    uint16_t r1, r2, r3, r4, r5, r6, r7;   /* local snapshot */
#if MQTT_WIFI_4G_ENABLE
    uint8_t cmd[128];
    uint8_t payload[128];
    uint16_t len;
#else
    uint8_t buf[256];
#endif

    REG_Lock();
    r1 = REG_HOLD_BUF[REG_IDX_TEMP];
    r2 = REG_HOLD_BUF[REG_IDX_HUMI];
    r3 = REG_HOLD_BUF[REG_IDX_DEV_VOLT];
    r4 = REG_HOLD_BUF[REG_IDX_DEV_CURR];
    r5 = REG_HOLD_BUF[REG_IDX_DEV_POWER];
    r6 = REG_HOLD_BUF[REG_IDX_SYS_VOLT];
    r7 = REG_HOLD_BUF[REG_IDX_CPU_TEMP];
    REG_Unlock();

#if MQTT_WIFI_4G_ENABLE
    sprintf((char *)payload,
        "{\"TP\":%d,\"RH\":%d,\"VO\":%d,\"CU\":%d,\"PW\":%d,\"VR\":%d,\"CPU\":%d}",
        r1, r2, r3, r4, r5, r6, r7);
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
    sprintf((char *)buf,
        "MQPUB,1,v1/devices/me/telemetry,{TP:%d,RH:%d,VO:%d,CU:%d,PW:%d,VR:%d,CPU:%d}",
        r1, r2, r3, r4, r5, r6, r7);
    Usart3_SendBuf(buf, strlen((char *)buf));
#endif
    return 1;
}

static uint8_t MQTT_SendRpcResponse(uint32_t request_id, const char *status)
{
    uint8_t buf[128];

    if ((request_id == 0) || (status == NULL))
        return 0;

#if MQTT_WIFI_4G_ENABLE
    {
        uint8_t cmd[128];
        uint16_t len = strlen(status);

        sprintf((char *)cmd,
            "AT+MQTTPUBRAW=0,\"v1/devices/me/rpc/response/%lu\",%d,0,0\r\n",
            request_id, len);
        strcpy((char *)Parse_Substr, ">");
        WIFI4G_CMD_Status = WIFI4G_NOT;
        Usart3_SendBuf(cmd, strlen((char *)cmd));
        if (Test_WIFI4G_CMD_Status(2000) != WIFI4G_OK)
            return 0;

        strcpy((char *)Parse_Substr, "OK\r\n");
        WIFI4G_CMD_Status = WIFI4G_NOT;
        Usart3_SendBuf((uint8_t *)status, len);
        if (Test_WIFI4G_CMD_Status(5000) != WIFI4G_OK)
            return 0;
    }
#else
    sprintf((char *)buf, "MQPUB,1,v1/devices/me/rpc/response/%lu,%s",
            request_id, status);
    Usart3_SendBuf(buf, strlen((char *)buf));
#endif
    return 1;
}

uint8_t MQTT_Parse_DeviceData(uint8_t *json, uint32_t request_id)
{
    cJSON *root;
    cJSON *method;
    cJSON *params;
    uint8_t state;
    uint8_t changed = 0;

    root = cJSON_Parse((char *)json);
    if (root == NULL)
        return 0;

    method = cJSON_GetObjectItem(root, "method");
    params = cJSON_GetObjectItem(root, "params");
    if ((method == NULL) || (method->valuestring == NULL)) {
        U1_printf("RPC parse no method\r\n");
        cJSON_Delete(root);
        return 0;
    }
    U1_printf("RPC method=%s\r\n", method->valuestring);

    if (strcmp(method->valuestring, "led1Status") == 0) {
        REG_Lock();
        state = (REG_HOLD_BUF[REG_IDX_OUTPUT] & LED1_CMD) ? 1 : 0;
        REG_Unlock();
        MQTT_SendRpcResponse(request_id, state ? "true" : "false");
        cJSON_Delete(root);
        return 1;
    } else if (strcmp(method->valuestring, "beepStatus") == 0) {
        REG_Lock();
        state = (REG_HOLD_BUF[REG_IDX_OUTPUT] & BEEP_CMD) ? 1 : 0;
        REG_Unlock();
        MQTT_SendRpcResponse(request_id, state ? "true" : "false");
        cJSON_Delete(root);
        return 1;
    } else if (strcmp(method->valuestring, "relayStatus") == 0) {
        REG_Lock();
        state = (REG_HOLD_BUF[REG_IDX_OUTPUT] & RELAY_CMD) ? 1 : 0;
        REG_Unlock();
        MQTT_SendRpcResponse(request_id, state ? "true" : "false");
        cJSON_Delete(root);
        return 1;
    }

    if (params == NULL) {
        cJSON_Delete(root);
        return 0;
    }

    {
        /* ── ThingsBoard RPC params 可能是直接值或者 {state:…} 对象 ── */
        cJSON *stateItem = cJSON_GetObjectItem(params, "state");
        if (stateItem) {
            /* params = {"state": true}  对象格式 */
            if (cJSON_IsBool(stateItem))
                state = cJSON_IsTrue(stateItem) ? 1 : 0;
            else
                state = (stateItem->valueint != 0) ? 1 : 0;
        } else {
            /* params = true / false / 1 / 0  直接值格式 */
            if (cJSON_IsBool(params))
                state = cJSON_IsTrue(params) ? 1 : 0;
            else
                state = (params->valueint != 0) ? 1 : 0;
        }
    }

    if (strcmp(method->valuestring, "led1Set") == 0) {
        REG_Lock();
        if (state)
            REG_HOLD_BUF[REG_IDX_OUTPUT] |= LED1_CMD;
        else
            REG_HOLD_BUF[REG_IDX_OUTPUT] &= (uint16_t)~LED1_CMD;
        REG_Unlock();
        changed = 1;
    } else if (strcmp(method->valuestring, "beepSet") == 0) {
        REG_Lock();
        if (state)
            REG_HOLD_BUF[REG_IDX_OUTPUT] |= BEEP_CMD;
        else
            REG_HOLD_BUF[REG_IDX_OUTPUT] &= (uint16_t)~BEEP_CMD;
        REG_Unlock();
        changed = 1;
    } else if (strcmp(method->valuestring, "relaySet") == 0) {
        REG_Lock();
        if (state)
            REG_HOLD_BUF[REG_IDX_OUTPUT] |= RELAY_CMD;
        else
            REG_HOLD_BUF[REG_IDX_OUTPUT] &= (uint16_t)~RELAY_CMD;
        REG_Unlock();
        changed = 1;
    }

    if (changed) {
        uint16_t hold0;

        REG_Lock();
        hold0 = REG_HOLD_BUF[REG_IDX_OUTPUT];
        REG_Unlock();
        App_Output_RefreshFromSharedRegs();
        U1_printf("RPC hold0=%04X\r\n", hold0);
        MQTT_SendRpcResponse(request_id, state ? "true" : "false");
    }

    cJSON_Delete(root);
    return changed;
}

/**
  * @brief  解析 MQTT 下行 JSON，控制输出
  * @param  json  待解析的 JSON 字符串 (如 {"LED1":1,"BEEP":0})
  * @retval 1=有输出变更, 0=无变更
  * @note   支持的键: LED1, LED2, BEEP, RELAY
  *         值 1/true = 打开, 0/false = 关闭
  */
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

    REG_Lock();
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
        if ((*p == '1') || (strncmp(p, "true", 4) == 0)) {
            REG_HOLD_BUF[REG_IDX_OUTPUT] |= cmds[i].mask;
            changed = 1;
        } else if ((*p == '0') || (strncmp(p, "false", 5) == 0)) {
            REG_HOLD_BUF[REG_IDX_OUTPUT] &= (uint16_t)~cmds[i].mask;
            changed = 1;
        }
    }
    if (changed) {
        REG_Unlock();
        App_Output_RefreshFromSharedRegs();
    } else {
        REG_Unlock();
    }
    return changed;
}
