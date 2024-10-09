/**
 * @file HW_MAX14921.c
 * @brief  Source code for the MAX14921 Cell Measurement/Balancing IC
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// System Includes
#include "stdint.h"
#include "string.h"
#include "SystemConfig.h"

// Firmware Includes
#include "HW.h"
#include "HW_MAX14921.h"
#include "HW_spi.h"

// Other Includes
#include "Utility.h"


/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

MAX_S max_chip;


/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

HW_SPI_Device_S SPI_MAX = {
    .handle  = SPI1,
    .ncs_pin = {
        .pin  = SPI1_MAX_NCS_Pin,
        .port = SPI1_MAX_NCS_Port,
    }
};


/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void MAX_translateConfig(MAX_config_S* config, uint8_t* data);
void MAX_decodeResponse(MAX_response_S* chip, uint8_t* data);

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Initializes MAX 14920/14921
 *
 * @retval true = Success, false = Failure
 */
bool MAX_init(void)
{
    memset(&max_chip, 0x00, sizeof(max_chip));

    GPIO_InitTypeDef GPIO_InitStruct = { 0 };

    GPIO_InitStruct.Pin   = MAX_SAMPLE_Pin;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(MAX_SAMPLE_GPIO_Port, &GPIO_InitStruct);

    HAL_GPIO_WritePin(MAX_SAMPLE_GPIO_Port, MAX_SAMPLE_Pin, GPIO_PIN_RESET);

    max_chip.dev                         = &SPI_MAX;
    max_chip.config.low_power_mode       = false;
    max_chip.config.diagnostic_enabled   = false;
    max_chip.config.sampling             = false;
    max_chip.config.sampling_start       = UINT32_MAX;
    max_chip.config.balancing            = 0x00;
    max_chip.config.output.state         = MAX_AMPLIFIER_SELF_CALIBRATION;
    max_chip.config.output.output.cell   = MAX_CELL1;

    MAX_readWriteToChip();

    if (max_chip.state.ic_id == MAX_PN_ERROR)
    {
        return false;
    }

    return true;
}

/**
 * @brief  Transfer commands and read data from MAX1492*
 *
 * @retval true = Success, false = Failure
 */
bool MAX_readWriteToChip(void)
{
    if (!HW_SPI_lock(max_chip.dev))
    {
        return false;
    }

    uint8_t wdata[3] = { 0x00 };
    uint8_t rdata[3] = { 0x00 };

    MAX_translateConfig(&max_chip.config, (uint8_t*)&wdata);

    for (uint8_t i = 0; i < 3; i++)
    {
        /**< Bits must be reversed because SPI bus transmits MSB first whereas MAX takes LSB first */
        wdata[i] = reverse_byte(wdata[i]);
        HW_SPI_transmitReceive8(max_chip.dev, wdata[i], &rdata[i]);
        rdata[i] = reverse_byte(rdata[i]);
    }

    MAX_decodeResponse(&max_chip.state, (uint8_t*)&rdata);

    HW_SPI_release(max_chip.dev);

    return true;
}


/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Translate MAX1492* configuration to message frame
 *
 * @param config Configuration to translate
 * @param data Data to send to device
 */
void MAX_translateConfig(MAX_config_S* config, uint8_t* data)
{
    data[0] = (uint8_t)config->balancing;
    data[1] = (uint8_t)(config->balancing >> 8);

    if (config->output.state == MAX_CELL_VOLTAGE)
    {
        data[2]  = 0x01;
        data[2] |= (uint8_t)config->output.output.cell << 1;
    }
    else
    {
        data[2] = 0x00;

        switch (config->output.state)
        {
            case (MAX_PARASITIC_ERROR_CALIBRATION):
                data[2] |= 0b0000 << 1;
                break;

            case (MAX_AMPLIFIER_SELF_CALIBRATION):
                data[2] |= 0b0001 << 1;
                break;

            case (MAX_TEMPERATURE_UNBUFFERED):
                data[2] |= 0b1000 << 1;
                break;

            case (MAX_PACK_VOLTAGE):
                data[2] |= 0b1100 << 1;
                break;

            case (MAX_TEMPERATURE_BUFFERED):
                data[2] |= 0b1100 << 1;
                break;

            default:
                /**< Should never reach here */
                break;
        }

        if ((config->output.state == MAX_TEMPERATURE_BUFFERED) ||
            (config->output.state == MAX_TEMPERATURE_UNBUFFERED)
            )
        {
            data[2] |= config->output.output.temp << 1;
        }
    }

    /**< Device always SAMPL IO controlled */    // data[2] |= (config->sampling) ? 0 : 1 << 5;
    HAL_GPIO_WritePin(MAX_SAMPLE_GPIO_Port, MAX_SAMPLE_Pin, (config->sampling) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    if (config->sampling)
    {
        data[2] |= (config->diagnostic_enabled) ? 1 << 6 : 0;
    }
    data[2] |= (config->low_power_mode) ? 1 << 7 : 0;
}

/**
 * @brief  Decode response from MAX1492*
 *
 * @param chip Chip peripheral storing the response
 * @param data Data received from the MAX1492*
 */
void MAX_decodeResponse(MAX_response_S* chip, uint8_t* data)
{
    chip->cell_undervoltage = (uint16_t)data[1] << 8 | data[0];
    if ((data[2] & 0x03) == 0x3)
    {
        chip->ic_id = MAX_PN_ERROR;
    }
    else
    {
        chip->ic_id = ((data[2] & 0x03) == 0x01) ? MAX_PN_14920 : MAX_PN_14921;
    }
    chip->die_version      = (data[2] & 0x0c) >> 2;
    chip->va_undervoltage  = (data[2] & 0x10) ? true : false;
    chip->vp_undervoltage  = (data[2] & 0x20) ? true : false;
    chip->ready            = (data[2] & 0x40) ? false : true;
    chip->thermal_shutdown = (data[2] & 0x80) ? true : false;
}
