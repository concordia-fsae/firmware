#pragma once

#include <stdint.h>

typedef uint32_t StackType_t;
typedef uint32_t UBaseType_t;

typedef struct
{
    uint32_t opaque;
} StaticTask_t;

typedef void* TaskHandle_t;

#define pdFALSE    0
#define pdTRUE     1
