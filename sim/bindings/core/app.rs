#[repr(C)]
pub struct AppDesc {
    pub app_start: u32,
    pub app_end: u32,
    pub app_crc_location: u32,
    pub app_component_id: u16,
    pub app_variant_id: u16,
}

impl AppDesc {
    pub const fn new(
        app_start: u32,
        app_end: u32,
        app_crc_location: u32,
        app_component_id: u16,
        app_variant_id: u16,
    ) -> Self {
        Self {
            app_start,
            app_end,
            app_crc_location,
            app_component_id,
            app_variant_id,
        }
    }
}
