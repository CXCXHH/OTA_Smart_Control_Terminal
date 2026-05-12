#ifndef SENSOR_APP_H
#define SENSOR_APP_H

/* 传感器应用: AHT20(温湿度) + INA226(电压/电流/功率) + MCU内部ADC */
void SensorApp_Init(void);
void SensorApp_Process(void);

#endif
