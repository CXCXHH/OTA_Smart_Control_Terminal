#ifndef AHT20_H
#define AHT20_H

#include <stdint.h>

#define AHT20_SLAVE_ADDRESS  0x70   /* (0x38 << 1) */

#define AHT20_STATUS_REG          0x00
#define AHT20_INIT_REG            0xBE
#define AHT20_TRIGMEASURE_REG     0xAC

uint8_t AHT20_Init(void);
uint8_t AHT20_Read(uint16_t *temp_centi, uint16_t *rh_centi);

#endif
