#pragma once

#include "drv_inputAD.h"
#include "HW_adc.h"
#include "HW_gpio.h"
#include "LIB_Types.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint32_t id;
    uint8_t  len;
    uint8_t  data[8];
} rig_can_packet_S;

#define RIG_SPI_TRANSACTION_MAX_BYTES    256U

typedef struct
{
    int32_t   port;
    int32_t   channel;
    float32_t value;
    uint64_t  timestamp_ns;
} rig_timer_channel_event_S;

typedef struct
{
    int32_t   channel;
    float32_t value;
    uint64_t  timestamp_ns;
} rig_timer_capture_event_S;

typedef struct
{
    int32_t  device;
    uint16_t tx_len;
    uint16_t rx_len;
    uint8_t  tx_data[RIG_SPI_TRANSACTION_MAX_BYTES];
    uint8_t  rx_data[RIG_SPI_TRANSACTION_MAX_BYTES];
    uint64_t timestamp_ns;
} rig_spi_transaction_S;

void      rig_runtime_reset(void);
void      rig_runtime_advance_time_ns(uint64_t elapsed_ns);
uint64_t  rig_runtime_get_time_ns(void);

void      rig_runtime_set_analog_input(drv_inputAD_channelAnalog_E channel, float32_t voltage);
float32_t rig_runtime_get_analog_input(drv_inputAD_channelAnalog_E channel);

void      rig_runtime_set_digital_io(HW_GPIO_pinmux_E channel, bool state);
bool      rig_runtime_get_digital_io(HW_GPIO_pinmux_E channel);

bool      rig_runtime_get_fault(int fault);

uint8_t   rig_runtime_can_bus_count(void);
bool      rig_runtime_can_push_rx(uint8_t bus, const rig_can_packet_S* packet);
bool      rig_runtime_can_pop_rx(uint8_t bus, rig_can_packet_S* packet);
bool      rig_runtime_can_push_tx(uint8_t bus, const rig_can_packet_S* packet);
bool      rig_runtime_can_pop_tx(uint8_t bus, rig_can_packet_S* packet);
uint32_t  rig_runtime_can_rx_count(uint8_t bus);
uint32_t  rig_runtime_can_tx_count(uint8_t bus);
void      rig_runtime_can_notify_rx(uint8_t bus);

bool      rig_runtime_timer_push_duty_input(const rig_timer_channel_event_S* event);
bool      rig_runtime_timer_latest_duty_input(int32_t port, int32_t channel, float32_t* value);
bool      rig_runtime_timer_push_frequency_input(const rig_timer_channel_event_S* event);
bool      rig_runtime_timer_latest_frequency_input(int32_t port, int32_t channel, float32_t* value);
bool      rig_runtime_timer_push_capture_input(const rig_timer_capture_event_S* event);
bool      rig_runtime_timer_latest_capture_input(int32_t channel, float32_t* value);
bool      rig_runtime_timer_push_duty_output(const rig_timer_channel_event_S* event);
bool      rig_runtime_timer_pop_duty_output(int32_t port, int32_t channel, rig_timer_channel_event_S* event);
uint32_t  rig_runtime_timer_duty_output_count(int32_t port, int32_t channel);
bool      rig_runtime_timer_push_frequency_output(const rig_timer_channel_event_S* event);
bool      rig_runtime_timer_pop_frequency_output(int32_t port, int32_t channel, rig_timer_channel_event_S* event);
uint32_t  rig_runtime_timer_frequency_output_count(int32_t port, int32_t channel);

bool      rig_runtime_spi_push_input(const rig_spi_transaction_S* transaction);
bool      rig_runtime_spi_pop_input(int32_t device, rig_spi_transaction_S* transaction);
bool      rig_runtime_spi_push_output(const rig_spi_transaction_S* transaction);
bool      rig_runtime_spi_pop_output(int32_t device, rig_spi_transaction_S* transaction);
uint32_t  rig_runtime_spi_output_count(int32_t device);
