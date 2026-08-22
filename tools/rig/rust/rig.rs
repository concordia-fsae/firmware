// Generic ownership and lifecycle container for Rig elements.
//
// A Rig owns element identity, online state, and elapsed time. Element
// implementations provide only their own reset and execution behavior; they
// do not know whether the owner is a firmware binding, a physics model, or a
// test backend.

pub trait RigElement {
    unsafe fn reset(&mut self);

    unsafe fn run_for_ns(&mut self, duration_ns: u64);
}

pub struct Rig<T> {
    elements: Vec<T>,
    online: Vec<bool>,
    elapsed_ns: u64,
}

impl<T> Default for Rig<T> {
    fn default() -> Self {
        Self::new()
    }
}

impl<T> Rig<T> {
    pub const fn new() -> Self {
        Self {
            elements: Vec::new(),
            online: Vec::new(),
            elapsed_ns: 0,
        }
    }

    pub fn add(&mut self, element: T) -> usize {
        let index = self.elements.len();
        self.elements.push(element);
        self.online.push(true);
        index
    }

    pub fn element(&self, index: usize) -> Option<&T> {
        self.elements.get(index)
    }

    pub fn element_mut(&mut self, index: usize) -> Option<&mut T> {
        self.elements.get_mut(index)
    }

    pub fn len(&self) -> usize {
        self.elements.len()
    }

    pub fn is_empty(&self) -> bool {
        self.elements.is_empty()
    }

    pub fn elapsed_ns(&self) -> u64 {
        self.elapsed_ns
    }

    pub fn is_online(&self, index: usize) -> Option<bool> {
        self.online.get(index).copied()
    }

    pub fn set_online(&mut self, index: usize, online: bool) -> bool {
        let Some(current) = self.online.get_mut(index) else {
            return false;
        };
        *current = online;
        true
    }
}

impl<T: RigElement> Rig<T> {
    pub unsafe fn reset(&mut self) {
        for element in &mut self.elements {
            unsafe { element.reset() };
        }
        self.online.fill(true);
        self.elapsed_ns = 0;
    }

    pub unsafe fn run_for_ns(&mut self, duration_ns: u64) {
        for (element, online) in self.elements.iter_mut().zip(&self.online) {
            if *online {
                unsafe { element.run_for_ns(duration_ns) };
            }
        }
        self.elapsed_ns = self.elapsed_ns.saturating_add(duration_ns);
    }
}

#[cfg(test)]
mod tests {
    use super::{Rig, RigElement};

    #[derive(Default)]
    struct Element {
        resets: usize,
        runs: Vec<u64>,
    }

    impl RigElement for Element {
        unsafe fn reset(&mut self) {
            self.resets += 1;
            self.runs.clear();
        }

        unsafe fn run_for_ns(&mut self, duration_ns: u64) {
            self.runs.push(duration_ns);
        }
    }

    #[test]
    fn rig_owns_element_lifecycle_and_online_gating() {
        let mut rig = Rig::new();
        let first = rig.add(Element::default());
        let second = rig.add(Element::default());

        assert_eq!(rig.len(), 2);
        assert_eq!(rig.is_online(first), Some(true));
        assert!(rig.set_online(second, false));

        unsafe { rig.run_for_ns(10) };
        assert_eq!(rig.element(first).unwrap().runs, vec![10]);
        assert!(rig.element(second).unwrap().runs.is_empty());
        assert_eq!(rig.elapsed_ns(), 10);

        unsafe { rig.reset() };
        assert_eq!(rig.element(first).unwrap().resets, 1);
        assert_eq!(rig.element(second).unwrap().resets, 1);
        assert_eq!(rig.elapsed_ns(), 0);
        assert_eq!(rig.is_online(second), Some(true));
    }
}
