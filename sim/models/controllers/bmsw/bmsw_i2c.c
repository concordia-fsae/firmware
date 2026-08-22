#include "HW_i2c.h"

// Environment.c is wired to the BMSW controller's second I2C peripheral.
// Keep that firmware-specific handle with the BMSW model; the shared I2C
// binding only owns the generic simulation handle and transfer functions.
I2C_HandleTypeDef i2c2;
