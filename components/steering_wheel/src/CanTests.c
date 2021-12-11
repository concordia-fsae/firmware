

#include "ModuleDesc.h"
#include "HW_can.h"


extern CAN_HandleTypeDef hcan;
CAN_TxHeaderTypeDef      pHeader;
uint32_t                 TxMailbox;


static void CanTests_init(void)
{
    // initialize structs
    HAL_CAN_Start(&hcan);    // start CAN
}


static void CanTests10Hz_PRD(void)
{
    pHeader.DLC   = 1;               // give message size of 1 byte
    pHeader.IDE   = CAN_ID_STD;      // set identifier to standard
    pHeader.RTR   = CAN_RTR_DATA;    // set data type to remote transmission request?
    pHeader.StdId = 0x101;           // define a standard identifier, used for message identification by filters (switch this for the other microcontroller)

    uint8_t a[8] = { 100, 0, 0, 0, 0, 0, 0, 0 };
    if (HAL_OK == HAL_CAN_AddTxMessage(&hcan, &pHeader, a, &TxMailbox))
    {
        HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
    }
}


static void CanTests100Hz_PRD(void)
{
}


// module description
const ModuleDesc_S CanTests_desc = {
    .moduleInit        = &CanTests_init,
    .periodic10Hz_CLK  = &CanTests10Hz_PRD,
    .periodic100Hz_CLK = &CanTests100Hz_PRD,
};
