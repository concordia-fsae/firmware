#[derive(Clone, Copy, Default)]
pub struct Timer {
    start: Option<u32>,
}
impl Timer {
    pub fn expired(&self, now: u32, ms: u32) -> bool {
        self.start.is_some_and(|s| now.wrapping_sub(s) >= ms)
    }
    pub fn running(&self, now: u32, ms: u32) -> bool {
        self.start.is_some() && !self.expired(now, ms)
    }
    pub fn stop(&mut self) {
        self.start = None;
    }
    pub fn restart(&mut self, now: u32) {
        self.start = Some(now);
    }
    pub fn hold(&mut self, condition: bool, now: u32, ms: u32) -> bool {
        if !condition {
            self.stop();
            return false;
        }
        if self.start.is_none() {
            self.restart(now);
        }
        self.expired(now, ms)
    }
}
