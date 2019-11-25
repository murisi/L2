SECTIONS
{
  . = 0x400000;
  .text : { KEEP(*(.text)) *(.text.*) }
  .data : { *(.data) }
  .bss : { *(.bss) }
}

