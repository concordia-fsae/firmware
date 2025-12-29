/**
 * RX_config* CAN.h
 * Header file for CANRX configuration
 */

#pragma once


/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// imports for timebase
#include "HW_tim.h"

// imports for CAN generated types
#include "CANTypes_generated.h"

// imports for data access
#include "BMS.h"
#include "drv_inputAD.h"
#include "drv_outputAD.h"
#include "ENV.h"
#include "FeatureDefines_generated.h"
#include "HW_adc.h"
#include "IMD.h"
#include "lib_nvm.h"
#include "Module.h"
#include "Sys.h"
#include "BMS_alarm.h"

/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

uint8_t                       CANIO_tx_getNLG513ControlByte(void);
uint8_t                       CANIO_tx_getElconControlByte(void);
CAN_prechargeContactorState_E CANIO_tx_getContactorState(void);

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define CANIO_UDS_BUFFER_LENGTH                     8U
#define CANIO_getTimeMs()                           (HW_TIM_getTimeMS())

#define set_packChargeLimit(m, b, n, s)             set(m, b, n, s, BMS.pack_charge_limit)
#define set_packDischargeLimit(m, b, n, s)          set(m, b, n, s, BMS.pack_discharge_limit)
#define set_packVoltage(m, b, n, s)                 set(m, b, n, s, BMS_VPACK_SOURCE)
#define set_packVoltageCalculated(m, b, n, s)       set(m, b, n, s, BMS.pack_voltage_calculated)
#define set_packVoltageMeasured(m, b, n, s)         set(m, b, n, s, BMS.pack_voltage_measured)
#define set_packVoltageMeasurementFault(m, b, n, s) set(m, b, n, s, BMS.pack_voltage_sense_fault ? CAN_FLAG_SET : CAN_FLAG_CLEARED)
#define set_packCycleCountedCoulombs(m, b, n, s)    set(m, b, n, s, BMS.counted_coulombs.amp_hr)
#define set_packAmpHours(m, b, n, s)                set(m, b, n, s, current_data.pack_amp_hours)
#define set_packCurrent(m, b, n, s)                 set(m, b, n, s, BMS.pack_current)
#define set_packContactorState(m, b, n, s)          set(m, b, n, s, CANIO_tx_getContactorState())
#define set_nlg513ControlByte(m, b, n, s)           set(m, b, n, s, CANIO_tx_getNLG513ControlByte())
#define set_nlg513MaxMainsCurrent(m, b, n, s)       set(m, b, n, s, 16.0f)
#define set_nlg513MaxChargeVoltage(m, b, n, s)      set(m, b, n, s, BMS_CONFIGURED_PACK_MAX_VOLTAGE)
#define set_nlg513MaxChargeCurrent(m, b, n, s)      set(m, b, n, s, (CANIO_tx_getNLG513ControlByte() == 0x40) ? 0.0f : BMS.pack_charge_limit)
#define transmit_BMSB_brusaChargeCommand            (SYS_SFT_checkBrusaChargerTimeout() == false)
#define set_taskUsage1kHz(m, b, n, s)               set(m, b, n, s, Module_getTotalRuntimePercentage(MODULE_1kHz_TASK));
#define set_taskUsage100Hz(m, b, n, s)              set(m, b, n, s, Module_getTotalRuntimePercentage(MODULE_100Hz_TASK));
#define set_taskUsage10Hz(m, b, n, s)               set(m, b, n, s, Module_getTotalRuntimePercentage(MODULE_10Hz_TASK));
#define set_taskUsage1Hz(m, b, n, s)                set(m, b, n, s, Module_getTotalRuntimePercentage(MODULE_1Hz_TASK));
#define set_taskUsageIdle(m, b, n, s)               set(m, b, n, s, Module_getTotalRuntimePercentage(MODULE_IDLE_TASK));
#define set_elconMaxChargeVoltage(m, b, n, s)       set(m, b, n, s, BMS_CONFIGURED_PACK_MAX_VOLTAGE)
#define set_elconMaxChargeCurrent(m, b, n, s)       set(m, b, n, s, BMS.pack_charge_limit)
#define set_elconControlByte(m, b, n, s)            set(m, b, n, s, CANIO_tx_getElconControlByte())
#define transmit_BMSB_elconChargeCommand            (SYS_SFT_checkElconChargerTimeout() == false)
#define transmit_BMSB_currentLimit                  (SYS_SFT_checkMCTimeout() == false)
#define set_maxCharge(m, b, n, s)                   set(m, b, n, s, BMS.pack_charge_limit);
#define set_maxDischarge(m, b, n, s)                set(m, b, n, s, BMS.pack_discharge_limit);
#define set_packRH(m, b, n, s)                      set(m, b, n, s, ENV.board.rh)
#define set_packTemperature(m, b, n, s)             set(m, b, n, s, ENV.board.ambient_temp)
#define set_taskIterations1kHz(m, b, n, s)          set(m, b, n, s, (uint16_t)Module_getTotalRuntimeIterations(MODULE_1kHz_TASK))
#define set_taskIterations100Hz(m, b, n, s)         set(m, b, n, s, (uint16_t)Module_getTotalRuntimeIterations(MODULE_100Hz_TASK))
#define set_taskIterations10Hz(m, b, n, s)          set(m, b, n, s, (uint16_t)Module_getTotalRuntimeIterations(MODULE_10Hz_TASK))
#define set_taskIterations1Hz(m, b, n, s)           set(m, b, n, s, (uint16_t)Module_getTotalRuntimeIterations(MODULE_1Hz_TASK))
#define set_taskStack1kHz(m, b, n, s)               set(m, b, n, s, Module_getMinStackLeft(MODULE_1kHz_TASK))
#define set_taskStack100Hz(m, b, n, s)              set(m, b, n, s, Module_getMinStackLeft(MODULE_100Hz_TASK))
#define set_taskStack10Hz(m, b, n, s)               set(m, b, n, s, Module_getMinStackLeft(MODULE_10Hz_TASK))
#define set_taskStack1Hz(m, b, n, s)                set(m, b, n, s, Module_getMinStackLeft(MODULE_1Hz_TASK))
#define set_tsmsChg(m, b, n, s)                     set(m, b, n, s, (drv_inputAD_getDigitalActiveState(DRV_INPUTAD_DIGITAL_TSMS_CHG) == DRV_IO_ACTIVE) ? \
                                                        CAN_DIGITALSTATUS_ON : CAN_DIGITALSTATUS_OFF)
