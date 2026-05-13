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
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
#include <stdio.h>

volatile uint8_t MQTT_Download_Flag = 0;
volatile uint8_t MQTT_OTA_FLAG = 0;
OTA_FW_Info_t OTA_Info;

#define OTA_W25_BLOCK_SIZE        (64UL * 1024UL)
#define OTA_W25_TARGET_BLOCK      0UL  /* Bootloader 自动升级从 block 0 读取 */
#define OTA_W25_BASE_ADDR         (OTA_W25_TARGET_BLOCK * OTA_W25_BLOCK_SIZE)
#define OTA_EE_COMPAT_ADDR        0U
#define OTA_EE_COMPAT_SIZE        80U
#define OTA_EE_EXT_ADDR           80U
#define OTA_EE_EXT_SIZE           16U
#define OTA_READY_MAGIC           0x4F544132UL

typedef struct
{
    uint32_t ota_flag;
    uint32_t firelen[11];
    uint8_t ota_ver[32];
} APP_OTA_CompatInfo_t;

typedef struct
{
    uint32_t ready_magic;
    uint32_t target_block;
    uint32_t fw_crc32;
    uint32_t fw_size;
} APP_OTA_ExtInfo_t;

static uint8_t MQTT_ParseQuotedField(const char *json, const char *key, char *out, uint16_t out_len)
{
    char pattern[32];
    char *p;
    uint16_t i = 0;

    if ((out == NULL) || (out_len == 0))
        return 0;

    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    p = strstr((char *)json, pattern);
    if (p == NULL)
        return 0;
    p = strchr(p + strlen(pattern), ':');
    if (p == NULL)
        return 0;
    p++;
    while ((*p == ' ') || (*p == '\t'))
        p++;
    if (*p != '"')
        return 0;
    p++;
    while ((*p != '\0') && (*p != '"') && (i + 1 < out_len))
        out[i++] = *p++;
    if (*p != '"')
        return 0;
    out[i] = '\0';
    return 1;
}

static uint8_t MQTT_ParseUintField(const char *json, const char *key, uint32_t *value)
{
    char pattern[32];
    char *p;
    uint32_t result = 0;

    if (value == NULL)
        return 0;

    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    p = strstr((char *)json, pattern);
    if (p == NULL)
        return 0;
    p = strchr(p + strlen(pattern), ':');
    if (p == NULL)
        return 0;
    p++;
    while ((*p == ' ') || (*p == '\t'))
        p++;
    if ((*p < '0') || (*p > '9'))
        return 0;
    while ((*p >= '0') && (*p <= '9')) {
        result = result * 10UL + (uint32_t)(*p - '0');
        p++;
    }
    *value = result;
    return 1;
}

static uint8_t MQTT_ParseHex32(const char *text, uint32_t *value)
{
    uint32_t result = 0;
    uint8_t digit;

    if ((text == NULL) || (value == NULL))
        return 0;

    while ((*text == ' ') || (*text == '\t'))
        text++;
    if ((text[0] == '0') && ((text[1] == 'x') || (text[1] == 'X')))
        text += 2;
    if (*text == '\0')
        return 0;

    while (*text != '\0') {
        if ((*text >= '0') && (*text <= '9'))
            digit = (uint8_t)(*text - '0');
        else if ((*text >= 'a') && (*text <= 'f'))
            digit = (uint8_t)(*text - 'a' + 10);
        else if ((*text >= 'A') && (*text <= 'F'))
            digit = (uint8_t)(*text - 'A' + 10);
        else
            return 0;
        result = (result << 4) | digit;
        text++;
    }

    *value = result;
    return 1;
}

static void OTA_CRC32_Init(uint32_t *crc)
{
    *crc = 0xFFFFFFFFUL;
}

static void OTA_CRC32_Update(uint32_t *crc, const uint8_t *data, uint32_t len)
{
    uint32_t c = *crc;
    uint32_t i;
    uint8_t j;

    for (i = 0; i < len; i++) {
        c ^= data[i];
        for (j = 0; j < 8; j++) {
            c = (c >> 1) ^ (0xEDB88320UL & (uint32_t)(-(int32_t)(c & 1UL)));
        }
    }
    *crc = c;
}

static uint32_t OTA_CRC32_Final(uint32_t crc)
{
    return crc ^ 0xFFFFFFFFUL;
}

static void OTA_W25_Write(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    uint16_t chunk;
    uint16_t page_off;

    while (len > 0UL) {
        page_off = (uint16_t)(addr % W25Q64_PAGE_SIZE);
        chunk = (uint16_t)(W25Q64_PAGE_SIZE - page_off);
        if (chunk > len)
            chunk = (uint16_t)len;
        W25Q64_PageProgram(addr, buf, chunk);
        addr += chunk;
        buf += chunk;
        len -= chunk;
    }
}

