/**
 * @file Sensors.c
 * @brief  Source code for the ARS sensors
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @version 0.1
 * @date 2022-07-23
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "Sensors.h"

#include "ModuleDesc.h"
#include "Utility.h"
#include "Files.h"

#include "HW_MAX7356.h"
#include "HW_MPRL.h"
#include "HW_NPA.h"
#include "HW_clock.h"

#include "string.h"


/******************************************************************************
 *          P R I V A T E  F U N C T I O N  P R O T O T Y P E S
 ******************************************************************************/

static void Sensors_Init(void);
static void Sensors_Read_100Hz(void);


/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

Sensors_S data = { 0 };

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Blocks the system for 5ms for external initialization and opens all I2C mux's for read entry
 */
static void Sensors_Init(void)
{
    memset(&data, 0x00, sizeof(Sensors_S));
    HW_Delay(5);
    MAX_SetGates(MAX_ALL_GATES);
    Files_Init("'timestamp', 'diff_status', 'diff_press', 'temp', 'press_16'\n", 
                ">IcHH16H\n");
}

/**
 * @brief  Read from all sensors and populate data struct in blocking mode
 *
 * @note Assumes entry with all mux's open
 */
static void Sensors_Read_100Hz(void)
{
    uint8_t start_time;

    FS_State_E file_state = Files_GetState();

    if (file_state != FS_READY) return;

    MPRL_StartConversion();
    start_time = HW_GetTick();

    if (data.timestamp != 0x00) Files_Write(&data, sizeof(data));

    data.npa = NPA_Read();
    
    while ((HW_GetTick() - start_time) < 5);

    data.timestamp = start_time;
    uint8_t index = 0;

    for (int i = 0; i < MAX(MPRL_BUS1_COUNT, MPRL_BUS2_COUNT); i++) {
        MAX_SetGates(MAX_GATE(i));
        if (i < MPRL_BUS1_COUNT)
            data.pressure[index++] = MPRL_ReadData(I2C_Bus1);
        if (i < MPRL_BUS2_COUNT)
            data.pressure[index++] = MPRL_ReadData(I2C_Bus2);
    }
    MAX_SetGates(MAX_ALL_GATES);
}

/******************************************************************************
 *                           P U B L I C  V A R S
 ******************************************************************************/

const ModuleDesc_S Sensors_desc = {
    .moduleInit        = &Sensors_Init,
    .periodic100Hz_CLK = &Sensors_Read_100Hz,
};
