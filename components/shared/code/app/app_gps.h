/**
 * @file app_gps.h
 * @brief Header file for GPS application
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "LIB_Types.h"
#include "ModuleDesc.h"

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    float32_t lat;
    float32_t lon;
    float32_t alt;
} app_gps_pos_S;

typedef struct
{
    float32_t course;
    float32_t speedMps;
} app_gps_heading_S;

typedef struct
{
    uint8_t date;
    uint8_t month;
    uint16_t year;

    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
} app_gps_time_S;

typedef struct
{
    uint32_t utcMs;
    uint8_t drStage;
    uint8_t dynamicStatus;
    uint8_t alarmStatus;
} app_gps_pairmsg_S;

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern const ModuleDesc_S app_gps_desc;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void app_gps_resetBuffers(void);

void app_gps_getPos(app_gps_pos_S* pos);
void app_gps_getHeading(app_gps_heading_S* heading);
void app_gps_getTime(app_gps_time_S* time);
void app_gps_getPairmsg(app_gps_pairmsg_S* pairmsg);

// Not thread safe
app_gps_pos_S*  app_gps_getPosRef(void);
app_gps_heading_S*  app_gps_getHeadingRef(void);
app_gps_time_S* app_gps_getTimeRef(void);
app_gps_pairmsg_S* app_gps_getPairmsgRef(void);

bool app_gps_isValid(void);
uint16_t app_gps_getCrcFailures(void);
uint16_t app_gps_getInvalidTransactions(void);
uint16_t app_gps_getNumberSamples(void);
uint16_t app_gps_getUartErrorOreCount(void);
uint16_t app_gps_getUartErrorFeCount(void);
uint16_t app_gps_getUartErrorNeCount(void);
uint16_t app_gps_getUartErrorPeCount(void);
void app_gps_recordUartError(uint32_t errorCode);
