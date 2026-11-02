/*
 * STM32H723ZG: 1 MB flash at 0x0800 0000 (boot address after reset),
 * 128 KB DTCM at 0x2000 0000 for data, bss and stack (RM0468 2.3).
 */
ENTRY(Reset_Handler)

MEMORY
{
	FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 1024K
	DTCM  (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}

_estack = ORIGIN(DTCM) + LENGTH(DTCM);

SECTIONS
{
	.isr_vector :
	{
		KEEP(*(.isr_vector))
	} > FLASH

	.text :
	{
		*(.text*)
		*(.rodata*)
		. = ALIGN(4);
	} > FLASH

	.ARM.exidx :
	{
		*(.ARM.exidx*)
	} > FLASH

	_sidata = LOADADDR(.data);

	.data :
	{
		. = ALIGN(4);
		_sdata = .;
		*(.data*)
		. = ALIGN(4);
		_edata = .;
	} > DTCM AT > FLASH

	.bss (NOLOAD) :
	{
		. = ALIGN(4);
		_sbss = .;
		*(.bss*)
		*(COMMON)
		. = ALIGN(4);
		_ebss = .;
	} > DTCM
}
