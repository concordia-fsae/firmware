/**
 * STM32F103C8.ld
 * Linker script for STM32F103C8
 */

/* Entry Point */
ENTRY(Reset_Handler)

/* Highest address of the user mode stack */
_estack = ORIGIN(RAM) + LENGTH(RAM);    /* bottom of stack (grows down) */

__STACK_SIZE = 0x400;
__HEAP_SIZE = 0x200;

#if MCU_STM32_PN == FDEFS_STM32_PN_STM32F103XB
__RAM_SIZE = 20K;
__FLASH_SIZE = 64K;
#elif MCU_STM32_PN == FDEFS_STM32_PN_STM32F105
__RAM_SIZE = 64K;
__FLASH_SIZE = 256K;
#else
#error "Chipset not supported"
#endif
__BOOT_FLASH_SIZE = 8K;

#if FEATURE_IS_ENABLED(NVM_LIB_ENABLED)
__NVM_SIZE = 2 * NVM_BLOCK_SIZE;
#else
__NVM_SIZE = 0;
#endif

__RAM_ORIGIN = 0x20000000;     /* don't change */
__FLASH_ORIGIN = 0x8000000;    /* don't change */
__APP_FLASH_ORIGIN = __FLASH_ORIGIN + __BOOT_FLASH_SIZE;
__APP_FLASH_SIZE = __FLASH_SIZE - __NVM_SIZE - __BOOT_FLASH_SIZE;
__FLASH_END = __FLASH_ORIGIN + __FLASH_SIZE - __NVM_SIZE;
__BOOT_FLASH_END = __FLASH_ORIGIN + __BOOT_FLASH_SIZE;

/* Memory definition */
MEMORY
{
    RAM      (rwx)    : ORIGIN = __RAM_ORIGIN,       LENGTH = __RAM_SIZE
#if APP_FUNCTION_ID == FDEFS_FUNCTION_ID_BL
    FLASH    (rx)     : ORIGIN = __FLASH_ORIGIN,     LENGTH = __BOOT_FLASH_SIZE
#elif APP_FUNCTION_ID == FDEFS_FUNCTION_ID_BLU
    FLASH    (rx)     : ORIGIN = __APP_FLASH_ORIGIN, LENGTH = __APP_FLASH_SIZE
#else
#error "Function not supported"
#endif
#if FEATURE_IS_ENABLED(NVM_LIB_ENABLED)
    NVM      (rw)     : ORIGIN = __FLASH_END,        LENGTH = __NVM_SIZE
#endif
}

SECTIONS
{
#if APP_FUNCTION_ID == FDEFS_FUNCTION_ID_BLU
  .appDescriptor :
  {
    __app_desc_addr = .;
    . = ALIGN(4);
    KEEP(*(.appDescriptor))
  } > FLASH
#elif APP_FUNCTION_ID != FDEFS_FUNCTION_ID_BL
#error "Function not supported"
#endif

#if MCU_STM32_PN == FDEFS_STM32_PN_STM32F103XB
  .isr_vector : ALIGN(0x100)
#elif MCU_STM32_PN == FDEFS_STM32_PN_STM32F105
  .isr_vector : ALIGN(0x400)
#else
#error "Chipset not supported"
#endif
  {
    __app_start_addr = .;
    KEEP(*(.isr_vector))    /* interrupt vector table */
  } > FLASH

  /* The program code and other data into "FLASH" Rom type memory */
  .text :
  {
    . = ALIGN(4);
    *(.text .text.* .gnu.linkonce.t.*)        /* .text sections (code) */
    *(.rodata .rodata.* .gnu.linkonce.r.*)    /* .rodata sections (constants, strings, etc.) */
    *(.eh_frame)                              /* https://stackoverflow.com/questions/26300819/why-gcc-compiled-c-program-needs-eh-frame-section */

    . = ALIGN(4);
    KEEP (*(.init))

    . = ALIGN(4);
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array))
    PROVIDE_HIDDEN (__preinit_array_end = .);

    . = ALIGN(4);
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(.init_array))
    KEEP (*(SORT(.init_array.*)))
    PROVIDE_HIDDEN (__init_array_end = .);

    . = ALIGN(4);
    KEEP (*(.fini))

    . = ALIGN(4);
    _etext = .;        /* address of end of text section */
  } > FLASH

  .ARM.extab :
  {
    *(.ARM.extab* .gnu.linkonce.armextab.*)
  } > FLASH

  .ARM.exidx :
  {
    PROVIDE (__exidx_start = .);
    *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    PROVIDE (__exidx_end = .);
  } > FLASH

  /* .ARM.attributes 0 : */
  /* { */
  /*   *(.ARM.attributes) */
  /* } */

  /* Initialized data sections into "RAM" Ram type memory */
  .relocate :
  {
    . = ALIGN(4);
    _sdata = .;               /* create a global symbol at start of section */
    *(.data .data.*)          /* .data sections */
    *(.ramfunc .ramfunc.*)    /* .ramfunc sections */
    . = ALIGN(4);
    _edata = .;               /* define a global symbol at end of section */

  } > RAM AT> FLASH
  /* Used by the startup to initialize data */
  _sidata = LOADADDR(.relocate);
#if APP_FUNCTION_ID == FDEFS_FUNCTION_ID_BL
#if FEATURE_IS_ENABLED(FEATURE_FLASHABLE_BOOTLOADER)
  __appcrc_addr = __BOOT_FLASH_END - 4;
  __appdesc_addr = __appcrc_addr - SIZEOF(.appDescriptor);
#else
  __appdesc_addr = LOADADDR(.relocate) + SIZEOF(.relocate);
  __appcrc_addr = __appdesc_addr + SIZEOF(.appDescriptor);
#endif
  .appDescriptor __appdesc_addr :
  {
    __app_desc_addr = .;
    . = ALIGN(4);
    KEEP(*(.appDescriptor))
  } > FLASH
#elif APP_FUNCTION_ID == FDEFS_FUNCTION_ID_BLU
  __appcrc_addr = LOADADDR(.relocate) + SIZEOF(.relocate);
#else
#error "Function not supported"
#endif
  .crc __appcrc_addr :
  {
    __app_crc_addr = .;
    __app_end_addr = .;
    KEEP(*(.appCrc))
  } > FLASH


  /* Uninitialized data section into "RAM" Ram type memory */
  .bss(NOLOAD) :
  {
    /* This is used by the startup in order to initialize the .bss section */
    . = ALIGN(4);
    _sbss = .;         /* define a global symbol at bss start */
    __bss_start__ = _sbss;
    *(.bss .bss.*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;         /* define a global symbol at bss end */
    __bss_end__ = _ebss;
  } > RAM

  /* Heap and Stack definitions, if needed */
  .heap (COPY) :
  {
    . = ALIGN(8);
    __HeapLimit = .;
    . = . + __HEAP_SIZE;
    . = ALIGN(8);
    __HeapTop = .;
    PROVIDE(end = .);
  } > RAM

  .stack (ORIGIN(RAM) + LENGTH(RAM) - __STACK_SIZE) (COPY) :
  {
    . = ALIGN(8);
    __StackLimit = .;
    . = . + __STACK_SIZE;
    . = ALIGN(8);
    __StackTop = .;
    _estack = .;
    PROVIDE(__stack = __StackTop);
  } > RAM

  ASSERT(__HeapTop < __StackLimit, "RAM overflowed stack")

  /* Remove information from the compiler libraries */
  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
    libc_nano.a ( * )
    libg_nano.a ( * )
  }
}
