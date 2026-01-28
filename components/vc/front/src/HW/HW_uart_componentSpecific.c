/**
 * @file HW_adc_componentSpecific.c
 * @brief  Source code for ADC firmware
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "HW_uart.h"
#include "SystemConfig.h"
#include "app_gps.h"

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

DMA_HandleTypeDef hdma_uart3rx;

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Init function for UART firmware
 */
HW_StatusTypeDef_E HW_UART_init_componentSpecific(void)
{
    huart[HW_UART_PORT_GPS].Instance = USART3;
    huart[HW_UART_PORT_GPS].Init.BaudRate = 115200;
    huart[HW_UART_PORT_GPS].Init.WordLength = UART_WORDLENGTH_8B;
    huart[HW_UART_PORT_GPS].Init.StopBits = UART_STOPBITS_1;
    huart[HW_UART_PORT_GPS].Init.Parity = UART_PARITY_NONE;
    huart[HW_UART_PORT_GPS].Init.Mode = UART_MODE_TX_RX;
    huart[HW_UART_PORT_GPS].Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart[HW_UART_PORT_GPS].Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart[HW_UART_PORT_GPS]) != HAL_OK)
    {
        Error_Handler();
    }

    return HW_OK;
}

/**
 * @brief  Callback for STM32 HAL once ADC initialization is complete
 *
 * @param adcHandle pointer to ADC peripheral
 */
void HAL_UART_MspInit(UART_HandleTypeDef* huart)
{
    if(huart->Instance==USART3)
    {
        __HAL_RCC_USART3_CLK_ENABLE();

        __HAL_AFIO_REMAP_USART3_PARTIAL();

        hdma_uart3rx.Instance = DMA1_Channel3;
        hdma_uart3rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
        hdma_uart3rx.Init.PeriphInc = DMA_PINC_DISABLE;
        hdma_uart3rx.Init.MemInc = DMA_MINC_ENABLE;
        hdma_uart3rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_uart3rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        hdma_uart3rx.Init.Mode = DMA_CIRCULAR;
        hdma_uart3rx.Init.Priority = DMA_PRIORITY_LOW;

        if (HAL_DMA_Init(&hdma_uart3rx) != HAL_OK)
        {
            Error_Handler();
        }

        __HAL_LINKDMA(huart,hdmarx,hdma_uart3rx);

        HAL_NVIC_SetPriority(USART3_IRQn, DMA_IRQ_PRIO, 0U);
        HAL_NVIC_EnableIRQ(USART3_IRQn);
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef* huart)
{
    if (huart->Instance == USART3)
    {
        uint8_t* rxBuf = huart->pRxBuffPtr;
        uint16_t rxLen = huart->RxXferSize;

        (void)HAL_UART_DMAStop(huart);
        app_gps_resetBuffers();

        if ((rxBuf != NULL) && (rxLen > 0U))
        {
            (void)HAL_UART_Receive_DMA(huart, rxBuf, rxLen);
        }
    }
}
