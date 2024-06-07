/**
 * @file HW_HIH.h
 * @brief  Header file for HIH driver
 */

#include <stdbool.h>
#include <stdint.h>

#include "HW_i2c.h"

typedef enum {
    HIH_INIT = 0x00,
    HIH_MEASURING,
    HIH_WAITING,
    HIH_ERROR,
} HIH_State_E;

typedef struct {
    uint16_t temp;
    uint16_t rh;
    HIH_State_E state;
} HIH_Data_S;

typedef struct {
    HW_I2C_Device_S* dev;
    HIH_Data_S data;
} HIH_S;

extern HIH_S hih_chip;

bool HIH_init(void);
bool HIH_getData(void);
bool HIH_startConversion(void);

