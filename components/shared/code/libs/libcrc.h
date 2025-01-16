/*
 * libcrc.c
 * Various CRC calculation utils
 */

#pragma once

// component specific configuration
#include "libcrc_componentSpecific.h"

// system includes
#include "stdint.h"

#if defined(LIBCRC_CRC8_FAST) || defined(LIBCRC_CRC8_SLOW)
/*
 * crc8_calculate
 * @brief calculate the CRC8 checksum for the given data. This will either be the
 *        slow version (which consumes less memory) or the fast version (which uses
 *        a lookup table), depending on which of LIBCRC_CRC8_FAST or LIBCRC_CRC8_SLOW
 *        is defined
 * @param crc uint8_t initial crc value to use
 * @param data uint8_t* data array to calculate the CRC8 for
 * @param len uint16_t length of the data array
 */
uint8_t crc8_calculate(uint8_t crc, const uint8_t *data, uint16_t len);
#endif
