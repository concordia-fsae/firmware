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
#define BIT_POS_HPCF_XL        5U
#define BIT_POS_ODR            4U
#define BIT_POS_PAGE_SEL       4U
#define BIT_POS_FS_VL          2U
#define BIT_POS_IF_INC         2U
#define BIT_POS_TDA            2U
#define BIT_POS_I2C_DISABLE    2U
#define BIT_POS_PAGE_READ      5U
#define BIT_POS_PAGE_WRITE     6U
#define BIT_POS_EMB_FUNC_LIR   7U
#define BIT_POS_INT_CLR_ON_READ 6U
#define BIT_POS_GDA            1U
#define BIT_POS_I3C_DISABLE    1U
#define BIT_POS_LPF2_XL_EN     1U
#define BIT_POS_LPF1_SEL_G     1U
#define BIT_POS_XLDA           0U
#define BIT_POS_FIFO_MODE      0U
#define BIT_POS_SW_RESET       0U
#define BIT_POS_GYRO_FTYPE     0U
#define BIT_POS_FSM_EN         0U
#define BIT_POS_FSM1_EN        0U
#define BIT_POS_FSM_INIT       0U

#define PACK_IMU_FTYPE(dev)    (dev->config.accelLpfEnabled ? (uint8_t)(dev->config.accelFtype << BIT_POS_HPCF_XL) : 0U)
#define PACK_GYRO_FTYPE(dev)   (dev->config.gyroLpfEnabled ? (uint8_t)(dev->config.gyroFtype << BIT_POS_GYRO_FTYPE) : 0U)
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
#define PACK_LPF2_XL_EN        (0b1 << BIT_POS_LPF2_XL_EN)
#define PACK_LPF1_SEL_G        (0b1 << BIT_POS_LPF1_SEL_G)

#define PACK_PAGE_READ         (0b1 << BIT_POS_PAGE_READ)
#define PACK_PAGE_WRITE        (0b1 << BIT_POS_PAGE_WRITE)

#define BIT_POS_FSM_ODR        3U

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

static bool getParam(drv_asm330_S* dev, uint8_t reg, uint8_t* data);
static bool setParam(drv_asm330_S* dev, uint8_t reg, uint8_t data);
static bool setFuncCfgAccess(drv_asm330_S* dev, bool enable);
static bool writeAdvancedPageReg(drv_asm330_S* dev, uint8_t page, uint8_t addr, uint8_t value);
static bool writeAdvancedPageBlock(drv_asm330_S* dev, uint16_t startAddr, const uint8_t* data, uint16_t len);
static bool getFsmStatusInfo(uint8_t programNumber, uint8_t* reg, uint8_t* mask);
static bool getFsmEnableInfo(uint8_t programNumber, uint8_t* reg, uint8_t* mask);

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

static bool setFuncCfgAccess(drv_asm330_S* dev, bool enable)
{
    return setParam(dev, ASM330LHB_FUNC_CFG_ACCESS, enable ? PACK_CFG_ACCESS : 0U);
}

static bool writeAdvancedPageReg(drv_asm330_S* dev, uint8_t page, uint8_t addr, uint8_t value)
{
    uint8_t tmp = 0U;
    const uint8_t page_sel_val = (uint8_t)(((page & 0x0FU) << BIT_POS_PAGE_SEL) | 0x01U);

    if (!getParam(dev, ASM330LHB_PAGE_RW, &tmp))
    {
        return false;
    }

    tmp &= (uint8_t)~(PACK_PAGE_READ | PACK_PAGE_WRITE);
    tmp |= PACK_PAGE_WRITE;
    if (!setParam(dev, ASM330LHB_PAGE_RW, tmp))
    {
        return false;
    }

    if (!setParam(dev, ASM330LHB_PAGE_SEL, page_sel_val))
    {
        return false;
    }

    if (!setParam(dev, ASM330LHB_PAGE_ADDRESS, addr))
    {
        return false;
    }

    if (!setParam(dev, ASM330LHB_PAGE_VALUE, value))
    {
        return false;
    }

    if (!setParam(dev, ASM330LHB_PAGE_SEL, 0x01U))
    {
        return false;
    }

    tmp &= (uint8_t)~(PACK_PAGE_READ | PACK_PAGE_WRITE);
    if (!setParam(dev, ASM330LHB_PAGE_RW, tmp))
    {
        return false;
    }

    return true;
}

static bool writeAdvancedPageBlock(drv_asm330_S* dev, uint16_t startAddr, const uint8_t* data, uint16_t len)
{
    for (uint16_t i = 0U; i < len; i++)
    {
        const uint16_t addr = (uint16_t)(startAddr + i);
        const uint8_t page = (uint8_t)((addr >> 8) & 0x0FU);
        const uint8_t offset = (uint8_t)(addr & 0xFFU);

        if (!writeAdvancedPageReg(dev, page, offset, data[i]))
        {
            return false;
        }
    }

    return true;
}

