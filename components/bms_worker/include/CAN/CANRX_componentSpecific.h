/**
 RX_config* CAN.h
 * Header file for CANRX configuration
 */

#pragma once


/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW_tim.h"
#include "SigTx.c"
// imports for data access
#include "Cooling.h"
#include "Environment.h"
#include "BatteryMonitoring.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define CANIO_UDS_BUFFER_LENGTH 8U
#define CANIO_getTimeMs() (HW_TIM_getTimeMS())

#define set_envFaultFlag(m,b,n,s)                set(m,b,n,s, ENV.state == ENV_FAULT)
#define set_envErrorFlag(m, b, n, s)             set(m,b,n,s, ENV.state == ENV_ERROR)
#define set_faultFlag(m, b, n, s)                set(m,b,n,s, BMS.fault)
#define set_errorFlag(m, b, n, s)                set(m,b,n,s, BMS.state == BMS_ERROR)
#define set_dischargeLimit(m, b, n, s)           set(m,b,n,s, BMS.discharge_limit)
#define set_chargeLimit(m, b, n, s)              set(m,b,n,s, BMS.charge_limit)
#define set_tempMax(m, b, n, s)                  set(m,b,n,s, ENV.values.max_temp)
#define set_segmentVoltage(m, b, n, s)           set(m,b,n,s, BMS.pack_voltage)
#define set_voltageMax(m, b, n, s)               set(m,b,n,s, BMS.voltage.max)
#define set_voltageMin(m, b, n, s)               set(m,b,n,s, BMS.voltage.min)
#define set_cellTemp1(m, b, n, s)                set(m,b,n,s, ENV.values.temps[1].temp)
#define set_cellTemp0(m, b, n, s)                set(m,b,n,s, ENV.values.temps[0].temp)
#define set_socMax(m, b, n, s)                   set(m,b,n,s, BMS.relative_soc.max)
#define set_socAvg(m, b, n, s)                   set(m,b,n,s, BMS.relative_soc.avg)
#define set_socMin(m, b, n, s)                   set(m,b,n,s, BMS.relative_soc.min)
#define set_tempAvg(m, b, n, s)                  set(m,b,n,s, ENV.values.avg_temp)
#define set_cellVoltageAvg(m, b, n, s)           set(m,b,n,s, BMS.voltage.avg)
#define set_cellTemp10(m, b, n, s)               set(m,b,n,s, ENV.values.temps[10].temp)
#define set_cellTemp9(m, b, n, s)                set(m,b,n,s, ENV.values.temps[9].temp)
#define set_cellTemp8(m, b, n, s)                set(m,b,n,s, ENV.values.temps[8].temp)
#define set_cellTemp7(m, b, n, s)                set(m,b,n,s, ENV.values.temps[7].temp)
#define set_cellTemp6(m, b, n, s)                set(m,b,n,s, ENV.values.temps[6].temp)
#define set_cellTemp5(m, b, n, s)                set(m,b,n,s, ENV.values.temps[5].temp)
#define set_cellTemp4(m, b, n, s)                set(m,b,n,s, ENV.values.temps[4].temp)
#define set_cellTemp3(m, b, n, s)                set(m,b,n,s, ENV.values.temps[3].temp)
#define set_cellTemp2(m, b, n, s)                set(m,b,n,s, ENV.values.temps[2].temp)
#define set_cellTemp19(m, b, n, s)               set(m,b,n,s, ENV.values.temps[19].temp)
#define set_cellTemp18(m, b, n, s)               set(m,b,n,s, ENV.values.temps[18].temp)
#define set_cellTemp17(m, b, n, s)               set(m,b,n,s, ENV.values.temps[17].temp)
#define set_cellTemp16(m, b, n, s)               set(m,b,n,s, ENV.values.temps[16].temp)
#define set_cellTemp15(m, b, n, s)               set(m,b,n,s, ENV.values.temps[15].temp)
#define set_cellTemp14(m, b, n, s)               set(m,b,n,s, ENV.values.temps[14].temp)
#define set_cellTemp13(m, b, n, s)               set(m,b,n,s, ENV.values.temps[13].temp)
#define set_cellTemp12(m, b, n, s)               set(m,b,n,s, ENV.values.temps[12].temp)
#define set_cellTemp11(m, b, n, s)               set(m,b,n,s, ENV.values.temps[11].temp)
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
#define set_mcuTemp(m, b, n, s)                  set(m,b,n,s, ENV.values.board.mcu_temp)
#define set_boardTemp0(m, b, n, s)               set(m,b,n,s, ENV.values.board.brd_temp[0])
#define set_boardTemp1(m, b, n, s)               set(m,b,n,s, ENV.values.board.brd_temp[1])
#define set_fan1RPM(m, b, n, s)                  set(m,b,n,s, COOL.rpm[1])
#define set_fan0RPM(m, b, n, s)                  set(m,b,n,s, COOL.rpm[0])
#define set_coolPct1(m, b, n, s)                 set(m,b,n,s, COOL.percentage[1])
#define set_coolState1(m, b, n, s)               set(m,b,n,s, (COOL.state[1] != COOL_OFF) ? CAN_OUTPUTSTATE_ON : CAN_OUTPUTSTATE_OFF)
#define set_coolPct0(m, b, n, s)                 set(m,b,n,s, COOL.percentage[0])
#define set_coolState0(m,b,n,s)                  set(m,b,n,s, (COOL.state[0] != COOL_OFF) ? CAN_OUTPUTSTATE_ON : CAN_OUTPUTSTATE_OFF)

#include "TemporaryStubbing.h"
