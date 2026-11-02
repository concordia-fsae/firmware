#pragma once
typedef enum { DRV_OUTPUTAD_DIGITAL_MCU_UART_EN, } drv_outputAD_channelDigital_E;
typedef enum { DRV_IO_INACTIVE, DRV_IO_ACTIVE, }   drv_io_activeState_E;
void drv_outputAD_setDigitalActiveState(drv_outputAD_channelDigital_E channel, drv_io_activeState_E state);
