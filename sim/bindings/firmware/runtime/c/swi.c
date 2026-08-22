#include "runtime_state.h"
#include "swi.h"

RTOS_swiHandle_T* SWI_create(RTOS_swiPri_E priority, RTOS_swiFn_t handler)
{
    if ((priority >= RTOS_SWI_PRI_COUNT) || (rig_runtime_swi_count[priority] >= RTOS_SWI_MAX_PER_PRI))
    {
        return NULL;
    }

    const uint8_t   index    = rig_runtime_swi_count[priority]++;
    RTOS_swiHandle_T* handle = &rig_runtime_swi_handles[priority][index];
    handle->handler  = handler;
    handle->priority = priority;
    handle->event    = 1UL << index;
    return handle;
}

void SWI_invoke(RTOS_swiHandle_T* handle)
{
    if ((handle != NULL) && (handle->handler != NULL))
    {
        handle->handler();
    }
}

bool SWI_invokeFromISR(RTOS_swiHandle_T* handle)
{
    SWI_invoke(handle);
    return true;
}

void SWI_disable(void)
{
}

void SWI_enable(void)
{
}
