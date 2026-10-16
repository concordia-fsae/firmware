SECTIONS {
  .appDescriptor ORIGIN(DESCRIPTOR) : {
    LONG(ADDR(.vector_table));
    LONG(__app_end);
    LONG(__app_end);
    SHORT(9); /* FDEFS_COMPONENT_ID_SWS: index 7 + feature enum offset 2 */
    SHORT(0);
  } > DESCRIPTOR
}
__app_end = LOADADDR(.data) + SIZEOF(.data);
ASSERT((__app_end & 3) == 0, "image must be word aligned for STM32 CRC");
ASSERT(__app_end + 4 <= ORIGIN(FLASH) + LENGTH(FLASH), "no room for image CRC");
ASSERT(__euninit + 2048 <= ORIGIN(RAM) + LENGTH(RAM), "reserve at least 2 KiB for stack");
