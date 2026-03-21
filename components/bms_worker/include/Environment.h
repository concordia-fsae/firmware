/**
 * @file Environment.h
 * @brief  Header file for Environment sensors
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// System Includes
#include "stdbool.h"
#include "stdint.h"

// Other Includes
#include "LIB_Types.h"

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    CH1 = 0x00,
    CH2,
    CH3,
    CH4,
    CH5,
    CH6,
    CH7,
    CH8,
    CH9,
#if APP_VARIANT_ID == 0U
    CH10,
    CH11,
    CH12,
    CH13,
    CH14,
    CH15,
    CH16,
    CH17,
    CH18,
    CH19,
    CH20,
#endif
    CHANNEL_COUNT,
} ENV_thermistorID_E;

typedef struct
{
    bool    therm_error;
    float32_t temp;
} ENV_temperature_S;

typedef struct
{
    bool fault;
    struct
    {
        struct
        {
            float32_t ambient_temp;
            float32_t rh;
        } board;
        ENV_temperature_S temps[CHANNEL_COUNT];
        float32_t       max_temp;
        float32_t       min_temp;
        float32_t       avg_temp;
    } values;
    bool startRhHeater;
} ENV_S;


/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern ENV_S ENV;
