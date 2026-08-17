#include "LIB_Types.h"

// The production BMSW cooling module references the tachometer capture
// callbacks supplied by its MCU timer component.  Rig's timer binding owns
// the simulated peripheral, so the host model supplies the component's
// no-signal capture result without linking STM32 timer implementation code.
float32_t HW_TIM1_getFreqCH1(void)
{
    return 0.0f;
}

float32_t HW_TIM1_getFreqCH2(void)
{
    return 0.0f;
}
