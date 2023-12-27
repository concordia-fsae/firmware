/**
 * @file HW_MAX14921.c
 * @brief  Source code for the MAX14921 Cell Measurement/Balancing IC
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @date 2023-12-23
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW_MAX14921.h"

#include "HW_spi.h"
#include "SystemConfig.h"

#include "Utility.h"
#include "string.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

/******************************************************************************
 *                               M A C R O S
 ******************************************************************************/

/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

HW_SPI_Device_S MAX14921 = {
    .handle  = SPI1,
    .ncs_pin = {
        .pin  = SPI1_MAX_NCS_Pin,
        .port = SPI1_MAX_NCS_Port,
    }
};

static MAX14921_S max_chip;

/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void MAX_TranslateConfig(MAX14921_Config_S*, uint8_t*);
void MAX_DecodeResponse(MAX14921_Response_S*, uint8_t*);

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

bool MAX_Init()
{
    memset(&max_chip, 0x00, sizeof(max_chip));

    max_chip.dev                         = &MAX14921;
    max_chip.config.low_power_mode       = false;
    max_chip.config.diagnostic_enabled   = false;
    max_chip.config.sampling             = false;
    max_chip.config.sampling_start_100us = 0x00;
    max_chip.config.balancing            = 0x00;
    max_chip.config.output.state         = AMPLIFIER_SELF_CALIBRATION;
    max_chip.config.output.output.cell   = CELL1;

    MAX_ReadWriteToChip(&max_chip);

    if (max_chip.state.ic_id == PN_ERROR)
        return false;

    return true;
}

bool MAX_ReadWriteToChip(MAX14921_S* chip)
{
    if (!HW_SPI_Lock(chip->dev))
        return false;

    uint8_t wdata[3] = { 0x00 };
    uint8_t rdata[3] = { 0x00 };

    MAX_TranslateConfig(&chip->config, (uint8_t*)&wdata);

    for (uint8_t i = 0; i < 3; i++)
    {
        /**< Bits must be reversed because SPI bus transmits MSB first whereas MAX takes LSB first */
        wdata[i] = reverse_byte(wdata[i]);
        HW_SPI_TransmitReceive8(chip->dev, wdata[i], &rdata[i]);
        rdata[i] = reverse_byte(rdata[i]);
    }

    MAX_DecodeResponse(&chip->state, (uint8_t*)&rdata);

    HW_SPI_Release(chip->dev);

    return true;
}


/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

void MAX_TranslateConfig(MAX14921_Config_S* config, uint8_t* data)
{
    data[0] = (uint8_t)config->balancing;
    data[1] = (uint8_t)(config->balancing >> 8);

    if (config->output.state == CELL_VOLTAGE)
    {
        data[2] = 0x01;
        data[2] = (uint8_t)config->output.output.cell << 1;
    }
    else
    {
        data[2] = 0x00;

        switch (config->output.state)
        {
            case (PARASITIC_ERROR_CALIBRATION):
                data[2] |= 0b0000 << 1;
                break;
            case (AMPLIFIER_SELF_CALIBRATION):
                data[2] |= 0b0001 << 1;
                break;
            case (TEMPERATURE_UNBUFFERED):
                data[2] |= 0b1000 << 1;
                break;
            case (PACK_VOLTAGE):
                data[2] |= 0b1100 << 1;
                break;
            case (TEMPERATURE_BUFFERED):
                data[2] |= 0b1100 << 1;
                break;
            default:
                /**< Should never reach here */
                break;
        }

        if (config->output.state == TEMPERATURE_BUFFERED ||
            config->output.state == TEMPERATURE_UNBUFFERED)
        {
            data[2] |= config->output.output.temp << 1;
        }
    }

    data[2] |= (config->sampling) ? 0 : 1 << 5;
    data[2] |= (config->diagnostic_enabled) ? 1 << 6 : 0;
    data[2] |= (config->low_power_mode) ? 1 << 7 : 0;
}

void MAX_DecodeResponse(MAX14921_Response_S* chip, uint8_t* data)
{
    chip->cell_undervoltage = data[1] << 8 | data[0];
    if ((data[2] & 0x03) == 0x3) 
    {
        chip->ic_id = PN_ERROR;
    }
    else {
        chip->ic_id = ((data[2] & 0x03) == 0x01) ? PN_14920 : PN_14921;
    }
    chip->die_version = (data[2] & 0x0c) >> 2;
    chip->va_undervoltage = (data[2] & 0x10) ? true : false;
    chip->vp_undervoltage = (data[2] & 0x20) ? true : false;
    chip->ready = (data[2] & 0x40) ? false : true;
    chip->thermal_shutdown = (data[2] & 0x80) ? true : false;
}