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

__RAM_SIZE = 20K;
__FLASH_SIZE = 58K;

__RAM_ORIGIN = 0x20000000;     /* don't change */
__FLASH_ORIGIN = 0x08002000;    /* don't change */


/* Memory definition */
MEMORY
{
    RAM      (rwx)    : ORIGIN = __RAM_ORIGIN,       LENGTH = __RAM_SIZE
    FLASH    (rx)     : ORIGIN = __FLASH_ORIGIN,     LENGTH = __FLASH_SIZE
}

SECTIONS
{
  /* app descriptor must be at the origin so that the bootloader can find it */
  .appDescriptor __FLASH_ORIGIN :
  {
    . = ALIGN(4);
    KEEP(*(.appDescriptor))
  } > FLASH

  /* The program code and other data into "FLASH" Rom type memory */
  /* vector table must be aligned on */
  .isr_vector : ALIGN(0x100)
  {
    __app_start_addr = .;
    KEEP(*(.isr_vector))                      /* interrupt vector table */
  } > FLASH

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

  __app_end_addr = LOADADDR(.relocate) + SIZEOF(.relocate);
  /* CRC goes at the very end of the app */
  .crc __app_end_addr :
  {
    __app_crc_addr = .;
    KEEP(*(.appCrc))                 /* app description location */
  } > FLASH

  /* Used by the startup to initialize data */
  _sidata = LOADADDR(.crc);

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
  } > RAM

  .stack (ORIGIN(RAM) + LENGTH(RAM) - __STACK_SIZE) (COPY) :
  {
    . = ALIGN(8);
    __StackLimit = .;
    . = . + __STACK_SIZE;
    . = ALIGN(8);
    __StackTop = .;
    _estack = .;
  } > RAM

  ASSERT(__HeapTop < __StackLimit, "RAM overflowed stack")

  /* Remove information from the compiler libraries */
  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
  }

  .ARM.attributes 0 :
  {
    *(.ARM.attributes)
  }
}
