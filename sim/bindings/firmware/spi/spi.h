#pragma once

#include <stdbool.h>
#include <stdint.h>

#define RIG_SPI_TRANSACTION_MAX_BYTES 256U

typedef struct
{
    int32_t  device;
    uint16_t tx_len;
    uint16_t rx_len;
    uint8_t  tx_data[RIG_SPI_TRANSACTION_MAX_BYTES];
    uint8_t  rx_data[RIG_SPI_TRANSACTION_MAX_BYTES];
    uint64_t timestamp_ns;
} rig_spi_transaction_S;

bool     rig_runtime_spi_push_input(const rig_spi_transaction_S* transaction);
bool     rig_runtime_spi_pop_input(int32_t device, rig_spi_transaction_S* transaction);
bool     rig_runtime_spi_lock_device(int32_t device);
bool     rig_runtime_spi_release_device(int32_t device);
bool     rig_runtime_spi_push_output(const rig_spi_transaction_S* transaction);
bool     rig_runtime_spi_pop_output(int32_t device, rig_spi_transaction_S* transaction);
uint32_t rig_runtime_spi_output_count(int32_t device);
