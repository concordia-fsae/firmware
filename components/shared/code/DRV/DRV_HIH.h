/**
 * @file HW_HIH.h
 * @brief  Header file for HIH driver
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW_i2c.h"
#include "LIB_Types.h"

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum {
    DRV_HIH_INIT = 0x00,
    DRV_HIH_MEASURING,
    DRV_HIH_WAITING,
    DRV_HIH_ERROR,
} DRV_HIH_State_E;

typedef struct {
    uint16_t temp;
    uint16_t rh;
    DRV_HIH_State_E state;
} DRV_HIH_Data_S;

typedef struct {
    HW_I2C_Device_S* dev;
    DRV_HIH_Data_S data;
} DRV_HIH_S;

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern DRV_HIH_S hih_chip;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

bool DRV_HIH_init(void);
bool DRV_HIH_getData(void);
bool DRV_HIH_startConversion(void);

