#pragma once

#include <stdint.h>

void     rig_runtime_reset(void);
void     rig_runtime_advance_time_ns(uint64_t elapsed_ns);
uint64_t rig_runtime_get_time_ns(void);
