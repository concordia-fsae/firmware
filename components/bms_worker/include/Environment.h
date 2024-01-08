/**
 * @file Environment.h
 * @brief  Header file for Environment sensors
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @version 
 * @date 2023-12-27
 */

#pragma once


/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW_LTC2983.h"

#include "stdint.h"
#include "Module.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum {
    ENV_INIT = 0x00,
    ENV_RUNNING,
    ENV_ERROR,
} Environment_State_E;

typedef struct {
    struct {
        uint16_t mcu_temp; /**< Stored in 0.1 deg C */
        int16_t ambient_temp; /**< Stored in 0.1 deg C */
        uint16_t rh; /**< Stored in 0.01% RH */
    } board;
    struct {
        int16_t cell_temps[CHANNEL_COUNT]; /**< Stored in 0.1 deg C */ 
        int16_t max_temp;
        int16_t min_temp;
        int16_t avg_temp;
    } cells;
} Env_Variables_S;

typedef struct {
    Environment_State_E state;
    Env_Variables_S values;
} Environment_S;

/******************************************************************************
 *                               M A C R O S
 ******************************************************************************/

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/