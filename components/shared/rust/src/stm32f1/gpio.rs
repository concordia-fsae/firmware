use stm32f1xx_hal::gpio::{ErasedPin, Floating, Input};

/// Physical active-low inputs. Debouncing is handled by the shared input module.
pub struct ActiveLowInputs<const N: usize> {
    pins: [ErasedPin<Input<Floating>>; N],
}
impl<const N: usize> ActiveLowInputs<N> {
    pub fn new(pins: [ErasedPin<Input<Floating>>; N]) -> Self {
        Self { pins }
    }
    pub fn sample(&self) -> [bool; N] {
        let mut raw = [false; N];
        for (active, pin) in raw.iter_mut().zip(&self.pins) {
            *active = pin.is_low();
        }
        raw
    }
}
