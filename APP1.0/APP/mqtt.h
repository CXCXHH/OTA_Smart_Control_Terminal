#ifndef MQTT_H
#define MQTT_H

#include <stdint.h>

#define MQTT_WIFI_4G_ENABLE  0  /* 1=ESP8266(WiFi), 0=ML307(4G Cat.1) */

extern volatile uint8_t MQTT_Download_Flag;

uint8_t MQTT_Connect_Server(void);
uint8_t MQTT_SendData(void);       /* 上报 JSON 传感器数据 */
uint8_t MQTT_Parse_JsonData(uint8_t *json);  /* 解析下行 JSON 控制 */
uint8_t MQTT_Parse_DeviceData(uint8_t *json, uint32_t request_id);

#endif
