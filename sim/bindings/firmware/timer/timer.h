#pragma once

#include "LIB_Types.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    int32_t   port;
    int32_t   channel;
    float32_t value;
    uint64_t  timestamp_ns;
} rig_timer_channel_event_S;

typedef struct
{
    int32_t  channel;
    float32_t value;
    uint64_t timestamp_ns;
} rig_timer_capture_event_S;

bool     rig_runtime_timer_push_duty_input(const rig_timer_channel_event_S* event);
bool     rig_runtime_timer_latest_duty_input(int32_t port, int32_t channel, float32_t* value);
bool     rig_runtime_timer_push_frequency_input(const rig_timer_channel_event_S* event);
bool     rig_runtime_timer_latest_frequency_input(int32_t port, int32_t channel, float32_t* value);
bool     rig_runtime_timer_push_capture_input(const rig_timer_capture_event_S* event);
bool     rig_runtime_timer_latest_capture_input(int32_t channel, float32_t* value);
bool     rig_runtime_timer_push_duty_output(const rig_timer_channel_event_S* event);
bool     rig_runtime_timer_pop_duty_output(int32_t port, int32_t channel, rig_timer_channel_event_S* event);
uint32_t rig_runtime_timer_duty_output_count(int32_t port, int32_t channel);
bool     rig_runtime_timer_push_frequency_output(const rig_timer_channel_event_S* event);
bool     rig_runtime_timer_pop_frequency_output(int32_t port, int32_t channel, rig_timer_channel_event_S* event);
uint32_t rig_runtime_timer_frequency_output_count(int32_t port, int32_t channel);
