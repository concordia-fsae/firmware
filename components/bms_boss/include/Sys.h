/**
 * @file Sys.h
 * @brief  Header file for System Manager
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

/**< Firmware Includes */
#include "HW_gpio.h"

#include "FloatTypes.h"
#include "stdbool.h"
#include "stdint.h"


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    SYS_INIT = 0x00,
    SYS_RUNNING,
    SYS_ERROR,
} SYS_state_E;

typedef enum {
    SYS_CONTACTORS_OPEN = 0x00,
    SYS_CONTACTORS_PRECHARGE,
    SYS_CONTACTORS_CLOSED,
    SYS_CONTACTORS_HVP_CLOSED,
} SYS_Contactors_E;


typedef struct
{
    SYS_state_E  state;
    SYS_Contactors_E contacts;
    struct
    {
        uint32_t last_message;
        float32_t voltage;
    }ts;
    struct
    {
        uint32_t last_message;
        float32_t voltage;
    }charger;
} SYS_S;

extern SYS_S SYS;


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

void SYS_SFT_openShutdown(void);
void SYS_SFT_closeShutdown(void);
void SYS_SFT_openContactors(void);
void SYS_SFT_cycleContacts(void);
void SYS_SFT_setTSVoltage(float32_t v);
bool SYS_SFT_checkMCTimeout(void);
void SYS_SFT_setChargerVoltage(float32_t v);
bool SYS_SFT_checkChargerTimeout(void);
void SYS_stopCharging(void);
void SYS_continueCharging(void);
