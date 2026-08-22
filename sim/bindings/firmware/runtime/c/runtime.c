#include "runtime.h"

#include "runtime_state.h"

#include "CAN/CAN.h"
#include "lib_nvm.h"

#include <string.h>

rig_runtime_state_S rig_runtime;
RTOS_swiHandle_T    rig_runtime_swi_handles[RTOS_SWI_PRI_COUNT][RTOS_SWI_MAX_PER_PRI];
uint8_t             rig_runtime_swi_count[RTOS_SWI_PRI_COUNT];

RTOS_swiHandle_T    * CANRX_swi;
RTOS_swiHandle_T    * CANTX_swi;
RTOS_swiHandle_T    * NVM_swi;

void rig_runtime_reset(void)
{
    memset(&rig_runtime,            0x00, sizeof(rig_runtime));
    memset(rig_runtime_swi_handles, 0x00, sizeof(rig_runtime_swi_handles));
    memset(rig_runtime_swi_count,   0x00, sizeof(rig_runtime_swi_count));
    CANRX_swi = NULL;
    CANTX_swi = NULL;
    NVM_swi   = NULL;
#if FEATURE_IS_ENABLED(FEATURE_CANRX_SWI)
    CANRX_swi = SWI_create(RTOS_SWI_PRI_0, &CANRX_SWI);
#endif
#if FEATURE_IS_ENABLED(FEATURE_CANTX_SWI)
    CANTX_swi = SWI_create(RTOS_SWI_PRI_0, &CANTX_SWI);
#endif
}
