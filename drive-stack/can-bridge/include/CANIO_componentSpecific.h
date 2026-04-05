/*
 * Minimal CANIO_componentSpecific.h for can-bridge (userspace).
 */

#pragma once

#include <stdint.h>
#include <time.h>

#define CANIO_UDS_BUFFER_LENGTH    8U

static inline uint32_t CANIO_getTimeMs(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)((ts.tv_sec * 1000U) + (ts.tv_nsec / 1000000U));
}
