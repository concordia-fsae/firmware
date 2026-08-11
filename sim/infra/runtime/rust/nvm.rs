pub type Storage = u16;

pub const STORAGE_WORD_BYTES: usize = core::mem::size_of::<Storage>();
pub const FEATURE_ENABLED: u32 = 1;
pub const FLASH_BACKED_BLOCK_COUNT: usize = 2;

unsafe extern "C" {
    static mut __FLASH_NVM_ORIGIN: Storage;
    static mut __FLASH_NVM_END: Storage;
}

pub struct ControllerNvm<const BLOCK_SIZE: usize, const LIB_ENABLED: u32, const FLASH_BACKED: u32>;

impl<const BLOCK_SIZE: usize, const LIB_ENABLED: u32, const FLASH_BACKED: u32>
    ControllerNvm<BLOCK_SIZE, LIB_ENABLED, FLASH_BACKED>
{
    pub const ENABLED: bool = LIB_ENABLED == FEATURE_ENABLED;
    pub const FLASH_BACKED_ENABLED: bool = FLASH_BACKED == FEATURE_ENABLED;
    pub const STORAGE_WORD_BYTES: usize = STORAGE_WORD_BYTES;
    pub const STORAGE_BYTES: usize = if Self::ENABLED && Self::FLASH_BACKED_ENABLED {
        BLOCK_SIZE * FLASH_BACKED_BLOCK_COUNT
    } else {
        0
    };
    pub const END_OFFSET_BYTES: usize = if Self::STORAGE_BYTES >= STORAGE_WORD_BYTES {
        Self::STORAGE_BYTES - STORAGE_WORD_BYTES
    } else {
        0
    };

    pub const fn new() -> Self {
        if Self::ENABLED {
            assert!(
                Self::FLASH_BACKED_ENABLED,
                "controller NVM model requires NVM_FLASH_BACKED"
            );
            assert!(
                Self::STORAGE_BYTES >= STORAGE_WORD_BYTES,
                "controller NVM storage must contain at least one storage word"
            );
        }
        Self
    }

    pub fn reset(&self) {
        if Self::ENABLED {
            reset_flash_backed_storage();
        }
    }
}

impl<const BLOCK_SIZE: usize, const LIB_ENABLED: u32, const FLASH_BACKED: u32> Default
    for ControllerNvm<BLOCK_SIZE, LIB_ENABLED, FLASH_BACKED>
{
    fn default() -> Self {
        Self::new()
    }
}

pub fn reset_flash_backed_storage() {
    unsafe {
        let origin = core::ptr::addr_of_mut!(__FLASH_NVM_ORIGIN);
        let end = core::ptr::addr_of_mut!(__FLASH_NVM_END);
        let len = end.offset_from(origin) as usize + 1;
        core::ptr::write_bytes(origin, 0xFF, len);
    }
}
