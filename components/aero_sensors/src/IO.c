/**
 * @file IO.c
 * @brief  Source code for ARS Input Output
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @version 0.1
 * @date 2022-07-23
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "IO.h"

#include "Files.h"
#include "ModuleDesc.h"


/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

void LED_SetColor(FS_State_E state);
void IO_Init(void);
void IO_Read(void);


/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

uint8_t led[3] = { 0 };

const HW_GPIO_S inputs[] = {
    [BUTTON] = {
        .port = BUTTON_Port,
        .pin  = BUTTON_Pin,
    }
};

const HW_GPIO_S outputs[] = {
    [RED_LED] = {
        .port = LED_R_Port,
        .pin  = LED_R_Pin,
    },
    [GREEN_LED] = {
        .port = LED_G_Port,
        .pin  = LED_G_Pin,
    },
    [BLUE_LED] = {
        .port = LED_B_Port,
        .pin  = LED_B_Pin,
    }
};


/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

void LED_SetColor(FS_State_E state)
{
    if (state == FS_BUSY)
    {
        led[RED_LED]   = 255;
        led[GREEN_LED] = 0;
        led[BLUE_LED]  = 0;
    }
    else if (state == FS_PAUSE)
    {
        led[RED_LED]   = 0;
        led[GREEN_LED] = 0;
        led[BLUE_LED]  = 255;
    }
    else if (state == FS_READY)
    {
        led[RED_LED]   = 0;
        led[GREEN_LED] = 255;
        led[BLUE_LED]  = 0;
    }
}

void IO_Read(void)
{
    static uint8_t input_flag = 0;

    for (int i = 0; i < INPUT_COUNT; i++)
    {
        if (HW_GPIO_ReadPin(&inputs[i]) == HW_PIN_SET)
        {
            if (!(input_flag & 1 << i))
            {
                input_flag |= 1 << i;

                if (i == BUTTON)
                {
                   Files_NextState(); 
                }
            }
        }
        else
        {
            input_flag &= ~(1 << i);
        }
    }

    LED_SetColor(Files_GetState());

    for (int i = 0; i < LED_COUNT; i++)
    {
        if (led[i])
            HW_GPIO_WritePin(&outputs[i], HW_PIN_SET);
        else
            HW_GPIO_WritePin(&outputs[i], HW_PIN_RESET);
    }
}


/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

const ModuleDesc_S IO_desc = {
    .moduleInit       = &IO_Read,
    .periodic10Hz_CLK = &IO_Read,
};
