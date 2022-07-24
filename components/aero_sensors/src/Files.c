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


/******************************************************************************
 *                         P R I V A T E  V A R S
 ******************************************************************************/

char curr_file[] = "A_run.bin"; 

FATFS fs;
FIL file;


/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

void Files_Init(void)
{
    f_mount(&fs, "/", 1);
    f_open(&file, (char*) &curr_file, FA_OPEN_ALWAYS | FA_READ | FA_WRITE);
}

void Files_write(const void * buff, uint32_t count)
{
    UINT bw;

    f_write(&file, buff, count, &bw);
}

void Files_next(void)
{
    f_close(&file);

    curr_file[0]++;

    f_open(&file, (char*) &curr_file, FA_OPEN_ALWAYS | FA_READ | FA_WRITE);
}


