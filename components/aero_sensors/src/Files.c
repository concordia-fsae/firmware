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

char curr_file[] = "A.bin";

FATFS      fs    = { 0 };
FIL        file  = { 0 };
FS_State_E state = 0;


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

/**
 * @brief  Mount the drive, write config file, and open first file
 *
 * @param heading heading of csv
 * @param structure python struct_fmt https://docs.python.org/3/library/struct.html 
 */
void Files_Init(char* heading, char* structure)
{
    uint16_t heading_cnt = 0;
    uint16_t structure_cnt = 0;

    f_mount(&fs, "/", 1);
    f_open(&file, "config.txt", FA_OPEN_ALWAYS | FA_READ | FA_WRITE);

    for (char* i = heading; *i != 0x00; i++)
        heading_cnt++;
    for (char* i = structure; *i != 0x00; i++)
        structure_cnt++;

    state = FS_READY; //Could be bug seeing as State needs to be ready by default
    Files_Write(heading, heading_cnt);
    Files_Write(structure, structure_cnt);

    f_close(&file);

   f_open(&file, (char*)&curr_file, FA_OPEN_ALWAYS | FA_READ | FA_WRITE);
   state = FS_PAUSE;
  // f_mount(0, "", 0); Need to unmount device at end of program
}

/**
 * @brief  Writes data to the filesystem
 *
 * @param buff Buffer to write from
 * @param count Amount of bytes to write
 */
void Files_Write(const void* buff, uint32_t count)
{
    UINT bw; /**< bytes written */

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
            Files_openNext();
            break;
        case FS_BUSY:
            state = FS_ERROR;
            break;
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
void Files_openNext(void)
{
    if (state == FS_BUSY || state == FS_CHANGING_FILE)
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
