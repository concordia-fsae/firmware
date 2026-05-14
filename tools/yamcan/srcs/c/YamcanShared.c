/*
 * YamcanShared.c
 * Shared memory backing for yamcan RX state.
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "YamcanConfig.h"
#include "YamcanShared.h"

#include <string.h>

#if defined(YAMCAN_USE_SHM)
# include <errno.h>
# include <fcntl.h>
# include <sys/mman.h>
# include <sys/stat.h>
# include <unistd.h>
#endif

/******************************************************************************
 *                           S H A R E D  S T A T E
 ******************************************************************************/

YamcanShared YAMCAN_SHARED_SECTION g_yamcan_static;
YamcanShared                       *g_yamcan = &g_yamcan_static;

void YAMCAN_shared_init_static(void)
{
    g_yamcan = &g_yamcan_static;
    memset(g_yamcan, 0, sizeof(*g_yamcan));
}

int YAMCAN_shared_init_shm(const char *name)
{
#if defined(YAMCAN_USE_SHM)
    const size_t size = sizeof(YamcanShared);
    int          fd   = shm_open(name, O_RDWR | O_CREAT, 0666);
    if (fd < 0)
    {
        return -1;
    }
    if (ftruncate(fd, (off_t)size) != 0)
    {
        close(fd);
        return -1;
    }
    void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (ptr == MAP_FAILED)
    {
        return -1;
    }
    g_yamcan = (YamcanShared *)ptr;
    return 0;
#else // if defined(YAMCAN_USE_SHM)
    (void)name;
    return -1;
#endif // if defined(YAMCAN_USE_SHM)
}

