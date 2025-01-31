/*
 * HW_NVIC.c
 * This file describes low-level, mostly hardware-specific NVIC behaviour
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

// module include
#include "HW_NVIC.h"

#include "HW.h"
#include "Utilities.h"


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * \brief        Set Interrupt Priority
 * \details      Encodes the priority for an interrupt with the given priority group,
 *               preemptive priority value, and subpriority value.
 *               In case of a conflict between priority grouping and available
 *               priority bits (NVIC_PRIO_BITS), the smallest possible priority group is set.
 *               The interrupt number can be positive to specify a device specific interrupt
 *               or negative to specify a processor exception.
 * \note         The priority cannot be set for every processor exception.
 * \param [in]   preemptPriority  Preemptive priority value (starting from 0).
 * \param [in]   subPriority      Subpriority value (starting from 0).
 */
void NVIC_SetPriority(IRQ_map_E IRQn, uint32_t preemptPriority, uint32_t subPriority)
{
    uint32_t priorityGroup       = (NVIC_PRIGROUP & (uint32_t)0x07UL);  // only values 0 through 7 are used

    uint32_t preemptPriorityBits = ((7UL - priorityGroup) > (uint32_t)(NVIC_PRIO_BITS))
                                   ? (uint32_t)(NVIC_PRIO_BITS)
                                   : (uint32_t)(7UL - priorityGroup);

    uint32_t subPriorityBits = ((priorityGroup + (uint32_t)(NVIC_PRIO_BITS)) < (uint32_t)7UL)
                                   ? (uint32_t)0UL
                                   : (uint32_t)((priorityGroup - 7UL) + (uint32_t)(NVIC_PRIO_BITS));

    uint32_t encodedPriority = (
        ((preemptPriority & (uint32_t)((1UL << (preemptPriorityBits)) - 1UL)) << subPriorityBits) |
        ((subPriority & (uint32_t)((1UL << (subPriorityBits)) - 1UL)))
        );

    if ((int32_t)(IRQn) >= 0)
    {
        pNVIC->IPR[((uint32_t)IRQn)] = (uint8_t)((encodedPriority << (8U - NVIC_PRIO_BITS)) & (uint32_t)0xFFUL);
    }
    else
    {
        pSCB->SHPR[(((uint32_t)IRQn) & 0xFUL) - 4UL] = (uint8_t)((encodedPriority << (8U - NVIC_PRIO_BITS)) & (uint32_t)0xFFUL);
    }
}

/**
 * \brief       Enable Interrupt
 * \details     Enables a device specific interrupt in the NVIC interrupt controller.
 * \param [in]  IRQn  Device specific interrupt number.
 * \note        IRQn must not be negative.
 */
void NVIC_EnableIRQ(IRQ_map_E IRQn)
{
    if ((int32_t)(IRQn) >= 0)
    {
        __COMPILER_BARRIER();
        pNVIC->ISER[(((uint32_t)IRQn) >> 5UL)] = (uint32_t)(1UL << (((uint32_t)IRQn) & 0x1FUL));
        __COMPILER_BARRIER();
    }
}

void NVIC_disableInterrupts(void)
{
    NVIC_regMap *rNVIC = (NVIC_regMap *)NVIC_BASE;

    rNVIC->ICER[0] = 0xFFFFFFFF;
    rNVIC->ICER[1] = 0xFFFFFFFF;
    rNVIC->ICPR[0] = 0xFFFFFFFF;
    rNVIC->ICPR[1] = 0xFFFFFFFF;

    SET_REG(STK_CTRL, 0x04);    // disable the systick, which operates separately from nvic
}
