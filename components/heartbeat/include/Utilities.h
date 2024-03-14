#pragma once
#include "Types.h"


#define SET_REG(addr, val)    do { *(vu32*)(addr) = val; } while (0)
#define GET_REG(addr)         (*(vu32*)(addr))

#define asm __asm__
