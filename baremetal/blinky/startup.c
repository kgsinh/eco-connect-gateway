/*
 * What runs before main(): the Cortex-M7 reads the initial stack pointer
 * from word 0 of the vector table at 0x0800 0000 and jumps to word 1,
 * Reset_Handler (PM0253 2.3.4). Reset_Handler copies initialised data from
 * flash to RAM, zeroes .bss and calls main(). Section symbols come from
 * stm32h723zg.ld.
 */
#include <stdint.h>

extern uint32_t _sidata;   /* .data load address in flash */
extern uint32_t _sdata;    /* .data start in RAM */
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;
extern uint32_t _estack;   /* top of DTCM */

extern int main(void);

void Reset_Handler(void);
void Default_Handler(void);

void Reset_Handler(void)
{
	const uint32_t *src = &_sidata;
	uint32_t *dst = &_sdata;

	/* _sdata.._edata and _sbss.._ebss are bounds of one linker section each */
	while (dst < &_edata) {
		*dst = *src;
		dst++;
		src++;
	}

	for (dst = &_sbss; dst < &_ebss; dst++) {
		*dst = 0U;
	}

	(void)main();

	for (;;) {
	}
}

/* Any unexpected exception (HardFault etc.) parks here: halt to inspect */
void Default_Handler(void)
{
	for (;;) {
	}
}

/*
 * Core exceptions only (PM0253 Table 17): no peripheral interrupt is used.
 * Word 0 is the stack pointer value, words 1..15 are handler addresses; a
 * struct keeps both types without casting between data and code pointers.
 */
typedef void (*vector_t)(void);

struct vector_table {
	const uint32_t *initial_sp;
	vector_t handlers[15];
};

__attribute__((section(".isr_vector"), used))
static const struct vector_table vectors = {
	.initial_sp = &_estack,   /* 0  initial SP */
	.handlers = {
		Reset_Handler,        /* 1  Reset */
		Default_Handler,      /* 2  NMI */
		Default_Handler,      /* 3  HardFault */
		Default_Handler,      /* 4  MemManage */
		Default_Handler,      /* 5  BusFault */
		Default_Handler,      /* 6  UsageFault */
		0,                    /* 7  reserved */
		0,                    /* 8  reserved */
		0,                    /* 9  reserved */
		0,                    /* 10 reserved */
		Default_Handler,      /* 11 SVCall */
		Default_Handler,      /* 12 DebugMonitor */
		0,                    /* 13 reserved */
		Default_Handler,      /* 14 PendSV */
		Default_Handler,      /* 15 SysTick (unused: polled, TICKINT = 0) */
	},
};
