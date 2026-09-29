MEMORY
{
  /* No-SoftDevice Cornix UF2 layout. Never link over storage or bootloader. */
  FLASH : ORIGIN = 0x00001000, LENGTH = 0x000D3000
  /* 0xD4000..0xF4000: RMK storage. 0xF4000 onward: bootloader / settings. */
  RAM : ORIGIN = 0x20000008, LENGTH = 0x0003FFF8
}
