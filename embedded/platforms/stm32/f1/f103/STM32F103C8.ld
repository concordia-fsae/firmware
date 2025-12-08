/**
 * STM32F103C8.ld
 * Linker script for STM32F103C8
 */

/* Entry Point */
ENTRY(Reset_Handler)

/* Chip-specific constants */
#define FLASH_ORIGIN 0x08000000
#define FLASH_SIZE   64K
#define RAM_ORIGIN   0x20000000
#define RAM_SIZE     20K
/* end chip specific constants */


/* Highest address of the user mode stack */
_estack = ORIGIN(RAM) + LENGTH(RAM);    /* bottom of stack (grows down) */

/* stack and heap definitions */
#if defined(LD_USE_STACK)
__STACK_SIZE = 0x400;
#else
__STACK_SIZE = 0;
#endif

#if defined(LD_USE_HEAP)
__HEAP_SIZE = 0x200;
#endif

#ifndef NVM_LIB_ENABLED
NVM_LIB_ENABLED = 0;
NVM_BLOCK_SIZE = 0;
NVM_SIZE = 0;
#endif

#if NVM_LIB_ENABLED
NVM_SIZE = (2*NVM_BLOCK_SIZE);
#else
NVM_SIZE = 0;
#endif

#if NVM_LIB_ENABLED
__FLASH_NVM_ORIGIN = FLASH_ORIGIN + FLASH_SIZE - NVM_SIZE;
__FLASH_NVM_END = __FLASH_NVM_ORIGIN + NVM_SIZE;
#endif

/* determine app start address and size */
#if defined(APP_START_ADDR)
# if APP_START_ADDR < FLASH_ORIGIN
#  error "App flash cannot start earlier than the flash origin address"
# endif
__APP_SIZE = FLASH_SIZE - (APP_START_ADDR - FLASH_ORIGIN) - NVM_SIZE;
#else
  __APP_SIZE = FLASH_SIZE;
# define APP_START_ADDR  FLASH_ORIGIN
#endif

/* Memory definition */
MEMORY
{
    RAM         (rwx)    : ORIGIN = RAM_ORIGIN,                          LENGTH = RAM_SIZE
    FLASH       (rx)     : ORIGIN = APP_START_ADDR,                      LENGTH = __APP_SIZE
#if NVM_LIB_ENABLED
    NVM_BLOCK   (rw)     : ORIGIN = __FLASH_NVM_ORIGIN,                  LENGTH = NVM_BLOCK_SIZE
#endif
}

SECTIONS
{
/*
 * if the app is starting at the origin, the vector table must come before the
 * app descriptor
 */
#if APP_START_ADDR != FLASH_ORIGIN
  /*
   * for an app, the app descriptor must be at the origin so that the bootloader can find it.
   */
  .appDescriptor :
  {
    . = ALIGN(4);
    KEEP(*(.appDescriptor))
  } > FLASH
#endif

  /* The interrupt vector table must be aligned according to the number of interrupts in the table,
   * with a minimum alignment of 32 words (128 bytes) to support the built-in arm interrupts plus
   * an additional 16 interrupts.
   * Alignment is effectively 16 plus the number of interrupts in the table, rounded up to the next power
   * of 2. E.g. for 17 to 64 interrupts, the table must be aligned to 64 words (256 bytes)
   * https://developer.arm.com/documentation/dui0552/a/cortex-m3-peripherals/system-control-block/vector-table-offset-register
   */
  .isr_vector : ALIGN(0x100)
  {
    __app_start_addr = .;
    KEEP(*(.isr_vector))    /* interrupt vector table */
  } > FLASH

/*
* If the app starts at the flash origin, the app descriptor must come after
* the vector table
*/
#if APP_START_ADDR == FLASH_ORIGIN
  .appDescriptor :
  {
    . = ALIGN(4);
    KEEP(*(.appDescriptor))
  } > FLASH
#endif


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
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(.fini_array))
    KEEP (*(SORT(.fini_array.*)))
    PROVIDE_HIDDEN (__fini_array_end = .);

    . = ALIGN(4);
    KEEP (*crtbegin.o(.ctors))
    KEEP (*(EXCLUDE_FILE (*crtend.o) .ctors))
    KEEP (*(SORT(.ctors.*)))
    KEEP (*crtend.o(.ctors))

    . = ALIGN(4);
    KEEP (*crtbegin.o(.dtors))
    KEEP (*(EXCLUDE_FILE (*crtend.o) .dtors))
    KEEP (*(SORT(.dtors.*)))
    KEEP (*crtend.o(.dtors))

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

#if NVM_LIB_ENABLED
  .nvm(NOLOAD) :
  {
    . = ALIGN(4);
    KEEP(*(.nvm))
    . = ALIGN(4);
  } > NVM_BLOCK
  ASSERT(SIZEOF(".nvm") < NVM_BLOCK_SIZE / 2, "NVM size should be more than twice the size of all members.")
#endif
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

  /* CRC needs to be at the end of the _image_, which should be right after the
   * "relocate" section
   */
  __app_end_addr = LOADADDR(.relocate) + SIZEOF(.relocate);

  /* CRC goes at the very end of the app */
  .crc __app_end_addr :
  {
    . = ALIGN(4);
    __app_crc_addr = .;
    KEEP(*(.appCrc))
    . = ALIGN(4);
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
#if defined(LD_USE_HEAP)
  .heap (COPY) :
  {
    . = ALIGN(8);
    __HeapLimit = .;
    . = . + __HEAP_SIZE;
    . = ALIGN(8);
    __HeapTop = .;
    PROVIDE(end = .);
  } > RAM
#endif

#if defined(LD_USE_HEAP)
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
#endif

#if defined(LD_USE_HEAP) && defined(LD_USE_STACK)
  ASSERT(__HeapTop < __StackLimit, "RAM overflowed stack")
#endif


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
