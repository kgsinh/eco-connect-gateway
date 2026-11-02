/**
 * @file startup.c
 * @brief Vector table and reset handler for the bare-metal blinky.
 *
 * On reset the Cortex-M7 loads SP from word 0 of the vector table at
 * 0x0800 0000 and branches to word 1 (PM0253 2.3.4). Section boundary
 * symbols are defined in stm32h723zg.ld.
 */
#include <stdint.h>

extern uint32_t _sidata;   /**< Load address of .data in flash */
extern uint32_t _sdata;    /**< Start of .data in RAM */
extern uint32_t _edata;    /**< End of .data in RAM */
extern uint32_t _sbss;     /**< Start of .bss */
extern uint32_t _ebss;     /**< End of .bss */
extern uint32_t _estack;   /**< Initial stack pointer: top of DTCM */

extern int main(void);

void Reset_Handler(void);
void Default_Handler(void);

/**
 * @brief Reset entry: initialise .data and .bss, then call main().
 */
void Reset_Handler(void)
{
	const uint32_t *src = &_sidata;
	uint32_t *dst = &_sdata;

	/* Each pointer pair bounds a single linker output section */
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

/**
 * @brief Handler for every unused exception.
 * @note Spins so a debugger can halt and inspect the faulting state.
 */
void Default_Handler(void)
{
	for (;;) {
	}
}

/** @brief Exception handler entry in the vector table. */
typedef void (*vector_t)(void);

/**
 * @brief Cortex-M7 vector table, core exceptions only (PM0253 Table 17).
 *
 * Word 0 is a data address and words 1..15 are code addresses, so they are
 * separate members rather than one array that would need a pointer cast.
 */
struct vector_table {
	const uint32_t *initial_sp;   /**< Word 0: initial main stack pointer */
	vector_t handlers[15];        /**< Words 1..15: exception handlers */
};

__attribute__((section(".isr_vector"), used))
static const struct vector_table vectors = {
	.initial_sp = &_estack,
	.handlers = {
		Reset_Handler,     /* 1  Reset */
		Default_Handler,   /* 2  NMI */
		Default_Handler,   /* 3  HardFault */
		Default_Handler,   /* 4  MemManage */
		Default_Handler,   /* 5  BusFault */
		Default_Handler,   /* 6  UsageFault */
		0,                 /* 7  reserved */
		0,                 /* 8  reserved */
		0,                 /* 9  reserved */
		0,                 /* 10 reserved */
		Default_Handler,   /* 11 SVCall */
		Default_Handler,   /* 12 DebugMonitor */
		0,                 /* 13 reserved */
		Default_Handler,   /* 14 PendSV */
		Default_Handler,   /* 15 SysTick: not used, the tick is polled */
	},
};
