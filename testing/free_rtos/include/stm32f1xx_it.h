/**
 ******************************************************************************
 * @file    stm32f1xx_it.h
 * @brief   This file contains the headers of the interrupt handlers.
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under Ultimate Liberty license
 * SLA0044, the "License"; You may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 *                             www.st.com/SLA0044
 *
 ******************************************************************************
 */


// ------------------------------------------------------------------------- \\
//                                 NOTE                                      \\
// This file has been modified such that it _only_ includes code which is    \\
// _different_ from a typically generated file.                              \\
// ** DO NOT DELETE ** code from your own files that doesn't appear in this  \\
// file.                                                                     \\


/* Exported functions prototypes ---------------------------------------------*/
void TIM4_IRQHandler(void);

// DELETE THE FOLLOWING FROM YOUR FILE IF IT EXISTS

void SVC_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);

// STOP DELETING HERE

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
