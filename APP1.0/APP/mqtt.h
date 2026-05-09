#ifndef MQTT_H
#define MQTT_H

#include <stdint.h>

#define MQTT_WIFI_4G_ENABLE  1  /* 1=ESP8266, 0=ML307 */

extern volatile uint8_t MQTT_Download_Flag;

uint8_t MQTT_Connect_Server(void);
uint8_t MQTT_SendData(void);
uint8_t MQTT_Parse_JsonData(uint8_t *json);

#endif
