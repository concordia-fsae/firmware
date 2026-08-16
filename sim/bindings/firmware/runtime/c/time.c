#include "runtime.h"

#include "runtime_state.h"

#include "HW_tim.h"

void rig_runtime_advance_time_ns(uint64_t elapsed_ns)
{
    rig_runtime.time_ns += elapsed_ns;
}

uint32_t HW_TIM_getTimeMS(void)
{
    return (uint32_t)(rig_runtime.time_ns / 1000000ULL);
}

uint32_t HW_getTick(void)
{
    return HW_TIM_getTimeMS();
}

uint64_t rig_runtime_get_time_ns(void)
{
    return rig_runtime.time_ns;
}

uint64_t HW_TIM_getBaseTick(void)
{
    return rig_runtime.time_ns / 1000ULL;
}
