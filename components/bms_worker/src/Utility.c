#include "Utility.h"

float32_t ln(float32_t x)
{
    uint32_t bx = *(uint32_t*)(&x);
    uint32_t ex = bx >> 23;
    int32_t  t  = (int32_t)ex - (int32_t)127;

    // unsigned int s = (t < 0) ? (-t) : t;
    bx = 1065353216 | (bx & 8388607);
    x  = *(float32_t*)(&bx);
    return -1.49278 + (2.11263 + (-0.729104 + 0.10969 * x) * x) * x + 0.6931471806 * t;
}
