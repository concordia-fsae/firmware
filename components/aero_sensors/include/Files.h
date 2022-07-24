/**
 * @file Files.h
 * @brief  Header file for filesystem interface
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @version 0.1
 * @date 2022-07-23
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include <stdint.h>


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    FS_READY = 0x00,
    FS_BUSY,
    FS_CHANGING_FILE,
} FS_State_E;


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void Files_Init(void);
void Files_Write(const void* buff, uint32_t count);
void Files_Next(void);