static uint8_t OTA_WriteMetadata(uint32_t fw_crc32)
{
    uint8_t page_buf[96];
    APP_OTA_CompatInfo_t *compat;
    APP_OTA_ExtInfo_t *ext;
    uint8_t page;

    memset(page_buf, 0, sizeof(page_buf));
    AT24C02_ReadData(OTA_EE_COMPAT_ADDR, page_buf, sizeof(page_buf));

    compat = (APP_OTA_CompatInfo_t *)page_buf;
    ext = (APP_OTA_ExtInfo_t *)(page_buf + OTA_EE_EXT_ADDR);

    compat->ota_flag = 0xAABB1122UL;  /* Bootloader 要求的值 (OTA_SET_FLAG) */
    compat->firelen[0] = OTA_Info.fw_size; /* Bootloader 用 Firelen[0] 获取总体长度 */
    memset(compat->ota_ver, 0, sizeof(compat->ota_ver));
    strncpy((char *)compat->ota_ver, OTA_Info.fw_version, sizeof(compat->ota_ver) - 1U);

    ext->ready_magic = OTA_READY_MAGIC;
    ext->target_block = OTA_W25_TARGET_BLOCK;
    ext->fw_crc32 = fw_crc32;
    ext->fw_size = OTA_Info.fw_size;

    for (page = 0; page < (sizeof(page_buf) / 16U); page++) {
        AT24C02_WritePage((uint8_t)(page * 16U), &page_buf[page * 16U]);
        Delay_ms(5);
    }

    return 1;
}

static void MQTT_RequestOtaChunk(void)
{
    uint8_t buf[96];

    /* 固定请求 OTA_CHUNK_SIZE 字节，最后一包云端返回实际余数字节。
       OTA_Info.request_bytes 仍表示本包期望/写入字节数，用于响应匹配和 W25Q64 写入。 */
    sprintf((char *)buf, "MQPUB,1,v2/fw/request/%lu/chunk/%lu,%u",
            OTA_Info.request_id, OTA_Info.chunk_id, OTA_CHUNK_SIZE);
    Usart3_SendBuf(buf, strlen((char *)buf));
    if ((OTA_Info.chunk_id == 0UL) || ((OTA_Info.chunk_id % 16UL) == 0UL) ||
        (OTA_Info.request_bytes != OTA_CHUNK_SIZE)) {
        U1_printf("OTA req %lu/%lu len=%lu\r\n",
                  OTA_Info.request_id, OTA_Info.chunk_id, OTA_Info.request_bytes);
    }
}

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

    /* 订阅 OTA 固件块响应 topic */
    WIFI4G_CMD_Status = WIFI4G_NOT;
    sprintf((char *)buf, "AT+MQSUBM=2,1,0,4,\"v2/fw/response/+/chunk/#\"\r\n");
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
    char method[32];
    uint8_t state = 0;
    uint8_t changed = 0;
    char *p;
    int i;

    /* ── 1. Locate "method" key and extract its string value ── */
    p = strstr((char *)json, "\"method\"");
    if (!p) {
        U1_printf("RPC parse no method\r\n");
        return 0;
    }
    p += 8; /* skip past "\"method\"" */
    while (*p == ' ' || *p == '\t') p++;
    if (*p != ':')  { U1_printf("RPC parse no method\r\n"); return 0; }
    p++;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '"')  { U1_printf("RPC parse no method\r\n"); return 0; }
    p++; /* skip opening quote */
    for (i = 0; *p && *p != '"' && i < (int)sizeof(method) - 1; i++)
        method[i] = *p++;
    if (*p != '"')  { U1_printf("RPC parse no method\r\n"); return 0; }
    method[i] = '\0';
    /* ── 2. Query methods (no params needed) ── */
    if (strcmp(method, "led1Status") == 0) {
        REG_Lock();
        state = (REG_HOLD_BUF[REG_IDX_OUTPUT] & LED1_CMD) ? 1 : 0;
        REG_Unlock();
        MQTT_SendRpcResponse(request_id, state ? "true" : "false");
        return 1;
    } else if (strcmp(method, "beepStatus") == 0) {
        REG_Lock();
        state = (REG_HOLD_BUF[REG_IDX_OUTPUT] & BEEP_CMD) ? 1 : 0;
        REG_Unlock();
        MQTT_SendRpcResponse(request_id, state ? "true" : "false");
        return 1;
    } else if (strcmp(method, "relayStatus") == 0) {
        REG_Lock();
        state = (REG_HOLD_BUF[REG_IDX_OUTPUT] & RELAY_CMD) ? 1 : 0;
        REG_Unlock();
        MQTT_SendRpcResponse(request_id, state ? "true" : "false");
        return 1;
    }

    /* ── 3. Set methods: locate "params" key ── */
    p = strstr((char *)json, "\"params\"");
    if (!p) return 0;
    p += 8; /* skip past "\"params\"" */
    while (*p == ' ' || *p == '\t') p++;
    if (*p != ':') return 0;
    p++;
    while (*p == ' ' || *p == '\t') p++;

    /* ── 4. Determine boolean state from params ── */
    /* Object form: {"state": true}  -> skip { and look for inner key */
    if (*p == '{') {
        p = strstr(p + 1, "\"state\"");
        if (!p) return 0;
        p += 7; /* skip past "\"state\"" */
        while (*p == ' ' || *p == '\t') p++;
        if (*p != ':') return 0;
        p++;
        while (*p == ' ' || *p == '\t') p++;
    }
    /* Direct form: true / false / 1 / 0 */
    if (*p == 't' && strncmp(p, "true", 4) == 0)
        state = 1;
    else if (*p == 'f' && strncmp(p, "false", 5) == 0)
        state = 0;
    else if (*p == '1')
        state = 1;
    else if (*p == '0')
        state = 0;
    else
        return 0;

    /* ── 5. Apply set methods ── */
    if (strcmp(method, "led1Set") == 0) {
        REG_Lock();
        if (state)
            REG_HOLD_BUF[REG_IDX_OUTPUT] |= LED1_CMD;
        else
            REG_HOLD_BUF[REG_IDX_OUTPUT] &= (uint16_t)~LED1_CMD;
        REG_Unlock();
        changed = 1;
    } else if (strcmp(method, "beepSet") == 0) {
        REG_Lock();
        if (state)
            REG_HOLD_BUF[REG_IDX_OUTPUT] |= BEEP_CMD;
        else
            REG_HOLD_BUF[REG_IDX_OUTPUT] &= (uint16_t)~BEEP_CMD;
        REG_Unlock();
        changed = 1;
    } else if (strcmp(method, "relaySet") == 0) {
        REG_Lock();
        if (state)
            REG_HOLD_BUF[REG_IDX_OUTPUT] |= RELAY_CMD;
        else
            REG_HOLD_BUF[REG_IDX_OUTPUT] &= (uint16_t)~RELAY_CMD;
        REG_Unlock();
        changed = 1;
    }

    if (changed) {
        App_Output_RefreshFromSharedRegs();
        MQTT_SendRpcResponse(request_id, state ? "true" : "false");
    }

    return changed;
}

