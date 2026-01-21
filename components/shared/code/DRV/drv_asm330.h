/**
 * @file drv_asm330.h
 * @brief Header file for the ST ASM330 line of IMU devices
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW_spi.h"
#include "drv_imu.h"
#include "asm330lhb_reg.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define drv_asm330_getFifoOverrun(dev)       ((drv_asm330_getFifoStatus(dev) & MASK_FIFO_OVR) != 0U)
#define drv_asm330_getFifoElementsReady(dev) (drv_asm330_getFifoStatus(dev) & MASK_DIFF_FIFO)

#define MASK_DIFF_FIFO  0x3ff
#define MASK_FIFO_OVR   (1U << 14U)

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef enum
{
    DRV_ASM330_STATE_INIT = 0x00U,
    DRV_ASM330_STATE_RUNNING,
    DRV_ASM330_STATE_UNSUPPORTED,
} drv_asm330_state_E;

typedef struct
{
    int16_t x;
    int16_t y;
    int16_t z;
} drv_asm330_vector_S;

typedef uint32_t drv_asm330_timestamp_t;

typedef union
{
    drv_asm330_timestamp_t timestamp;
    drv_asm330_vector_S vector;
} drv_asm330_result_U;

typedef struct
{
    uint8_t tag;
    uint8_t elem[6U];
} __attribute__((packed)) drv_asm330_fifoElement_S;

typedef struct
{
    asm330lhb_fifo_data_out_tag_t tag;
    drv_asm330_result_U elem;
} drv_asm330_fifoElementUnpack_S;

typedef struct
{
    HW_spi_device_E dev;
    struct {
        asm330lhb_odr_xl_t odr;
        asm330lhb_fs_xl_t  scaleA;
    } config;
    struct {
        float32_t scaleA;
        drv_asm330_state_E state;
    } state;
} drv_asm330_S;

/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

bool drv_asm330_init(drv_asm330_S* dev);
bool drv_asm330_getInertialMeasurement(drv_asm330_S* dev, drv_imu_accel_S* accel);
bool drv_asm330_getGyroMeasurement(drv_asm330_S* dev, drv_imu_gyro_S* gyro);
void drv_asm330_getAccelFromVec(drv_asm330_S* dev, drv_asm330_vector_S* vec, drv_imu_accel_S* accel);
void drv_asm330_getGyroFromVec(drv_asm330_S* dev, drv_asm330_vector_S* vec, drv_imu_gyro_S* gyro);
drv_asm330_state_E drv_asm330_getState(drv_asm330_S* dev);

uint16_t drv_asm330_getFifoStatus(drv_asm330_S* dev);
uint16_t drv_asm330_getFifoElements(drv_asm330_S* dev, uint8_t* data, uint16_t maxLen);
bool drv_asm330_getFifoElementsDMA(drv_asm330_S* dev, uint8_t* data, uint16_t maxLen);
asm330lhb_fifo_tag_t drv_asm330_unpackElement(drv_asm330_S* dev, drv_asm330_fifoElement_S* pack, drv_imu_vector_S* vec);
