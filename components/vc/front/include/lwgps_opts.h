/**
 * \file            lwgps_opts_template.h
 * \brief           LwGPS configuration file
 */

/*
 * Copyright (c) 2024 Tilen MAJERLE
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE
 * AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * This file is part of LwGPS - Lightweight GPS NMEA parser library.
 *
 * Author:          Tilen MAJERLE <tilen@majerle.eu>
 * Version:         v2.2.0
 */
#ifndef LWGPS_OPTS_HDR_H
#define LWGPS_OPTS_HDR_H

#define __DOXYGEN__ 0

/**
 * \defgroup        LWGPS_OPT Configuration
 * \brief           Default configuration setup
 * \{
 */

/**
 * \brief           Enables `1` or disables `0` `double precision` for floating point
 *                  values such as latitude, longitude, altitude.
 *
 *                  `double` is used as variable type when enabled, `float` when disabled.
 */
#define LWGPS_CFG_DOUBLE 0

/**
 * \brief           Enables `1` or disables `0` status reporting callback
 *                  by \ref lwgps_process
 *
 * \note            This is an extension, so not enabled by default.
 */
#define LWGPS_CFG_STATUS 0

/**
 * \brief           Enables `1` or disables `0` `GGA` statement parsing.
 *
 * \note            This statement must be enabled to parse:
 *                      - Latitude, Longitude, Altitude
 *                      - Number of satellites in use, fix (no fix, GPS, DGPS), UTC time
 */
#define LWGPS_CFG_STATEMENT_GPGGA 1

/**
 * \brief           Enables `1` or disables `0` `GSA` statement parsing.
 *
 * \note            This statement must be enabled to parse:
 *                      - Position/Vertical/Horizontal dilution of precision
 *                      - Fix mode (no fix, 2D, 3D fix)
 *                      - IDs of satellites in use
 */
#define LWGPS_CFG_STATEMENT_GPGSA 1

/**
 * \brief           Enables `1` or disables `0` `RMC` statement parsing.
 *
 * \note            This statement must be enabled to parse:
 *                      - Validity of GPS signal
 *                      - Ground speed in knots and coarse in degrees
 *                      - Magnetic variation
 *                      - UTC date
 */
#define LWGPS_CFG_STATEMENT_GPRMC 1

/**
 * \brief           Enables `1` or disables `0` `GSV` statement parsing.
 *
 * \note            This statement must be enabled to parse:
 *                      - Number of satellites in view
 *                      - Optional details of each satellite in view. See \ref LWGPS_CFG_STATEMENT_GPGSV_SAT_DET
 */
#define LWGPS_CFG_STATEMENT_GPGSV 1

/**
 * \brief           Enables `1` or disables `0` detailed parsing of each
 *                  satellite in view for `GSV` statement.
 *
 * \note            When this feature is disabled, only number of "satellites in view" is parsed
 */
#define LWGPS_CFG_STATEMENT_GPGSV_SAT_DET 0

/**
 * \brief           Enables `1` or disables `0` parsing and generation
 *                  of PUBX (uBlox) messages
 *
 *                  PUBX are a nonstandard ublox-specific extensions,
 *                  so disabled by default.
 */
#define LWGPS_CFG_STATEMENT_PUBX 0

/**
 * \brief           Enables `1` or disables `0` parsing and generation
 *                  of PUBX (uBlox) TIME messages.
 *
 * \note            TIME messages can be used to obtain:
 *                      - UTC time of week
 *                      - UTC week number
 *                      - Leap seconds (allows conversion to eg. TAI)
 *
 *                  This is a nonstandard ublox-specific extension,
 *                  so disabled by default.
 *
 *                  This configure option requires LWGPS_CFG_STATEMENT_PUBX
 */
#define LWGPS_CFG_STATEMENT_PUBX_TIME 0

/**
 * \brief           Enables `1` or disables `0` CRC calculation and check
 *
 * \note            When not enabled, CRC check is ignored
 */
#define LWGPS_CFG_CRC 1

/**
 * \brief           Enables `1` or disables `0` distance and bearing calculation
 *
 * \note            When not enabled, corresponding function is disabled
 */
#define LWESP_CFG_DISTANCE_BEARING 1

/**
 * \brief           Memory set function
 * 
 * \note            Function footprint is the same as \ref memset
 */
#define LWGPS_MEMSET(dst, val, len) memset((dst), (val), (len))

/**
 * \brief           Memory copy function
 * 
 * \note            Function footprint is the same as \ref memcpy
 */
#define LWGPS_MEMCPY(dst, src, len) memcpy((dst), (src), (len))

#endif /* LWGPS_OPTS_HDR_H */
