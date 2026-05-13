#ifndef MQTT_H
#define MQTT_H

#include <stdint.h>

#define MQTT_WIFI_4G_ENABLE  0  /* 1=ESP8266(WiFi), 0=ML307(4G Cat.1) */

/* ThingsBoard 下行 topic 模式 */
#define TB_ATTR_TOPIC       "v1/devices/me/attributes"
#define TB_RPC_TOPIC        "v1/devices/me/rpc/request/+"
#define TB_FW_RESP_TOPIC    "v2/fw/response/+/chunk/#"

#define OTA_CHUNK_SIZE      256U

extern volatile uint8_t MQTT_Download_Flag;
extern volatile uint8_t MQTT_OTA_FLAG;

/* OTA 元数据与下载状态（wifi4g.c 中引用） */
typedef struct
{
    char fw_title[32];
    char fw_version[32];
    uint32_t fw_size;
    char fw_checksum[16];
    uint8_t recv_buf[OTA_CHUNK_SIZE];
    uint8_t recv_flag;
    uint32_t request_id;
    uint32_t chunk_id;
    uint32_t request_bytes;
    uint32_t received_bytes;
} OTA_FW_Info_t;

extern OTA_FW_Info_t OTA_Info;

uint8_t MQTT_Connect_Server(void);
uint8_t MQTT_SendData(void);       /* 上报 JSON 传感器数据 */
uint8_t MQTT_Parse_JsonData(uint8_t *json);  /* 解析下行 JSON 控制 */
uint8_t MQTT_Parse_DeviceData(uint8_t *json, uint32_t request_id);

/* OTA 解析入口（由 wifi4g 解析层调用） */
uint8_t MQTT_Parse_OTAData(uint8_t *json);
void    MQTT_OTA_Process(void);

#endif
