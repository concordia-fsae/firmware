#include "HW_spi.h"

#include "rig_runtime.h"
#include "runtime_state.h"

#include "string.h"

static uint16_t rig_spi_clamped_len(uint16_t len)
{
    return (len > RIG_SPI_TRANSACTION_MAX_BYTES) ? RIG_SPI_TRANSACTION_MAX_BYTES : len;
}

static void rig_spi_copy_tx(rig_spi_transaction_S* transaction, const uint8_t* data, uint16_t len)
{
    const uint16_t copy_len = rig_spi_clamped_len(len);

    if ((data != NULL) && (copy_len > 0U))
    {
        memcpy(transaction->tx_data, data, copy_len);
    }
}

static void rig_spi_fill_rx_from_model(HW_spi_device_E dev, uint8_t* data, uint16_t len)
{
    if ((data == NULL) || (len == 0U))
    {
        return;
    }

    rig_spi_transaction_S rx_transaction = { 0 };
    if (rig_runtime_spi_pop_input((int32_t)dev, &rx_transaction))
    {
        const uint16_t copy_len = (rx_transaction.rx_len < len) ? rx_transaction.rx_len : len;
        if (copy_len > 0U)
        {
            memcpy(data, rx_transaction.rx_data, copy_len);
        }
        if (copy_len < len)
        {
            memset(&data[copy_len], 0xFF, (size_t)(len - copy_len));
        }
        return;
    }

    memset(data, 0xFF, len);
}

HW_StatusTypeDef_E HW_SPI_init(void)
{
    return HW_OK;
}

HW_StatusTypeDef_E HW_SPI_init_componentSpecific(void)
{
    return HW_OK;
}

HW_StatusTypeDef_E HW_SPI_deInit(void)
{
    return HW_OK;
}

bool HW_SPI_lock(HW_spi_device_E dev)
{
    return rig_runtime_spi_lock_device((int32_t)dev);
}

bool HW_SPI_release(HW_spi_device_E dev)
{
    return rig_runtime_spi_release_device((int32_t)dev);
}

bool HW_SPI_transmit(HW_spi_device_E dev, uint8_t* data, uint16_t len)
{
    rig_spi_transaction_S transaction = {
        .device       = (int32_t)dev,
        .tx_len       = rig_spi_clamped_len(len),
        .rx_len       =                       0U,
        .timestamp_ns = rig_runtime.time_ns,
    };

    rig_spi_copy_tx(&transaction, data, len);
    if (!rig_runtime_spi_push_output(&transaction))
    {
        return false;
    }
    return true;
}

bool HW_SPI_receive(HW_spi_device_E dev, uint8_t* data, uint16_t len)
{
    const rig_spi_transaction_S transaction = {
        .device       = (int32_t)dev,
        .tx_len       =                       0U,
        .rx_len       = rig_spi_clamped_len(len),
        .timestamp_ns = rig_runtime.time_ns,
    };

    if (!rig_runtime_spi_push_output(&transaction))
    {
        return false;
    }
    rig_spi_fill_rx_from_model(dev, data, len);
    return true;
}

bool HW_SPI_transmitReceive(HW_spi_device_E dev, uint8_t* rwData, uint16_t len)
{
    rig_spi_transaction_S transaction = {
        .device       = (int32_t)dev,
        .tx_len       = rig_spi_clamped_len(len),
        .rx_len       = rig_spi_clamped_len(len),
        .timestamp_ns = rig_runtime.time_ns,
    };

    rig_spi_copy_tx(&transaction, rwData, len);
    if (!rig_runtime_spi_push_output(&transaction))
    {
        return false;
    }
    rig_spi_fill_rx_from_model(dev, rwData, len);
    return true;
}

bool HW_SPI_transmitReceiveAsym(HW_spi_device_E dev, uint8_t* wData, uint16_t wLen, uint8_t* rData, uint16_t rLen)
{
    rig_spi_transaction_S transaction = {
        .device       = (int32_t)dev,
        .tx_len       = rig_spi_clamped_len(wLen),
        .rx_len       = rig_spi_clamped_len(rLen),
        .timestamp_ns = rig_runtime.time_ns,
    };

    rig_spi_copy_tx(&transaction, wData, wLen);
    if (!rig_runtime_spi_push_output(&transaction))
    {
        return false;
    }
    rig_spi_fill_rx_from_model(dev, rData, rLen);
    return true;
}

bool HW_SPI_dmaTransmitReceive(HW_spi_device_E dev, uint8_t* rwData, uint16_t len)
{
    return HW_SPI_transmitReceive(dev, rwData, len);
}

bool HW_SPI_dmaTransmitReceiveAsym(HW_spi_device_E dev, uint8_t* wData, uint16_t wLen, uint8_t* rData, uint16_t rLen)
{
    return HW_SPI_transmitReceiveAsym(dev, wData, wLen, rData, rLen);
}
