// Allocation-free CAN wire operations shared by Yamcan's Rust wrappers.

pub const fn mask(width: u8) -> u64 {
    if width >= 64 {
        u64::MAX
    } else {
        (1u64 << width) - 1
    }
}

pub fn get_bits(data: &[u8; 8], start: u8, width: u8) -> u64 {
    (u64::from_le_bytes(*data) >> start) & mask(width)
}

pub fn set_bits(data: &mut [u8; 8], start: u8, width: u8, raw: u64) {
    let mask = mask(width) << start;
    *data = ((u64::from_le_bytes(*data) & !mask) | ((raw << start) & mask)).to_le_bytes();
}

pub fn decode_raw(raw: u64, width: u8, signed: bool, scale: f64, offset: f64) -> f64 {
    let value = if signed && width > 0 && raw & (1u64 << (width - 1)) != 0 {
        (raw | !mask(width)) as i64 as f64
    } else {
        raw as f64
    };
    value * scale + offset
}

pub fn encode_raw(value: f64, scale: f64, offset: f64, min: f64, max: f64) -> u64 {
    ((value.max(min).min(max) - offset) / scale) as i64 as u64
}

pub fn apply_checksum(data: &mut [u8; 8], len: u8, start: u8, width: u8, seed: u32) {
    set_bits(data, start, width, 0);
    let sum = data
        .iter()
        .take(len as usize)
        .fold(seed, |s, b| s.wrapping_add(*b as u32));
    set_bits(data, start, width, sum as u64);
}

pub fn valid_checksum(data: &[u8; 8], len: u8, start: u8, width: u8, seed: u32) -> bool {
    let mut expected = *data;
    apply_checksum(&mut expected, len, start, width, seed);
    get_bits(data, start, width) == get_bits(&expected, start, width)
}