/**
  * @brief  处理 OTA 固件块响应数据
  * @param  buf  包含块数据的缓冲区
  * @param  len  数据长度
  * @note   由 WIFI4G_Parse_Queue 在检测到 chunk 响应时调用
  */
void MQTT_HandleChunkResponse(uint8_t *buf, uint16_t len)
{
    if (len > sizeof(OTA_Info.recv_buf))
        len = sizeof(OTA_Info.recv_buf);
    memcpy(OTA_Info.recv_buf, buf, len);
    OTA_Info.recv_flag = 1;
}

uint8_t MQTT_Parse_OTAData(uint8_t *json)
{
    uint32_t fw_size;

    if (!MQTT_ParseQuotedField((char *)json, "fw_title", OTA_Info.fw_title, sizeof(OTA_Info.fw_title)))
        return 0;
    if (!MQTT_ParseQuotedField((char *)json, "fw_version", OTA_Info.fw_version, sizeof(OTA_Info.fw_version)))
        return 0;
    if (!MQTT_ParseQuotedField((char *)json, "fw_checksum", OTA_Info.fw_checksum, sizeof(OTA_Info.fw_checksum)))
        return 0;
    if (!MQTT_ParseUintField((char *)json, "fw_size", &fw_size))
        return 0;

    OTA_Info.fw_size = fw_size;
    OTA_Info.recv_flag = 0;
    OTA_Info.received_bytes = 0;
    OTA_Info.chunk_id = 0;
    OTA_Info.request_bytes = OTA_CHUNK_SIZE;
    MQTT_OTA_FLAG = 1;
    U1_printf("OTA meta ver=%s size=%lu crc=%s\r\n",
              OTA_Info.fw_version, OTA_Info.fw_size, OTA_Info.fw_checksum);
    return 1;
}

