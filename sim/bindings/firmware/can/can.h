#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint32_t id;
    uint8_t  len;
    uint8_t  data[8];
} rig_can_packet_S;

uint8_t  rig_runtime_can_bus_count(void);
bool     rig_runtime_can_push_rx(uint8_t bus, const rig_can_packet_S* packet);
bool     rig_runtime_can_pop_rx(uint8_t bus, rig_can_packet_S* packet);
bool     rig_runtime_can_push_tx(uint8_t bus, const rig_can_packet_S* packet);
bool     rig_runtime_can_pop_tx(uint8_t bus, rig_can_packet_S* packet);
uint32_t rig_runtime_can_rx_count(uint8_t bus);
uint32_t rig_runtime_can_tx_count(uint8_t bus);
void     rig_runtime_can_notify_rx(uint8_t bus);
