#pragma once

// Retain the component configuration and enable checksum checks for this test variant.
#include "lwgps_opts.h"
#undef LWGPS_CFG_CRC
#define LWGPS_CFG_CRC    1
