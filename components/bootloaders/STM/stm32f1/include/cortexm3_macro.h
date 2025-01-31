/*
 * cortexm3_macro.h
 * Function prototypes for the low-level intrinsics defined in cortexm3_macro.S
 */

#pragma once


/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "Types.h"


/******************************************************************************
 *            P U B L I C  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void     __WFI(void);
void     __WFE(void);
void     __SEV(void);
void     __ISB(void);
void     __DSB(void);
void     __DMB(void);
void     __SVC(void);
uint32_t __MRS_CONTROL(void);
void     __MSR_CONTROL(uint32_t Control);
uint32_t __MRS_PSP(void);
void     __MSR_PSP(uint32_t TopOfProcessStack);
uint32_t __MRS_MSP(void);
void     __MSR_MSP(uint32_t TopOfMainStack);
void     __RESETPRIMASK(void);
void     __SETPRIMASK(void);
uint32_t __READ_PRIMASK(void);
void     __RESETFAULTMASK(void);
void     __SETFAULTMASK(void);
uint32_t __READ_FAULTMASK(void);
void     __BASEPRICONFIG(uint32_t NewPriority);
uint32_t __GetBASEPRI(void);
uint16_t __REV_HalfWord(uint16_t Data);
uint32_t __REV_Word(uint32_t Data);
