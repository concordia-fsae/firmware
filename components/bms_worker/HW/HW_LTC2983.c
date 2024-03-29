/**
 * @file HW_LTC2983.c
 * @brief  Header file for LTC2983 Thermistor Sensor
 */

#if defined(BMSW_BOARD_VA1)

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW_LTC2983.h"

#include "include/HW_spi.h"
#include "SystemConfig.h"
#include <stdint.h>


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define READ_INST_BYTE               0x03
#define WRITE_INST_BYTE              0x02

#define COMMAND_REG                  0x00
#define TEMP_RESULT_BASE_REG         0x10
#define GLAB_CONFIG_REG              0xf0
#define MULTIPLE_CHANNEL_MASK_REG    0xf4
#define MUX_DELAY_CONFIG_REG         0xff
#define CHANNEL_ASSIGN_REG           0x0200
#define CUSTOM_SENSOR_TABLE          0x250

#define DIRECT_ADC_MEAS              (0x1e << 27 | 0x01 << 26)


/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

HW_SPI_Device_S SPI_LTC2983 = {
    .handle  = SPI1,
    .ncs_pin = {
        .pin  = SPI1_LTC_NCS_Pin,
        .port = SPI1_LTC_NCS_Port,
    }
};

HW_GPIO_S       LTC2983_INT = {
    .pin  = LTC_INTERRUPT_Pin,
    .port = LTC_INTERRUPT_Port,
};

HW_GPIO_S       LTC2983_NRST = {
    .pin  = LTC_NRST_Pin,
    .port = LTC_NRST_Port,
};

LTC2983_S       ltc_chip = { 0 };


/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

bool LTC_writeMeasurementType(LTC_S* chip);
bool LTC_writeMultConvFlags(LTC_S* chip);
bool LTC_getResults(LTC_S* chip);
bool LTC_sendCMD(LTC_S* chip, uint16_t addr, uint8_t size, uint8_t* val);
bool LTC_readCMD(LTC_S* chip, uint16_t addr, uint8_t size, uint8_t* val);

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

bool LTC_init()
{
    ltc_chip.dev       = &LTC2983;
    ltc_chip.interrupt = &LTC2983_INT;
    ltc_chip.nrst      = &LTC2983_NRST;

    HW_GPIO_WritePin(ltc_chip.nrst, true);

    // uint8_t response = 0;

    // do {
    // LTC_ReadCMD(&ltc_chip, COMMAND_REG, 1, &response);
    // } while (response != 0x40);

    for (uint8_t i = 0; i < CHANNEL_COUNT; i++)
    {
        ltc_chip.config.msmnt_type[i]              = DIRECTADC_SGL;
        ltc_chip.config.multiple_conversion_flags |= 0x01 << i;
    }

    LTC_WriteMeasurementType(&ltc_chip);
    LTC_WriteMultConvFlags(&ltc_chip);

    return true;
}

bool LTC_startMeasurement(void)
{
    uint8_t data = 0x80;

    return LTC_SendCMD(&ltc_chip, COMMAND_REG, 1, &data);
}

bool LTC_getMeasurement(void)
{
    // uint8_t response = 0x00;
    // if (!LTC_ReadCMD(&ltc_chip, COMMAND_REG, 1, &response))
    // return false;

    // if (response != 0x40)
    // return false;

    for (uint8_t i = 0; i < CHANNEL_COUNT; i++)
    {
        if (!LTC_ReadCMD(&ltc_chip, TEMP_RESULT_BASE_REG + 4 * i, 4,
                         (uint8_t*)&ltc_chip.raw_results[i].raw)
            )
        {
            return false;
        }

        ltc_chip.raw_results[i].hard_fault = (ltc_chip.raw_results[i].raw & 0x01 << 31) ? true : false;
        ltc_chip.raw_results[i].soft_above = (ltc_chip.raw_results[i].raw & 0x01 << 27) ? true : false;
        ltc_chip.raw_results[i].soft_below = (ltc_chip.raw_results[i].raw & 0x01 << 26) ? true : false;
        ltc_chip.raw_results[i].valid      = (ltc_chip.raw_results[i].raw & 0x01 << 24) ? true : false;
        ltc_chip.raw_results[i].result     = (ltc_chip.raw_results[i].raw & 0xffffff) << 8;
    }

    return true;
}

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

bool LTC_writeMeasurementType(LTC2983_S* chip)
{
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++)
    {
        uint32_t chn_val = 0;

        switch (chip->config.msmnt_type[i])
        {
            case DIRECTADC_SGL:
                chn_val = DIRECT_ADC_MEAS;
                break;

            default:
                return false;
        }

        if (!LTC_SendCMD(chip, CHANNEL_ASSIGN_REG + 4 * i, 4, (uint8_t*)&chn_val))
        {
            return false;
        }
    }

    return true;
}

bool LTC_writeMultConvFlags(LTC2983_S* chip)
{
    if (!LTC_SendCMD(chip, MULTIPLE_CHANNEL_MASK_REG, 4,
                     (uint8_t*)&chip->config.multiple_conversion_flags)
        )
    {
        return false;
    }

    return true;
}

bool LTC_sendCMD(LTC2983_S* chip, uint16_t addr, uint8_t size, uint8_t* val)
{
    if (!HW_SPI_Lock(chip->dev))
    {
        return false;
    }

    HW_SPI_Transmit8(chip->dev, WRITE_INST_BYTE);
    HW_SPI_Transmit16(chip->dev, addr);

    for (uint8_t i = 0; i < size; i++)
    {
        HW_SPI_Transmit8(chip->dev, val[size - i - 1]);
    }

    if (!HW_SPI_Release(chip->dev))
    {
        return false;
    }

    return true;
}

bool LTC_readCMD(LTC2983_S* chip, uint16_t addr, uint8_t size, uint8_t* val)
{
    if (!HW_SPI_Lock(chip->dev))
    {
        return false;
    }

    HW_SPI_Transmit8(chip->dev, READ_INST_BYTE);
    HW_SPI_Transmit16(chip->dev, addr);

    for (uint8_t i = 0; i < size; i++)
    {
        HW_SPI_TransmitReceive8(chip->dev, 0xff, &val[size - i - 1]);
    }

    if (!HW_SPI_Release(chip->dev))
    {
        return false;
    }

    return true;
}

#endif /**< BMSW_BOARD_VA1 */
