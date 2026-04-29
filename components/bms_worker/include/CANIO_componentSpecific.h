/**
 RX_config* CAN.h
 * Header file for CANRX configuration
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// imports for time base
#include "HW_tim.h"
#include "Yamcan.h"

// imports for data access
#include "cooling.h"
#include "Environment.h"
#include "BatteryMonitoring.h"
#include "Module.h"
#include "drv_tempSensors.h"
#include "app_faultManager.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define CANIO_UDS_BUFFER_LENGTH 8U
#define CANIO_getTimeMs() (HW_TIM_getTimeMS())

#define set_fault_message (*(CAN_data_T*)app_faultManager_transmit())

#define set_faultTemp(m,b,n,s)                   set(m,b,n,s, (ENV.fault) ? CAN_FLAG_SET : CAN_FLAG_CLEARED)
#define set_faultBMS(m, b, n, s)                 set(m,b,n,s, (BMS.fault || (BMS.state == BMS_ERROR)) ? CAN_FLAG_SET : CAN_FLAG_CLEARED)
#define set_tempMax(m, b, n, s)                  set(m,b,n,s, ENV.values.max_temp)
#define set_segmentVoltage(m, b, n, s)           set(m,b,n,s, BMS.pack_voltage)
#define set_voltageMax(m, b, n, s)               set(m,b,n,s, BMS.voltage.max)
#define set_voltageMin(m, b, n, s)               set(m,b,n,s, BMS.voltage.min)
#define set_tempAvg(m, b, n, s)                  set(m,b,n,s, ENV.values.avg_temp)
#define set_cellVoltageAvg(m, b, n, s)           set(m,b,n,s, BMS.voltage.avg)
#define set_cellTemp9(m, b, n, s)                set(m,b,n,s, ENV.values.temps[CH9].temp)
#define set_cellTemp8(m, b, n, s)                set(m,b,n,s, ENV.values.temps[CH8].temp)
#define set_cellTemp7(m, b, n, s)                set(m,b,n,s, ENV.values.temps[CH7].temp)
#define set_cellTemp6(m, b, n, s)                set(m,b,n,s, ENV.values.temps[CH6].temp)
#define set_cellTemp5(m, b, n, s)                set(m,b,n,s, ENV.values.temps[CH5].temp)
#define set_cellTemp4(m, b, n, s)                set(m,b,n,s, ENV.values.temps[CH4].temp)
#define set_cellTemp3(m, b, n, s)                set(m,b,n,s, ENV.values.temps[CH3].temp)
#define set_cellTemp2(m, b, n, s)                set(m,b,n,s, ENV.values.temps[CH2].temp)
#define set_cellTemp1(m, b, n, s)                set(m,b,n,s, ENV.values.temps[CH1].temp)
#if APP_VARIANT_ID == 0U
#define set_cellTemp20(m, b, n, s)               set(m,b,n,s, ENV.values.temps[CH20].temp)
#define set_cellTemp19(m, b, n, s)               set(m,b,n,s, ENV.values.temps[CH19].temp)
#define set_cellTemp18(m, b, n, s)               set(m,b,n,s, ENV.values.temps[CH18].temp)
#define set_cellTemp17(m, b, n, s)               set(m,b,n,s, ENV.values.temps[CH17].temp)
#define set_cellTemp16(m, b, n, s)               set(m,b,n,s, ENV.values.temps[CH16].temp)
#define set_cellTemp15(m, b, n, s)               set(m,b,n,s, ENV.values.temps[CH15].temp)
#define set_cellTemp14(m, b, n, s)               set(m,b,n,s, ENV.values.temps[CH14].temp)
#define set_cellTemp13(m, b, n, s)               set(m,b,n,s, ENV.values.temps[CH13].temp)
#define set_cellTemp12(m, b, n, s)               set(m,b,n,s, ENV.values.temps[CH12].temp)
#define set_cellTemp11(m, b, n, s)               set(m,b,n,s, ENV.values.temps[CH11].temp)
#define set_cellTemp10(m, b, n, s)               set(m,b,n,s, ENV.values.temps[CH10].temp)
#endif
#define set_cellVoltage5(m, b, n, s)             set(m,b,n,s, BMS.cells[5].voltage)
#define set_cellVoltage4(m, b, n, s)             set(m,b,n,s, BMS.cells[4].voltage)
#define set_cellVoltage3(m, b, n, s)             set(m,b,n,s, BMS.cells[3].voltage)
#define set_cellVoltage2(m, b, n, s)             set(m,b,n,s, BMS.cells[2].voltage)
#define set_cellVoltage1(m, b, n, s)             set(m,b,n,s, BMS.cells[1].voltage)
#define set_cellVoltage0(m, b, n, s)             set(m,b,n,s, BMS.cells[0].voltage)
#define set_cellVoltage11(m, b, n, s)            set(m,b,n,s, BMS.cells[11].voltage)
#define set_cellVoltage10(m, b, n, s)            set(m,b,n,s, BMS.cells[10].voltage)
#define set_cellVoltage9(m, b, n, s)             set(m,b,n,s, BMS.cells[9].voltage)
#define set_cellVoltage8(m, b, n, s)             set(m,b,n,s, BMS.cells[8].voltage)
#define set_cellVoltage7(m, b, n, s)             set(m,b,n,s, BMS.cells[7].voltage)
#define set_cellVoltage6(m, b, n, s)             set(m,b,n,s, BMS.cells[6].voltage)
#define set_segmentVoltageHighRes(m, b, n, s)    set(m,b,n,s, BMS.pack_voltage)
#define set_segmentVoltageHighResCalculated(m, b, n, s)    set(m,b,n,s, BMS.calculated_pack_voltage)
#define set_cellVoltage15(m, b, n, s)            set(m,b,n,s, BMS.cells[15].voltage)
#define set_cellVoltage14(m, b, n, s)            set(m,b,n,s, BMS.cells[14].voltage)
#define set_cellVoltage13(m, b, n, s)            set(m,b,n,s, BMS.cells[13].voltage)
#define set_cellVoltage12(m, b, n, s)            set(m,b,n,s, BMS.cells[12].voltage)
#define set_boardRelativeHumidity(m, b, n, s)    set(m,b,n,s, ENV.values.board.rh)
#define set_boardAmbientTemp(m, b, n, s)         set(m,b,n,s, ENV.values.board.ambient_temp)
#if APP_VARIANT_ID == 1U
#define set_bus7v5Voltage(m, b, n, s)            set(m,b,n,s, drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_VSNS_7V5))
#endif
#define set_tempMcu(m, b, n, s)                  set(m,b,n,s, drv_tempSensors_getChannelTemperatureDegC(DRV_TEMPSENSORS_CHANNEL_MCU))
#define set_tempBalancing0(m, b, n, s)           set(m,b,n,s, drv_tempSensors_getChannelTemperatureDegC(DRV_TEMPSENSORS_CHANNEL_BALANCING1))
#define set_tempBalancing1(m, b, n, s)           set(m,b,n,s, drv_tempSensors_getChannelTemperatureDegC(DRV_TEMPSENSORS_CHANNEL_BALANCING2))
#if APP_VARIANT_ID == 1U
#define set_tempBoard(m, b, n, s)                set(m,b,n,s, drv_tempSensors_getChannelTemperatureDegC(DRV_TEMPSENSORS_CHANNEL_BOARD))
#endif
#define set_dbgThermVoltage1(m,b,n,s)               set(m,b,n,s, drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_MUX1_CH1))
#define set_dbgThermVoltage2(m,b,n,s)               set(m,b,n,s, drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_MUX1_CH2))
#define set_dbgThermVoltage3(m,b,n,s)               set(m,b,n,s, drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_MUX1_CH3))
#define set_dbgThermVoltage4(m,b,n,s)               set(m,b,n,s, drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_MUX1_CH4))
#define set_dbgThermVoltage5(m,b,n,s)               set(m,b,n,s, drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_MUX1_CH5))
#define set_dbgThermVoltage6(m,b,n,s)               set(m,b,n,s, drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_MUX1_CH6))
#define set_dbgThermVoltage7(m,b,n,s)               set(m,b,n,s, drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_MUX1_CH7))
#define set_dbgThermVoltage8(m,b,n,s)               set(m,b,n,s, drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_MUX1_CH8))
#if APP_VARIANT_ID == 0U
#define set_dbgThermVoltage9(m,b,n,s)               set(m,b,n,s, drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_MUX2_CH1))
#elif APP_VARIANT_ID == 1U
#define set_dbgThermVoltage9(m,b,n,s)               set(m,b,n,s, drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_TEMP_THERM9))
#endif
#define set_fan1RPM(m, b, n, s)                  set(m,b,n,s, drv_cooling_getRate(&cooling[COOLING_CHANNEL_FAN2]))
#define set_fan0RPM(m, b, n, s)                  set(m,b,n,s, drv_cooling_getRate(&cooling[COOLING_CHANNEL_FAN1]))
#define set_coolPct1(m, b, n, s)                 set(m,b,n,s, (drv_cooling_getPower(&cooling[COOLING_CHANNEL_FAN2]) * 100.0f))
#define set_coolState1(m, b, n, s)               set(m,b,n,s, (drv_cooling_getState(&cooling[COOLING_CHANNEL_FAN2]) != COOLING_OFF) ? \
                                                               CAN_DIGITALSTATUS_ON : CAN_DIGITALSTATUS_OFF)
#define set_coolPct0(m, b, n, s)                 set(m,b,n,s, (drv_cooling_getPower(&cooling[COOLING_CHANNEL_FAN1]) * 100.0f))
#define set_coolState0(m, b, n, s)               set(m,b,n,s, (drv_cooling_getState(&cooling[COOLING_CHANNEL_FAN1]) != COOLING_OFF) ? \
                                                               CAN_DIGITALSTATUS_ON : CAN_DIGITALSTATUS_OFF)
