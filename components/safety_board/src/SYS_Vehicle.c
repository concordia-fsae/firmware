/**
 * @file SYS_Vehicle.c
 * @brief  Source code for the Vehicle's Safety State Machine
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @version 0.1
 * @date 2023-01-16
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "SYS_Vehicle.h"

#include "SystemConfig.h"
#include <stdint.h>

#include "HW_can.h"


/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static uint8_t          status_flags      = 0;
static VEHICLE_STATES_E vehicle_state     = VEHICLE_INIT;
static int16_t          ts_voltage        = 0;    // Stored at 0.1 V increments
static int16_t          batt_voltage      = 0;    // Stored at 0.1 V increments
static uint32_t         last_ts_message   = 0;
static uint32_t         last_batt_message = 0;


/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

bool SYS_Validate(void);

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

void SYS_SAFETY_Init(void)
{
    SYS_CONTACTORS_OpenAll();

    if (HAL_GPIO_ReadPin(OK_HS_Port, OK_HS_Pin) == GPIO_PIN_SET)
    {
        SYS_SAFETY_SetStatus(IMD_STATUS, ON);
    }
    else
    {
        SYS_SAFETY_SetStatus(IMD_STATUS, OFF);
    }

    if (HAL_GPIO_ReadPin(TSMS_CHG_Port, TSMS_CHG_Pin) == GPIO_PIN_SET)
    {
        SYS_SAFETY_SetStatus(TSMS_STATUS, ON);
    }
    else
    {
        SYS_SAFETY_SetStatus(IMD_STATUS, OFF);
    }
}

void SYS_CONTACTORS_OpenAll(void)
{
    HAL_GPIO_WritePin(AIR_Port, AIR_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(PCHG_Port, PCHG_Pin, GPIO_PIN_RESET);
}

bool SYS_SAFETY_GetStatus(STATUS_INDEX_E status)
{
    return (status_flags & (0x01 << status));
}

void SYS_SAFETY_SetStatus(STATUS_INDEX_E status, STATUS_E state)
{
    if (state == ON)
    {
        status_flags |= 0x01 << status;

        if (status == IMD_STATUS)
        {
            HAL_GPIO_WritePin(IMD_STATUS_Port, IMD_STATUS_Pin, GPIO_PIN_SET);
        }
        else if (status == BMS_STATUS)
        {
            HAL_GPIO_WritePin(BMS_STATUS_Port, BMS_STATUS_Pin, GPIO_PIN_SET);
        }

        if ((status_flags & (0x01 << IMD_STATUS)) && ((status_flags & (0x01 << BMS_STATUS)) || status_flags & (0x01 << BMS_CHARGE_STATUS)))
        {
            vehicle_state = VEHICLE_READY;
        }

        if (vehicle_state == VEHICLE_READY && status_flags & (0x01 << TSMS_STATUS))
        {
            if (status_flags & (1 << BMS_CHARGE_STATUS))
            {
                // TODO: Complete charger heartbeat validation
                SYS_CONTACTORS_Switch(MAIN_CONTACTOR, ON);
                vehicle_state = CONTACTOR_MAIN_CLOSED;
            }
            else if (SYS_Validate())
            {
                SYS_CONTACTORS_Switch(PRECHARGE_CONTACTOR, ON);
                vehicle_state = CONTACTOR_PRECHARGE_CLOSED;
            }
        }
    }
    else
    {
        status_flags &= ~(0x01 << status);
        SYS_CONTACTORS_OpenAll();

        vehicle_state = VEHICLE_FAULT;

        if (status == IMD_STATUS)
        {
            HAL_GPIO_WritePin(IMD_STATUS_Port, IMD_STATUS_Pin, GPIO_PIN_RESET);
        }
        else if (status == BMS_STATUS)
        {
            HAL_GPIO_WritePin(BMS_STATUS_Port, BMS_STATUS_Pin, GPIO_PIN_RESET);
        }
    }
}

void SYS_CONTACTORS_Switch(CONTACTORS_E contactor, STATUS_E state)
{
    if (contactor == PRECHARGE_CONTACTOR)
    {
        HAL_GPIO_WritePin(PCHG_Port, PCHG_Pin, (state == ON) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
    else if (contactor == MAIN_CONTACTOR)
    {
        HAL_GPIO_WritePin(AIR_Port, AIR_Pin, (state == ON) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
}

void SYS_SAFETY_SetBatteryVoltage(int16_t v)
{
    batt_voltage      = v;
    last_batt_message = HAL_GetTick();
}

void SYS_SAFETY_SetBusVoltage(int16_t v)
{
    ts_voltage      = v;
    last_ts_message = HAL_GetTick();
    SYS_SAFETY_SetStatus(TS_STATUS, ON);
}

void SYS_SAFETY_CycleState(void)
{
    SYS_Validate();

    if (vehicle_state == CONTACTOR_PRECHARGE_CLOSED)
    {
        if (ts_voltage > (batt_voltage * 0.9))
        {
            vehicle_state = CONTACTOR_PRECHARGE_MAIN_CLOSED;

            SYS_CONTACTORS_Switch(MAIN_CONTACTOR, ON);
        }
    }
    else if (vehicle_state == CONTACTOR_PRECHARGE_MAIN_CLOSED)
    {
        vehicle_state = CONTACTOR_MAIN_CLOSED;

        SYS_CONTACTORS_Switch(PRECHARGE_CONTACTOR, OFF);
    }

    CAN_data_T data;

    data.u8[0] = status_flags;
    data.u8[1] = vehicle_state;
    data.u8[2] = (batt_voltage & 0xff00) >> 8;
    data.u8[3] = batt_voltage & 0xff;
    data.u8[4] = (ts_voltage & 0xff00) >> 8;
    data.u8[5] = ts_voltage & 0xff;


    CAN_sendMsgBus0(CAN_TX_PRIO_100HZ, data, SAFETY_BOARD_STATUS_ID, 6);
}

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

bool SYS_Validate(void)
{
    if (last_ts_message > (HAL_GetTick() - MOTOR_CONTROLLER_TIMEOUT) && last_batt_message > (HAL_GetTick() - BMS_TIMEOUT))
    {
        return true;
    }

    if (last_ts_message < (HAL_GetTick() - MOTOR_CONTROLLER_TIMEOUT))
    {
        SYS_SAFETY_SetStatus(MOTOR_CONTROLLER_TIMEOUT, OFF);
    }
    else if (last_batt_message < (HAL_GetTick() - BMS_TIMEOUT))
    {
        SYS_SAFETY_SetStatus(BMS_STATUS, OFF);
    }

    return false;
}
