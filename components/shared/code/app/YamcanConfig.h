/*
 * YamcanConfig.h
 * Project-specific configuration for yamcan-generated code.
 */

#pragma once

#include "CANIO_componentSpecific.h"

#ifndef YAMCAN_TIME_BASE_NS
# define YAMCAN_TIME_BASE_NS    0
#endif

#ifndef YAMCAN_GET_TIME_RAW
# define YAMCAN_GET_TIME_RAW()    (CANIO_getTimeMs())
#endif

#if YAMCAN_TIME_BASE_NS
# define YAMCAN_GET_TIME_MS()    ((uint32_t)(YAMCAN_GET_TIME_RAW() / 1000000U))
#else
# define YAMCAN_GET_TIME_MS()    ((uint32_t)(YAMCAN_GET_TIME_RAW()))
#endif