#define set_okHS(m, b, n, s)                        set(m, b, n, s, (drv_inputAD_getDigitalActiveState(DRV_INPUTAD_DIGITAL_OK_HS) == DRV_IO_ACTIVE) ? \
                                                        CAN_DIGITALSTATUS_ON : CAN_DIGITALSTATUS_OFF)
#define set_bmsIMDReset(m, b, n, s)                 set(m, b, n, s, (drv_inputAD_getDigitalActiveState(DRV_INPUTAD_DIGITAL_BMS_IMD_RESET) == DRV_IO_ACTIVE) ? \
                                                        CAN_DIGITALSTATUS_ON : CAN_DIGITALSTATUS_OFF)
#define set_imdStatusMem(m, b, n, s)                set(m, b, n, s, (drv_inputAD_getDigitalActiveState(DRV_INPUTAD_DIGITAL_IMD_STATUS_MEM) == DRV_IO_ACTIVE) ? \
                                                        CAN_DIGITALSTATUS_ON : CAN_DIGITALSTATUS_OFF)
#define set_bmsStatusMem(m, b, n, s)                set(m, b, n, s, (drv_inputAD_getDigitalActiveState(DRV_INPUTAD_DIGITAL_BMS_STATUS_MEM) == DRV_IO_ACTIVE) ? \
                                                        CAN_DIGITALSTATUS_ON : CAN_DIGITALSTATUS_OFF)
#define set_bmsStatus(m, b, n, s)                   set(m, b, n, s, (drv_outputAD_getDigitalActiveState(DRV_OUTPUTAD_DIGITAL_STATUS_BMS) == DRV_IO_ACTIVE) ? \
                                                        CAN_DIGITALSTATUS_ON : CAN_DIGITALSTATUS_OFF)
#define set_imdStatus(m, b, n, s)                   set(m, b, n, s, (drv_outputAD_getDigitalActiveState(DRV_OUTPUTAD_DIGITAL_STATUS_IMD) == DRV_IO_ACTIVE) ? \
                                                        CAN_DIGITALSTATUS_ON : CAN_DIGITALSTATUS_OFF)
#define set_packCSVoltage(m, b, n, s)               set(m, b, n, s, drv_inputAD_getAnalogVoltage(DRV_INPUTAD_ANALOG_CS))
#define set_packCSPVoltage(m, b, n, s)              set(m, b, n, s, HW_ADC_getVFromBank1Channel(ADC_BANK_CHANNEL_CS))
#define set_packCSNVoltage(m, b, n, s)              set(m, b, n, s, HW_ADC_getVFromBank2Channel(ADC_BANK_CHANNEL_CS))
#define set_packVPackPVoltage(m, b, n, s)           set(m, b, n, s, HW_ADC_getVFromBank1Channel(ADC_BANK_CHANNEL_VPACK))
#define set_packVPackNVoltage(m, b, n, s)           set(m, b, n, s, HW_ADC_getVFromBank2Channel(ADC_BANK_CHANNEL_VPACK))
#define set_mcuTemperature(m, b, n, s)              set(m, b, n, s, ENV.board.mcu_temp)
#define set_MaxPackTemp(m, b, n, s)                 set(m, b, n, s, Return_MaxPackTemp())
#define set_BMSTempAlarm(m, b, n, s)                set(m, b, n, s, Return_AlarmState())

#if FEATURE_IS_ENABLED(NVM_LIB_ENABLED)
# define set_nvmBootCycles(m, b, n, s)              set(m, b, n, s, (uint16_t)lib_nvm_getTotalCycles())
# define set_nvmRecordWrites(m, b, n, s)            set(m, b, n, s, (uint16_t)lib_nvm_getTotalRecordWrites())
# define set_nvmBlockErases(m, b, n, s)             set(m, b, n, s, (uint8_t)lib_nvm_getTotalBlockErases())
# define set_nvmFailedCrc(m, b, n, s)               set(m, b, n, s, (uint8_t)lib_nvm_getTotalFailedCrc())
# define set_nvmRecordFailedInit(m, b, n, s)        set(m, b, n, s, (uint8_t)lib_nvm_getTotalFailedRecordInit())
# define set_nvmRecordEmptyInit(m, b, n, s)         set(m, b, n, s, (uint8_t)lib_nvm_getTotalEmptyRecordInit())
# define set_nvmRecordsVersionFailed(m, b, n, s)    set(m, b, n, s, (uint8_t)lib_nvm_getTotalRecordsVersionFailed())
#else
# define transmit_BMSB_nvmInformation               false
#endif

#include "TemporaryStubbing.h"
