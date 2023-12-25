/*
 * CANIO-tx.c
 * CAN Transmit
 */


/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "CAN/CAN.h"

#include "HW_can.h"

#include "FreeRTOS_SWI.h"
#include "ModuleDesc.h"

#include "string.h"

// imports for data access
#include "IO.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/


/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    uint8_t txBusA10msIdx;
} cantx_S;


/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

static cantx_S cantx;


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * CANTX_BUS_A_10ms_SWI
 * send BUS_A messages
 */
void CANTX_BUS_A_10ms_SWI(void)
{
    // TODO: add overrun detection here

    // static uint8_t    counter = 0U;
    CAN_data_T        message;

    const packTable_S *entry = 0x00;

    if (entry != NULL)
    {
        CAN_sendMsgBus0(CAN_TX_PRIO_100HZ, message, entry->id, entry->len);
        return;
    }
}


/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

/**
 * CANIO_tx_100Hz_PRD
 * module 100Hz periodic function
 */
static void CANIO_tx_100Hz_PRD(void)
{
    // transmit 100Hz messages
    cantx.txBusA10msIdx = 0U;
    SWI_invoke(CANTX_BUS_A_10ms_swi);
}

/**
 * CANIO_tx_10Hz_PRD
 *
 */
static void CANIO_tx_10Hz_PRD(void)
{
    // CAN_sendMsgBus0(CAN_TX_PRIO_10HZ, (CAN_data_T){0x01}, 0x101, 8U);
}

/**
 * CANIO_tx_init
 * initialize module
 */
static void CANIO_tx_init(void)
{
    memset(&cantx, 0x00, sizeof(cantx));
    CAN_Start();    // start CAN
}


const ModuleDesc_S CANIO_tx = {
    .moduleInit        = &CANIO_tx_init,
    .periodic100Hz_CLK = &CANIO_tx_100Hz_PRD,
    .periodic10Hz_CLK  = &CANIO_tx_10Hz_PRD,
};

