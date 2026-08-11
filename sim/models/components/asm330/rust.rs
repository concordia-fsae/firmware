use super::spi::{self, SpiTransaction};

const ASM330_READ_BIT: u8 = 0x80;
const ASM330_REGISTER_MASK: u8 = 0x7F;
const ASM330_ID: u8 = 0x6B;
const ASM330_WHO_AM_I: u8 = 0x0F;
const ASM330_FSM_STATUS_A_MAINPAGE: u8 = 0x36;
const ASM330_FSM_STATUS_B_MAINPAGE: u8 = 0x37;
const ASM330_FIFO_STATUS1: u8 = 0x3A;
const ASM330_FIFO_DATA_OUT_TAG: u8 = 0x78;
const ASM330_GYRO_NC_TAG: u8 = 0x01;
const ASM330_XL_NC_TAG: u8 = 0x02;

pub fn bind_zero_model(device: i32) {
    spi::configure_responder(device, Some(zero_response));
}

#[unsafe(no_mangle)]
pub extern "C" fn rig_runtime_asm330_bind_zero_model(device: i32) {
    bind_zero_model(device);
}

fn zero_response(transaction: SpiTransaction) -> Option<SpiTransaction> {
    if transaction.tx_len == 0 || transaction.rx_len == 0 {
        return None;
    }

    let command = transaction.tx_data[0];
    if command & ASM330_READ_BIT == 0 {
        return None;
    }

    let mut response = SpiTransaction {
        device: transaction.device,
        rx_len: transaction.rx_len,
        timestamp_ns: transaction.timestamp_ns,
        ..SpiTransaction::default()
    };
    let rx_len = usize::from(
        response
            .rx_len
            .min(spi::RIG_SPI_TRANSACTION_MAX_BYTES as u16),
    );
    let register = command & ASM330_REGISTER_MASK;

    match register {
        ASM330_WHO_AM_I => response.rx_data[0] = ASM330_ID,
        ASM330_FSM_STATUS_A_MAINPAGE | ASM330_FSM_STATUS_B_MAINPAGE => {
            response.rx_data[..rx_len].fill(0x00);
        }
        ASM330_FIFO_STATUS1 => {
            response.rx_data[0] = 2;
            if rx_len > 1 {
                response.rx_data[1] = 0;
            }
        }
        ASM330_FIFO_DATA_OUT_TAG => fill_zero_fifo_samples(&mut response.rx_data[..rx_len]),
        _ => {
            response.rx_data[..rx_len].fill(0x00);
        }
    }

    Some(response)
}

fn fill_zero_fifo_samples(data: &mut [u8]) {
    const ASM330_FIFO_ELEMENT_BYTES: usize = 7;
    for (index, sample) in data.chunks_mut(ASM330_FIFO_ELEMENT_BYTES).enumerate() {
        sample.fill(0x00);
        if sample.len() == ASM330_FIFO_ELEMENT_BYTES {
            sample[0] = if index % 2 == 0 {
                ASM330_GYRO_NC_TAG
            } else {
                ASM330_XL_NC_TAG
            };
        }
    }
}
