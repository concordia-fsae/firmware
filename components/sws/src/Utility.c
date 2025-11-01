#include "Utility.h"

float32_t ln(float32_t x)
{
    uint32_t bx = *(uint32_t*)(&x);
    uint32_t ex = bx >> 23;
    int32_t  t  = (int32_t)ex - (int32_t)127;

    // unsigned int s = (t < 0) ? (-t) : t;
    bx = 1065353216 | (bx & 8388607);
    x  = *(float32_t*)(&bx);
    return (float32_t)(-1.49278f + (2.11263f + (-0.729104f + 0.10969f * x) * x) * x + 0.6931471806f * (float32_t)t);
}

uint8_t* reverse_bytes(uint8_t* in, uint8_t len)
{
    for (uint8_t i = 0; i < (len / 2); i++)
    {
        uint8_t tmp = in[i];

        in[i]           = in[len - i - 1];
        in[len - i - 1] = tmp;
    }

    return in;
}
