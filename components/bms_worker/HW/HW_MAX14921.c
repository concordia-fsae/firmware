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
    .handle = SPI1,
    .ncs_pin = {
        .pin = SPI1_MAX_NCS_Pin,
        .port = SPI1_MAX_NCS_Port,
    }
}; 

static MAX14921_S max_chip; 

/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void MAX_TranslateConfig(MAX14921_S*, uint8_t*);


  /******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

bool MAX_Init()
{
    memset(&max_chip, 0x00, sizeof(max_chip));

    max_chip.dev = &MAX14921;
    max_chip.config.low_power_mode = false;
    max_chip.config.diagnostic_enabled = false;
    max_chip.config.sampling = false;
    max_chip.config.sampling_start_100us = 0x00;
    max_chip.config.balancing = 0x00;
    max_chip.config.output.state = AMPLIFIER_SELF_CALIBRATION;
    max_chip.config.output.output.cell = CELL1;

    uint8_t wdata[3] = {0x00};
    uint8_t rdata[3] = {0x00};

    MAX_TranslateConfig(&max_chip, (uint8_t*) &wdata);

    HW_SPI_Lock(max_chip.dev);

    for (uint8_t i = 0; i < 3; i++)
    {
        HW_SPI_TransmitReceive8(max_chip.dev, wdata[i], &rdata[i]);
    }

    HW_SPI_Release(max_chip.dev);
    return true;
}

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

void MAX_TranslateConfig(MAX14921_S *config, uint8_t *data)
{
    data[0] = (uint8_t) config->config.balancing;
    data[1] = (uint8_t) (config->config.balancing >> 8);

    if (config->config.output.state == CELL_VOLTAGE)
    {
        data[2] = 0x01;
        switch(config->config.output.output.cell)
        {
            case CELL1:
              data[2] |= CELL1 << 1;
              break;
            case CELL2:
              data[2] |= CELL2 << 1;
              break;
            case CELL3:
              data[2] |= CELL3 << 1;
              break;
            case CELL4:
              data[2] |= CELL4 << 1;
              break;
            case CELL5:
              data[2] |= CELL5 << 1;
              break;
            case CELL6:
              data[2] |= CELL6 << 1;
              break;
            case CELL7:
              data[2] |= CELL7 << 1;
              break;
            case CELL8:
              data[2] |= CELL8 << 1;
              break;
            case CELL9:
              data[2] |= CELL9 << 1;
              break;
            case CELL10:
              data[2] |= CELL10 << 1;
              break;
            case CELL11:
              data[2] |= CELL11 << 1;
              break;
            case CELL12:
              data[2] |= CELL12 << 1;
              break;
            case CELL13:
              data[2] |= CELL13 << 1;
              break;
            case CELL14:
              data[2] |= CELL14 << 1;
              break;
            case CELL15:
              data[2] |= CELL15 << 1;
              break;
            case CELL16:
              data[2] |= CELL16 << 1;
              break;
            default:
              /**< Should never reach here */
              break;
        }
    }
    else {
        data[2] = 0x00;

        switch(config->config.output.state)
        {
          case(PARASITIC_ERROR_CALIBRATION):
              data[2] |= 0b0000 << 1;
              break;
          case(AMPLIFIER_SELF_CALIBRATION):
              data[2] |= 0b0001 << 1;
              break;
          case(TEMPERATURE_UNBUFFERED):
              data[2] |= 0b1000 << 1;
              break;
          case(PACK_VOLTAGE):
              data[2] |= 0b1100 << 1;
              break;
          case(TEMPERATURE_BUFFERED):
              data[2] |= 0b1100 << 1;
              break;
          default:
              /**< Should never reach here */
              break;
        }

        if (config->config.output.state == TEMPERATURE_BUFFERED ||
            config->config.output.state == TEMPERATURE_UNBUFFERED)
        {
            data[2] |= config->config.output.output.temp << 1;
        }
    }
        
    data[2] |= (config->config.sampling) ? 0 : 1 << 5;
    data[2] |= (config->config.diagnostic_enabled) ? 1 << 6 : 0;
    data[2] |= (config->config.low_power_mode) ? 1 << 7 : 0;
}
