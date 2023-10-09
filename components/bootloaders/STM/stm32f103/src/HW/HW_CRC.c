/*
 * HW_CRC.c
 * Implementation of the CRC peripheral
 *
 * The STM32's CRC peripheral performs a CRC-32 calculation with the standard
 * ethernet polynomial: 0x4C11DB7
 *
 * https://www.st.com/resource/en/application_note/an4187-using-the-crc-peripheral-on-stm32-microcontrollers-stmicroelectronics.pdf
 * https://ninja-calc.mbedded.ninja/calculators/software/crc-calculator
 *
 * Actual CRC type here is an MPEG-2 CRC with initial value 0xFFFFFFFF, no inversions,
 * and no output xor
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// module include
#include "HW_CRC.h"

// other includes
#include "Utilities.h"


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/*
 * CRC_reset
 * @brief Reset the CRC peripheral. Effectively just sets the data register to all Fs
 */
void CRC_mpeg2Reset(void)
{
    SET_REG(CRC_CR, GET_REG(CRC_CR) | CRC_CR_RESET);
}


/*
 * CRC_calculate
 * @brief calculate the CRC32 of the input data, with seed value 0xFFFFFFFF
 * @param data uint32_t* the data to calculate the CRC for
 * @param dataLen uint16_t length of the data array
 */
uint32_t CRC_mpeg2Calculate(uint32_t *data, uint16_t dataLen)
{
    CRC_mpeg2Reset();
    for (uint16_t idx = 0U; idx < dataLen; idx++)
    {
        SET_REG(CRC_DR, data[idx]);
    }
    return GET_REG(CRC_DR);
}


/*
 * CRC_init
 * @brief initialize the CRC peripheral
 */
void CRC_init(void)
{
    // enable the crc peripheral clock
    SET_REG(RCC_AHBENR, GET_REG(RCC_AHBENR) | RCC_AHBENR_CRC_CLK);
}
