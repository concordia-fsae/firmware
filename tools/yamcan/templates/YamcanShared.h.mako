/*
 * YamcanShared.h
 * Shared memory layout for yamcan RX state.
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include <stdint.h>

#include "SigRx.h"

#ifndef YAMCAN_SHARED_SECTION
#define YAMCAN_SHARED_SECTION
#endif

/******************************************************************************
 *                           S H A R E D  S T A T E
 ******************************************************************************/

typedef struct {
    uint32_t seq;
%for node in nodes:
  %for bus in node.on_buses:
    CANRX_${bus.upper()}_signals_S ${bus}_signals;
    CANRX_${bus.upper()}_messages_S ${bus}_messages;
  %endfor
%endfor
} YamcanShared;

extern YamcanShared *g_yamcan;

void YAMCAN_shared_init_static(void);
int YAMCAN_shared_init_shm(const char *name);