uint8_t MQTT_OTA_GetFW(void)
{
    uint32_t total_chunks;
    uint32_t i;
    uint32_t timeout;
    uint32_t crc32;
    uint32_t fw_crc32;
    uint32_t expect_crc32;
    uint32_t chunk_len;

    if ((OTA_Info.fw_size == 0UL) || (OTA_Info.fw_size > OTA_W25_BLOCK_SIZE)) {
        U1_printf("OTA size invalid:%lu\r\n", OTA_Info.fw_size);
        return 0;
    }

    W25Q64_EraseBlock64K(OTA_W25_BASE_ADDR);
    OTA_Info.request_id = (uint32_t)xTaskGetTickCount();
    if (OTA_Info.request_id == 0)
        OTA_Info.request_id = 1;
    OTA_Info.chunk_id = 0;
    OTA_Info.received_bytes = 0;

    total_chunks = OTA_Info.fw_size / OTA_CHUNK_SIZE;
    if ((OTA_Info.fw_size % OTA_CHUNK_SIZE) != 0UL)
        total_chunks++;

    OTA_CRC32_Init(&crc32);
    OTA_ClearAccum();  /* 清空跨调用累积缓冲，准备接收 chunk 响应 */
    for (i = 0; i < total_chunks; i++, OTA_Info.chunk_id++) {
        OTA_Info.request_bytes = ((i == (total_chunks - 1U)) && ((OTA_Info.fw_size % OTA_CHUNK_SIZE) != 0UL))
                               ? (OTA_Info.fw_size % OTA_CHUNK_SIZE)
                               : OTA_CHUNK_SIZE;
        {
            uint8_t chunk_retry = 0;
            retry_chunk:
            OTA_Info.recv_flag = 0;
            OTA_ClearAccum();
            MQTT_RequestOtaChunk();
            timeout = 60000UL;
            while (OTA_Info.recv_flag == 0U) {
                WIFI4G_Parse_Queue();
                if (timeout-- == 0UL) {
                    chunk_retry++;
                    if (chunk_retry >= 5) {
                        U1_printf("OTA chunk %lu fail after %u retries\r\n", i, chunk_retry);
                        return 0;
                    }
                    U1_printf("OTA chunk %lu retry %u\r\n", i, chunk_retry);
                    /* 重试重发当前 chunk，保持本轮 request_id 与参考工程一致。 */
                    goto retry_chunk;
                }
                Delay_ms(1);
            }
        }

        chunk_len = OTA_Info.request_bytes;
        if (i == 0) {
            U1_printf("CRC raw[0..15]: ");
            for (uint8_t _d = 0; _d < 16 && _d < chunk_len; _d++)
                U1_printf("%02X ", OTA_Info.recv_buf[_d]);
            U1_printf("\r\n");
        }
        OTA_CRC32_Update(&crc32, OTA_Info.recv_buf, chunk_len);
        OTA_W25_Write(OTA_W25_BASE_ADDR + OTA_Info.received_bytes, OTA_Info.recv_buf, chunk_len);
        OTA_Info.received_bytes += chunk_len;
    }

    fw_crc32 = OTA_CRC32_Final(crc32);
    if (!MQTT_ParseHex32(OTA_Info.fw_checksum, &expect_crc32)) {
        U1_printf("OTA checksum invalid:%s\r\n", OTA_Info.fw_checksum);
        return 0;
    }
    {
        /* 尝试原序和字节交换都匹配；如果都不匹配则警告但继续（Bootloader 硬件 CRC 会最终校验） */
        uint32_t fw_crc32_swapped = ((fw_crc32 >> 24) & 0xFF) | ((fw_crc32 >> 8) & 0xFF00) |
                                     ((fw_crc32 << 8) & 0xFF0000) | (fw_crc32 << 24);
        if (expect_crc32 != fw_crc32 && expect_crc32 != fw_crc32_swapped) {
            U1_printf("OTA crc warn calc=%08lX exp=%s (proceed anyway)\r\n",
                     fw_crc32, OTA_Info.fw_checksum);
            /* 使用云端的 CRC 写入元数据，Bootloader 会用硬件 CRC 校验 */
            fw_crc32 = expect_crc32;
        } else if (expect_crc32 == fw_crc32_swapped) {
            fw_crc32 = expect_crc32;
        }
    }

    OTA_WriteMetadata(fw_crc32);
    MQTT_OTA_FLAG = 0;
    U1_printf("OTA ready block2 len=%lu crc=%08lX\r\n", OTA_Info.fw_size, fw_crc32);
    U1_printf("OTA reboot for bootloader select\r\n");
    Delay_ms(100);
    NVIC_SystemReset();
    return 1;
}

void MQTT_OTA_Process(void)
{
    static uint8_t ota_busy;

    /* MQTT 断开时不处理 OTA，等重连 */
    if (!MQTT_OTA_FLAG || ota_busy || !MQTT_Download_Flag)
        return;

    ota_busy = 1;
    if (!MQTT_OTA_GetFW())
        U1_printf("OTA download fail\r\n");
    ota_busy = 0;
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
