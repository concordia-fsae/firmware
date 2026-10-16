use crate::time::Timer;
#[derive(Clone, Copy, Default)]
pub struct Button {
    pub pressed: bool,
    pub debouncing: bool,
    timer: Timer,
}
pub struct Buttons<const N: usize> {
    pub state: [Button; N],
    thresholds: [(u32, u32); N],
}
impl<const N: usize> Buttons<N> {
    pub fn new(thresholds: [(u32, u32); N]) -> Self {
        Self {
            state: [Button::default(); N],
            thresholds,
        }
    }
    pub fn sample(&mut self, raw: [bool; N], now: u32) {
        for (i, (button, active)) in self.state.iter_mut().zip(raw).enumerate() {
            let (press, release) = self.thresholds[i];
            let ms = if active { press } else { release };
            if active == button.pressed {
                button.timer.stop();
                button.debouncing = false;
            } else if button.timer.hold(true, now, ms) {
                button.pressed = active;
                button.timer.stop();
                button.debouncing = false;
            } else {
                button.debouncing = true;
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::Buttons;
    #[test]
    fn thresholds_and_input_count_are_application_configuration() {
        let mut inputs = Buttons::<2>::new([(5, 2), (20, 10)]);
        inputs.sample([true, true], 0);
        inputs.sample([true, true], 5);
        assert!(inputs.state[0].pressed);
        assert!(!inputs.state[1].pressed);
        inputs.sample([true, true], 20);
        assert!(inputs.state[1].pressed);
        inputs.sample([false, false], 21);
        inputs.sample([false, false], 23);
        assert!(!inputs.state[0].pressed);
        assert!(inputs.state[1].pressed);
        inputs.sample([false, false], 31);
        assert!(!inputs.state[1].pressed);
    }
}
