#ifndef INA226_H
#define INA226_H

#include <stdint.h>

#define INA226_ADDR                   0x82

#define CONFIG_REGISTER               0x00
#define SHUNT_VOLTAGE_REGISTER        0x01
#define BUS_VOLTAGE_REGISTER          0x02
#define POWER_REGISTER                0x03
#define CURRENT_REGISTER              0x04
#define CALIBRATION_REGISTER          0x05

#define CONFIG_REGISTER_INIT          0x4527  /* 16avg, 1.1ms CT, ~35ms/cycle */
#define CALIBRATION_REGISTER_INIT     5120

#define BUS_VOLTAGE_LSB               1.25f
#define CURRENT_LSB                   0.1f
#define POWER_LSB                     (0.1f * 25.0f)

uint8_t INA226_Init(void);
uint8_t INA226_WriteReg(uint8_t reg, uint16_t value);
uint16_t INA226_Read_BusVoltage(void);
uint16_t INA226_Read_Current(void);
uint16_t INA226_Read_Power(void);
uint16_t INA226_Read_Config(void);
uint16_t INA226_Read_Calibration(void);
uint16_t INA226_Read_ManufacturerID(void);
uint16_t INA226_Read_DieID(void);
uint8_t INA226_GetLastError(void);

#endif
