#pragma once

#include "HW_i2c.h"

// The simulation provides the component's default I2C handle for models that
// need to construct a firmware HW_I2C_Device_S.
extern I2C_HandleTypeDef i2c;
