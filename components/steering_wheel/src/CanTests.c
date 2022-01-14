/**
 * CanTests.c
 * Test file for CAN stuff
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "CAN/CanTypes.h"
#include "Display/Common.h"
#include "HW_can.h"
#include "ModuleDesc.h"


/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define CAN_1kHz_MSG_COUNT     1U
#define CAN_100Hz_MSG_COUNT    1U
#define CAN_10Hz_MSG_COUNT     1U
#define CAN_1Hz_MSG_COUNT      1U


/******************************************************************************
 *                              E X T E R N S
 ******************************************************************************/

extern CAN_HandleTypeDef hcan;


/******************************************************************************
 *                             T Y P E D E F S
 ******************************************************************************/

typedef struct
{
    CAN_TxMessage_t msgs_1kHz[CAN_1kHz_MSG_COUNT];
    CAN_TxMessage_t msgs_100Hz[CAN_100Hz_MSG_COUNT];
    CAN_TxMessage_t msgs_10Hz[CAN_10Hz_MSG_COUNT];
    CAN_TxMessage_t msgs_1z[CAN_1Hz_MSG_COUNT];
} CAN_Messages_S;


/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

/* static CAN_Messages_S CAN_Messages; */


/**
 * CanTests_init
 *
 */
static void CanTests_init(void)
{
    HAL_CAN_Start(&hcan);    // start CAN
}


/**
 * CanTests10Hz_PRD
 *
 */
static void CanTests10Hz_PRD(void)
{
    /* if (HAL_OK == CAN_sendMsg(&hcan, msg)) */
    /* { */
    /*     toggleInfoDotState(INFO_DOT_CAN_TX); */
    /* } */
}


/**
 * CanTests100Hz_PRD
 *
 */
static void CanTests100Hz_PRD(void)
{
}


// module description
const ModuleDesc_S CanTests_desc = {
    .moduleInit        = &CanTests_init,
    .periodic10Hz_CLK  = &CanTests10Hz_PRD,
    .periodic100Hz_CLK = &CanTests100Hz_PRD,
};
