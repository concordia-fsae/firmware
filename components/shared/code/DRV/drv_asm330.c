/**
 * @file drv_asm330.c
 * @brief Source file for the ST ASM330 line of IMU devices
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "drv_asm330.h"
#include "Utility.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define DEFAULT_SCALE_G        ((500.0f) / 65535.0f)

#define DEFAULT_SCALE_A        ((4.0f * GRAVITY) / 65535.0f)
#define SCALE_A_16G            ((32.0f * GRAVITY) / 65535.0f)
#define SCALE_A_8G             ((16.0f * GRAVITY) / 65535.0f)
#define SCALE_A_4G             ((8.0f * GRAVITY) / 65535.0f)

#define CONFIG_ENTRY(param, val) genCommandHeader(false, param), (uint8_t)val

#define BIT_POS_SOFT_RESET     7U
#define BIT_POS_CFG_ACCESS     7U
#define BIT_POS_READ           7U
#define BIT_POS_HP_EN_G        6U
#define BIT_POS_BDU            6U
#define BIT_POS_TIMESTAMP_EN   5U
#define BIT_POS_ODR            4U
#define BIT_POS_FS_VL          2U
#define BIT_POS_IF_INC         2U
#define BIT_POS_TDA            2U
#define BIT_POS_I2C_DISABLE    2U
#define BIT_POS_GDA            1U
#define BIT_POS_I3C_DISABLE    1U
#define BIT_POS_XLDA           0U
#define BIT_POS_FIFO_MODE      0U
#define BIT_POS_SW_RESET       0U

#define PACK_ODR_FREQ(freq)    (freq << BIT_POS_ODR)
#define PACK_ODR_SCALE(scale)  (scale << BIT_POS_FS_VL)
#define PACK_FIFO_FREQ(freq)   ((freq) | (freq << BIT_POS_ODR))
#define PACK_IF_INC            (0b1 << BIT_POS_IF_INC)
#define PACK_HP_EN_G           (0b1 << BIT_POS_HP_EN_G)
#define PACK_TDA               (0b1 << BIT_POS_TDA)
#define PACK_GDA               (0b1 << BIT_POS_GDA)
#define PACK_XLDA              (0b1 << BIT_POS_XLDA)
#define PACK_FIFO_CONTINUOUS   (0b110 << BIT_POS_FIFO_MODE)
#define PACK_I2C_DISABLE       (0b1 << BIT_POS_I2C_DISABLE)
#define PACK_I3C_DISABLE       (0b1 << BIT_POS_I3C_DISABLE)
#define PACK_TIMESTAMP_EN      (0b1 << BIT_POS_TIMESTAMP_EN)
#define PACK_SOFT_RESET        (0b1 << BIT_POS_SOFT_RESET)
#define PACK_SW_RESET          (0b1 << BIT_POS_SW_RESET)
#define PACK_CFG_ACCESS        (0b1 << BIT_POS_CFG_ACCESS)
#define PACK_BDU               (0b1 << BIT_POS_BDU)

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

static bool commHelper(drv_asm330_S* dev, uint8_t* wData, uint8_t wLen, uint8_t* rData, uint16_t rLen)
{
    if (!HW_SPI_lock(dev->dev))
    {
        return false;
    }

    HW_SPI_transmitReceiveAsym(dev->dev, wData, wLen, rData, rLen);
    HW_SPI_release(dev->dev);

    return true;
}

static inline uint8_t genCommandHeader(bool read, uint8_t command)
{
    return command | (read ? 1U << BIT_POS_READ : 0U);
}

static bool getVector(drv_asm330_S* dev, uint8_t command, drv_asm330_vector_S* vec)
{
    return commHelper(dev, &command, 1U, (uint8_t*)vec, sizeof(*vec));
}

static bool getParam(drv_asm330_S* dev, uint8_t reg, uint8_t* data)
{
    uint8_t command = genCommandHeader(true, reg);
    return commHelper(dev, &command, 1U, data, 1U);
}

static bool getFlag(drv_asm330_S* dev, uint8_t reg, uint8_t flag)
{
    uint8_t tmp = 0U;
    if (!getParam(dev, reg, &tmp))
    {
        return false;
    }

    return (tmp & flag) != 0;
}

static bool setParam(drv_asm330_S* dev, uint8_t reg, uint8_t data)
{
    uint8_t tmp[2U] = { genCommandHeader(false, reg), data };

    if (!HW_SPI_lock(dev->dev))
    {
        return false;
    }
    HW_SPI_transmit(dev->dev, (uint8_t*)&tmp, sizeof(tmp));
    HW_SPI_release(dev->dev);

    return true;
}

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

drv_asm330_state_E drv_asm330_getState(drv_asm330_S* dev)
{
    return dev->state.state;
}

bool drv_asm330_init(drv_asm330_S* dev)
{
    uint8_t id = 0;
    const bool valid = getParam(dev, ASM330LHB_WHO_AM_I, &id);

    if (valid && (id == ASM330LHB_ID))
    {
        setParam(dev, CONFIG_ENTRY(ASM330LHB_CTRL3_C, PACK_SW_RESET | PACK_IF_INC));
        while (getFlag(dev, ASM330LHB_CTRL3_C, PACK_SW_RESET));

        // General configs to always apply
        uint8_t configs[][2U] = {
            { CONFIG_ENTRY(ASM330LHB_FIFO_CTRL3, PACK_FIFO_FREQ(dev->config.odr)) },
            { CONFIG_ENTRY(ASM330LHB_CTRL1_XL, PACK_ODR_FREQ(dev->config.odr) | PACK_ODR_SCALE(dev->config.scaleA)) },
            { CONFIG_ENTRY(ASM330LHB_CTRL2_G, PACK_ODR_FREQ(dev->config.odr)) },
            { CONFIG_ENTRY(ASM330LHB_FIFO_CTRL4, PACK_FIFO_CONTINUOUS) },
            { CONFIG_ENTRY(ASM330LHB_CTRL3_C, PACK_IF_INC | PACK_BDU) },
        };

        for (uint8_t i = 0U; i < COUNTOF(configs); i++)
        {
            setParam(dev, configs[i][0U], configs[i][1U]);
        }

        switch (dev->config.scaleA)
        {
            case ASM330LHB_4g:
                dev->state.scaleA = SCALE_A_4G;
                break;
            case ASM330LHB_8g:
                dev->state.scaleA = SCALE_A_8G;
                break;
            case ASM330LHB_16g:
                dev->state.scaleA = SCALE_A_16G;
                break;
            default:
                dev->state.scaleA = DEFAULT_SCALE_A;
                break;
        }
    }
    else
    {
        dev->state.state = DRV_ASM330_STATE_UNSUPPORTED;
        return false;
    }

    dev->state.state = DRV_ASM330_STATE_RUNNING;
    return true;
}

bool drv_asm330_getInertialMeasurement(drv_asm330_S* dev, drv_imu_accel_S* accel)
{
    drv_asm330_vector_S vec = { 0U };
    uint8_t header = genCommandHeader(true, ASM330LHB_OUTX_L_A);

    if (!getVector(dev, header, &vec))
    {
        return false;
    }

    drv_asm330_getAccelFromVec(dev, &vec, accel);

    return true;
}

bool drv_asm330_getGyroMeasurement(drv_asm330_S* dev, drv_imu_gyro_S* gyro)
{
    drv_asm330_vector_S vec = { 0U };
    uint8_t header = genCommandHeader(true, ASM330LHB_OUTX_L_G);

    if (!getVector(dev, header, &vec))
    {
        return false;
    }

    drv_asm330_getGyroFromVec(dev, &vec, gyro);

    return true;
}

uint16_t drv_asm330_getFifoStatus(drv_asm330_S* dev)
{
    uint8_t command = genCommandHeader(true, ASM330LHB_FIFO_STATUS1);
    uint16_t response = { 0U };
    commHelper(dev, &command, 1U, (uint8_t*)&response, 2U);

    return response;
}

bool drv_asm330_getFifoElementsDMA(drv_asm330_S* dev, uint8_t* data, uint16_t maxLen)
{
    uint16_t count = drv_asm330_getFifoElementsReady(dev);
    count = count * sizeof(drv_asm330_fifoElement_S) < maxLen ? count : maxLen / sizeof(drv_asm330_fifoElement_S);
    uint8_t command = genCommandHeader(true, ASM330LHB_FIFO_DATA_OUT_TAG);

    return HW_SPI_dmaTransmitReceiveAsym(dev->dev, &command, sizeof(command), data, count * sizeof(drv_asm330_fifoElement_S));
}

uint16_t drv_asm330_getFifoElements(drv_asm330_S* dev, uint8_t* data, uint16_t maxLen)
{
    uint16_t count = drv_asm330_getFifoElementsReady(dev);
    count = count * sizeof(drv_asm330_fifoElement_S) < maxLen ? count : maxLen / sizeof(drv_asm330_fifoElement_S);

    if (count)
    {
        uint8_t command = genCommandHeader(true, ASM330LHB_FIFO_DATA_OUT_TAG);
        commHelper(dev, &command, 1U, data, count * sizeof(drv_asm330_fifoElement_S));
    }

    return count;
}

void drv_asm330_getAccelFromVec(drv_asm330_S* dev, drv_asm330_vector_S* vec, drv_imu_accel_S* accel)
{
    accel->accelX = vec->x * dev->state.scaleA;
    accel->accelY = vec->y * dev->state.scaleA;
    accel->accelZ = vec->z * dev->state.scaleA;
}

void drv_asm330_getGyroFromVec(drv_asm330_S* dev, drv_asm330_vector_S* vec, drv_imu_gyro_S* gyro)
{
    (void)dev;
    gyro->rotX = vec->x * DEFAULT_SCALE_G;
    gyro->rotY = vec->y * DEFAULT_SCALE_G;
    gyro->rotZ = vec->z * DEFAULT_SCALE_G;
}

asm330lhb_fifo_tag_t drv_asm330_unpackElement(drv_asm330_S* dev, drv_asm330_fifoElement_S* pack, drv_imu_vector_S* vec)
{
    drv_asm330_fifoElementUnpack_S unpack = { 0U };
    memcpy((uint8_t*)&unpack.tag, &pack->tag, sizeof(pack->elem));
    memcpy((uint8_t*)&unpack.elem, pack->elem, sizeof(pack->elem));

    switch (unpack.tag.tag_sensor)
    {
        case ASM330LHB_GYRO_NC_TAG:
            drv_asm330_getGyroFromVec(dev, &unpack.elem.vector, (drv_imu_gyro_S*)vec);
            break;
        case ASM330LHB_XL_NC_TAG:
            drv_asm330_getAccelFromVec(dev, &unpack.elem.vector, (drv_imu_accel_S*)vec);
            break;
        case ASM330LHB_TEMPERATURE_TAG:
            break;
        case ASM330LHB_TIMESTAMP_TAG:
            break;
        case ASM330LHB_CFG_CHANGE_TAG:
            break;
    }

    return unpack.tag.tag_sensor;
}