static bool getFsmStatusInfo(uint8_t programNumber, uint8_t* reg, uint8_t* mask)
{
    if ((programNumber == 0U) || (programNumber > 16U) || (reg == NULL) || (mask == NULL))
    {
        return false;
    }

    if (programNumber <= 8U)
    {
        *reg = ASM330LHB_FSM_STATUS_A_MAINPAGE;
        *mask = (uint8_t)(0x01U << (programNumber - 1U));
        return true;
    }

    *reg = ASM330LHB_FSM_STATUS_B_MAINPAGE;
    *mask = (uint8_t)(0x01U << (programNumber - 9U));
    return true;
}

static bool getFsmEnableInfo(uint8_t programNumber, uint8_t* reg, uint8_t* mask)
{
    if ((programNumber == 0U) || (programNumber > 16U) || (reg == NULL) || (mask == NULL))
    {
        return false;
    }

    if (programNumber <= 8U)
    {
        *reg = ASM330LHB_FSM_ENABLE_A;
        *mask = (uint8_t)(0x01U << (programNumber - 1U));
        return true;
    }

    *reg = ASM330LHB_FSM_ENABLE_B;
    *mask = (uint8_t)(0x01U << (programNumber - 9U));
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
            { CONFIG_ENTRY(ASM330LHB_CTRL1_XL, PACK_ODR_FREQ(dev->config.odr) |
                                               PACK_ODR_SCALE(dev->config.scaleA) |
                                               (uint8_t)(dev->config.accelLpfEnabled ? PACK_LPF2_XL_EN : 0U)) },
            { CONFIG_ENTRY(ASM330LHB_CTRL4_C, PACK_I2C_DISABLE |
                                              (uint8_t)(dev->config.gyroLpfEnabled ? PACK_LPF1_SEL_G : 0U)) },
            { CONFIG_ENTRY(ASM330LHB_CTRL8_XL, PACK_IMU_FTYPE(dev)) },
            { CONFIG_ENTRY(ASM330LHB_CTRL6_C, PACK_GYRO_FTYPE(dev)) },
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

        uint8_t freq = genCommandHeader(true, ASM330LHB_INTERNAL_FREQ_FINE);
        getParam(dev, freq, &freq);
        dev->state.sampleTime = 1.0f / (40000.0f + (0.0015f * freq * 40000.0f));
        dev->state.fsmStartAddress = 0U;
        dev->state.fsmMaxProgram = 0U;
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

bool drv_asm330_loadFsmProgram(drv_asm330_S* dev,
                               uint8_t programNumber,
                               uint16_t startAddress,
                               const uint8_t* program,
                               uint16_t programLength,
                               asm330lhb_fsm_odr_t odr)
{
    uint8_t tmp = 0U;
    uint8_t ctrl1_xl = 0U;
    uint8_t ctrl2_g = 0U;
    const uint8_t odr_mask = (uint8_t)(0x0FU << BIT_POS_ODR);
    bool ok = true;
    uint8_t fsm_en_reg = 0U;
    uint8_t fsm_en_mask = 0U;
    uint8_t fsm_programs = 0U;
    uint16_t fsm_start = 0U;

    if ((program == NULL) || (programLength == 0U))
    {
        return false;
    }

    if (!getFsmEnableInfo(programNumber, &fsm_en_reg, &fsm_en_mask))
    {
        return false;
    }

    fsm_programs = dev->state.fsmMaxProgram;
    if (programNumber > fsm_programs)
    {
        fsm_programs = programNumber;
    }

    fsm_start = dev->state.fsmStartAddress;
    if ((fsm_start == 0U) || (startAddress < fsm_start))
    {
        fsm_start = startAddress;
    }

    if (!getParam(dev, ASM330LHB_CTRL1_XL, &ctrl1_xl) ||
        !getParam(dev, ASM330LHB_CTRL2_G, &ctrl2_g))
    {
        return false;
    }

    if (!setParam(dev, ASM330LHB_CTRL1_XL, (uint8_t)(ctrl1_xl & ~odr_mask)) ||
        !setParam(dev, ASM330LHB_CTRL2_G, (uint8_t)(ctrl2_g & ~odr_mask)))
    {
        return false;
    }

    if (!setFuncCfgAccess(dev, true))
    {
        ok = false;
        goto restore_odr;
    }

    if (!writeAdvancedPageBlock(dev, startAddress, program, programLength))
    {
        (void)setFuncCfgAccess(dev, false);
        ok = false;
        goto restore_odr;
    }

    if (!writeAdvancedPageReg(dev, 0x01U, (uint8_t)(ASM330LHB_FSM_PROGRAMS & 0xFFU), fsm_programs))
    {
        (void)setFuncCfgAccess(dev, false);
        ok = false;
        goto restore_odr;
    }

    if (!writeAdvancedPageReg(dev, 0x01U, (uint8_t)(ASM330LHB_FSM_START_ADD_L & 0xFFU), (uint8_t)(fsm_start & 0xFFU)))
    {
        (void)setFuncCfgAccess(dev, false);
        ok = false;
        goto restore_odr;
    }

    if (!writeAdvancedPageReg(dev, 0x01U, (uint8_t)(ASM330LHB_FSM_START_ADD_H & 0xFFU), (uint8_t)((fsm_start >> 8) & 0xFFU)))
    {
        (void)setFuncCfgAccess(dev, false);
        ok = false;
        goto restore_odr;
    }

    if (!getParam(dev, ASM330LHB_EMB_FUNC_ODR_CFG_B, &tmp))
    {
        (void)setFuncCfgAccess(dev, false);
        ok = false;
        goto restore_odr;
    }

    tmp = (uint8_t)((tmp & ~(0x3U << BIT_POS_FSM_ODR)) | ((uint8_t)odr << BIT_POS_FSM_ODR));
    if (!setParam(dev, ASM330LHB_EMB_FUNC_ODR_CFG_B, tmp))
    {
        (void)setFuncCfgAccess(dev, false);
        ok = false;
        goto restore_odr;
    }

    if (!getParam(dev, ASM330LHB_EMB_FUNC_EN_B, &tmp))
    {
        (void)setFuncCfgAccess(dev, false);
        ok = false;
        goto restore_odr;
    }

    tmp |= (0b1 << BIT_POS_FSM_EN);
    if (!setParam(dev, ASM330LHB_EMB_FUNC_EN_B, tmp))
    {
        (void)setFuncCfgAccess(dev, false);
        ok = false;
        goto restore_odr;
    }

    if (!getParam(dev, ASM330LHB_PAGE_RW, &tmp))
    {
        (void)setFuncCfgAccess(dev, false);
        ok = false;
        goto restore_odr;
    }

    tmp |= (0b1 << BIT_POS_EMB_FUNC_LIR);
    if (!setParam(dev, ASM330LHB_PAGE_RW, tmp))
    {
        (void)setFuncCfgAccess(dev, false);
        ok = false;
        goto restore_odr;
    }

    tmp |= (0b1 << BIT_POS_INT_CLR_ON_READ);
    if (!setParam(dev, ASM330LHB_INT_CFG0, tmp))
    {
        (void)setFuncCfgAccess(dev, false);
        ok = false;
        goto restore_odr;
    }

    if (!getParam(dev, fsm_en_reg, &tmp))
    {
        (void)setFuncCfgAccess(dev, false);
        ok = false;
        goto restore_odr;
    }

    tmp |= fsm_en_mask;
    if (!setParam(dev, fsm_en_reg, tmp))
    {
        (void)setFuncCfgAccess(dev, false);
        ok = false;
        goto restore_odr;
    }

    if (getParam(dev, ASM330LHB_EMB_FUNC_INIT_B, &tmp))
    {
        tmp |= (0b1 << BIT_POS_FSM_INIT);
        (void)setParam(dev, ASM330LHB_EMB_FUNC_INIT_B, tmp);
        tmp &= (uint8_t)~(0b1 << BIT_POS_FSM_INIT);
        (void)setParam(dev, ASM330LHB_EMB_FUNC_INIT_B, tmp);
    }

    (void)setFuncCfgAccess(dev, false);
restore_odr:
    (void)setParam(dev, ASM330LHB_CTRL1_XL, ctrl1_xl);
    (void)setParam(dev, ASM330LHB_CTRL2_G, ctrl2_g);
    if (ok)
    {
        dev->state.fsmMaxProgram = fsm_programs;
        dev->state.fsmStartAddress = fsm_start;
    }
    return ok;
}

bool drv_asm330_getFsmEvent(drv_asm330_S* dev, uint8_t programNumber, bool* eventDetected)
{
    uint8_t status = 0U;
    uint8_t reg = 0U;
    uint8_t mask = 0U;

    if (eventDetected == NULL)
    {
        return false;
    }

    if (!getFsmStatusInfo(programNumber, &reg, &mask))
    {
        return false;
    }

    if (!getParam(dev, reg, &status))
    {
        return false;
    }

    *eventDetected = (status & mask) != 0U;
    return true;
}

bool drv_asm330_clearFsmStatus(drv_asm330_S* dev)
{
    uint8_t tmp = 0U;

    if (!getParam(dev, ASM330LHB_FSM_STATUS_A_MAINPAGE, &tmp))
    {
        return false;
    }

    if (!getParam(dev, ASM330LHB_FSM_STATUS_B_MAINPAGE, &tmp))
    {
        return false;
    }

    return true;
}

bool drv_asm330_getFsmStatus(drv_asm330_S* dev, uint8_t* statusA, uint8_t* statusB)
{
    uint8_t tmp = 0U;

    if (statusA)
    {
        if (!getParam(dev, ASM330LHB_FSM_STATUS_A_MAINPAGE, &tmp))
        {
            return false;
        }
        *statusA = tmp;
    }

    if (statusB)
    {
        if (!getParam(dev, ASM330LHB_FSM_STATUS_B_MAINPAGE, &tmp))
        {
            return false;
        }
        *statusB = tmp;
    }

    return true;
}
