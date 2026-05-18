use std::alloc::{GlobalAlloc, Layout, System, __default_lib_allocator};

#[rustc_std_internal_symbol]
#[linkage = "weak"]
pub unsafe fn __rust_alloc(size: usize, align: usize) -> *mut u8 {
    unsafe { __default_lib_allocator::__rdl_alloc(size, align) }
}

#[rustc_std_internal_symbol]
#[linkage = "weak"]
pub unsafe fn __rdl_alloc(size: usize, align: usize) -> *mut u8 {
    unsafe {
        let layout = Layout::from_size_align_unchecked(size, align);
        System.alloc(layout)
    }
}

#[rustc_std_internal_symbol]
#[linkage = "weak"]
pub unsafe fn __rust_alloc_zeroed(size: usize, align: usize) -> *mut u8 {
    unsafe { __default_lib_allocator::__rdl_alloc_zeroed(size, align) }
}

#[rustc_std_internal_symbol]
#[linkage = "weak"]
pub unsafe fn __rdl_alloc_zeroed(size: usize, align: usize) -> *mut u8 {
    unsafe {
        let layout = Layout::from_size_align_unchecked(size, align);
        System.alloc_zeroed(layout)
    }
}

#[rustc_std_internal_symbol]
#[linkage = "weak"]
pub unsafe fn __rust_dealloc(ptr: *mut u8, size: usize, align: usize) {
    unsafe {
        __default_lib_allocator::__rdl_dealloc(ptr, size, align);
    }
}

#[rustc_std_internal_symbol]
#[linkage = "weak"]
pub unsafe fn __rdl_dealloc(ptr: *mut u8, size: usize, align: usize) {
    unsafe {
        let layout = Layout::from_size_align_unchecked(size, align);
        System.dealloc(ptr, layout);
    }
}

#[rustc_std_internal_symbol]
#[linkage = "weak"]
pub unsafe fn __rust_realloc(
    ptr: *mut u8,
    old_size: usize,
    align: usize,
    new_size: usize,
) -> *mut u8 {
    unsafe { __default_lib_allocator::__rdl_realloc(ptr, old_size, align, new_size) }
}

#[rustc_std_internal_symbol]
#[linkage = "weak"]
pub unsafe fn __rdl_realloc(
    ptr: *mut u8,
    old_size: usize,
    align: usize,
    new_size: usize,
) -> *mut u8 {
    unsafe {
        let old_layout = Layout::from_size_align_unchecked(old_size, align);
        System.realloc(ptr, old_layout, new_size)
    }
}
