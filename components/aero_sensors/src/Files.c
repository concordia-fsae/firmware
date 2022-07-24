/**
 * @file Files.c
 * @brief  Source code for filesystem interface
 * @author Joshua Lafleur (josh.lafleur@outlook.com)
 * @version 0.1
 * @date 2022-07-23
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "Files.h"

#include "ff.h"

#include "ErrorHandler.h"


/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

char curr_file[] = "A_run.bin";

FATFS      fs    = { 0 };
FIL        file  = { 0 };
FS_State_E state = 0;


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Mount and open the first file
 */
void Files_Init(void)
{
    f_mount(&fs, "/", 1);
    f_open(&file, (char*)&curr_file, FA_OPEN_ALWAYS | FA_READ | FA_WRITE);
    state = FS_PAUSE;
}

/**
 * @brief  Writes data to the filesystem
 *
 * @param buff Buffer to write from
 * @param count Amount of bytes to write
 */
void Files_Write(const void* buff, uint32_t count)
{
    UINT bw;

    if (state == FS_BUSY)
        Error_Handler(); /**< Indicates timing error */
    if (state == FS_CHANGING_FILE || state == FS_PAUSE)
        return;

    state = FS_BUSY;
    f_write(&file, buff, count, &bw);
    state = FS_READY;
}

/**
 * @brief  Closes the current file and opens the next file
 */
void Files_NextState(void)
{
    switch (state) {
        case FS_PAUSE:
            state = FS_READY;
            break;
        case FS_READY:
            state = FS_PAUSE;
            Files_CloseOpen();
        default:
            break;
    }
}

/**
 * @brief  Puts the filesystem FSM into the ready state
 */
void Files_Start(void)
{
    state = FS_READY;
}

/**
 * @brief  Closes the current file and opens the next
 */
void Files_CloseOpen(void)
{
    if (state == FS_BUSY)
        Error_Handler(); /**< Indicates timing error */

    state = FS_CHANGING_FILE;

    f_close(&file);

    curr_file[0]++;

    f_open(&file, (char*)&curr_file, FA_OPEN_ALWAYS | FA_READ | FA_WRITE);

    state = FS_PAUSE;
}

/**
 * @brief  Returns current filesystem state
 *
 * @retval   Current filesystem state
 */
FS_State_E Files_GetState(void)
{
    return state;
}
