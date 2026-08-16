// Generic model datapath descriptor and provider contract.
//
// The `interface` field is intentionally opaque to Rig. A consuming binding
// assigns its own interface IDs and translates descriptors into typed paths.

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct ModelDataPathDescriptor {
    pub interface: u16,
    pub port: i32,
    pub channel: i32,
    pub device: i32,
}

pub trait ModelDataPathProvider {
    fn datapath_count(&self) -> u32 {
        0
    }

    fn datapath_descriptor(&self, _index: u32) -> Option<ModelDataPathDescriptor> {
        None
    }
}

pub fn datapath_count<Target, Runtime>(
    model: &mut super::model::NodeModel<Target, Runtime>,
) -> u32
where
    Runtime: super::model::ModelRuntime,
    Target: super::model::NodeTarget<Runtime> + ModelDataPathProvider,
{
    model.target().datapath_count()
}

pub fn datapath_descriptor<Target, Runtime>(
    model: &mut super::model::NodeModel<Target, Runtime>,
    index: u32,
) -> Option<ModelDataPathDescriptor>
where
    Runtime: super::model::ModelRuntime,
    Target: super::model::NodeTarget<Runtime> + ModelDataPathProvider,
{
    model.target().datapath_descriptor(index)
}

#[cfg(test)]
mod tests {
    use super::ModelDataPathDescriptor;

    #[test]
    fn descriptor_has_a_stable_c_layout() {
        assert_eq!(std::mem::size_of::<ModelDataPathDescriptor>(), 16);
        assert_eq!(std::mem::align_of::<ModelDataPathDescriptor>(), 4);
    }
}
