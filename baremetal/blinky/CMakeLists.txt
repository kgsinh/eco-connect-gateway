# Bare-metal register blinky: no HAL, no RTOS, no vendor startup code.
# Build: see build.sh.
cmake_minimum_required(VERSION 3.20)
project(baremetal_blinky C)

add_executable(blinky.elf main.c startup.c)

set(CPU_FLAGS -mcpu=cortex-m7 -mthumb -mfloat-abi=soft)

target_compile_options(blinky.elf PRIVATE
	${CPU_FLAGS}
	-std=c11 -O1 -g3
	-Wall -Wextra -Wpedantic
	-ffreestanding -fno-builtin
	# keep the copy/zero loops in startup.c from becoming memcpy/memset calls
	-fno-tree-loop-distribute-patterns
	-ffunction-sections -fdata-sections
)

target_link_options(blinky.elf PRIVATE
	${CPU_FLAGS}
	-nostdlib
	-T ${CMAKE_CURRENT_SOURCE_DIR}/stm32h723zg.ld
	-Wl,--gc-sections
	-Wl,-Map=blinky.map
	-Wl,--print-memory-usage
)

target_link_libraries(blinky.elf PRIVATE gcc)

add_custom_command(TARGET blinky.elf POST_BUILD
	COMMAND ${CMAKE_SIZE} blinky.elf
)
