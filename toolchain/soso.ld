
ENTRY(_start)

SECTIONS
{
  /* ELF header and program headers take space, skip a page for safety */
  . = 0x40000000 + 0x1000;

  .text : ALIGN(0x1000) {
    *(.text.entry)
    *(.text*)
    . = ALIGN(0x1000);
  }

  .rodata : ALIGN(0x1000) {
    *(.rodata*)
    . = ALIGN(0x1000);
  }

  .data : ALIGN(0x1000) {
    *(.data*)
    . = ALIGN(0x1000);
  }

  .bss : ALIGN(0x1000) {
    *(.bss*)
    *(COMMON)
    . = ALIGN(0x1000);
  }

  /DISCARD/ : {
    *(.eh_frame*)
    *(.note*)
    *(.comment)
  }
}
