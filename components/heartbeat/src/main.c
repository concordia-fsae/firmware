#include "HW.h"
#include "Utilities.h"

#include <stdint.h>


typedef struct
{
    const uint32_t appStart;
    const uint32_t appEnd;
    const uint32_t appCrcLocation;
} appDesc_S;

extern const uint8_t __app_start_addr[];
extern const uint8_t __app_end_addr[];
extern const uint8_t __app_crc_addr[];

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"

__attribute__((section(".appDescriptor")))
const appDesc_S appDesc = {
    .appStart       = (const uint32_t)__app_start_addr,
    .appEnd         = (const uint32_t)__app_end_addr,
    .appCrcLocation = (const uint32_t)__app_crc_addr,
};
#pragma GCC diagnostic pop


__attribute__((section(".testData")))
const uint32_t testData = 0xFFFFFFFF;

// make the binary bigger to exercise UDS app downloads
__attribute__((__used__))
volatile const uint32_t garbageData[1024] = { 0xFF };

int main()
{
    if (garbageData[0] == 0xFF) {}
    setupCLK();
    setupLEDAndButton();

    for (;;)
    {
        strobePin(LED_BANK, LED_PIN, STARTUP_BLINKS, BLINK_SLOW, LED_ON_STATE);
    }

    return 0;
}
