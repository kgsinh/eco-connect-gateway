#!/usr/bin/env bash
# Build (and optionally flash) the bare-metal blinky. From any directory:
#   baremetal/blinky/build.sh          # build -> baremetal/blinky/build/blinky.elf
#   baremetal/blinky/build.sh flash    # build, then flash over ST-Link
# Toolchain and programmer paths come from tools/zephyr_env.sh.
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"

if [ -z "${GNUARMEMB_TOOLCHAIN_PATH:-}" ] || [ -z "${STM32_PROGRAMMER_CLI:-}" ]; then
	# shellcheck source=../../tools/zephyr_env.sh
	source "$here/../../tools/zephyr_env.sh"
fi

cmake -S "$here" -B "$here/build" -G Ninja \
	-DCMAKE_TOOLCHAIN_FILE="$here/arm-none-eabi.cmake" \
	-DCMAKE_BUILD_TYPE=
cmake --build "$here/build"

if [ "${1:-}" = "flash" ]; then
	# mode=UR: connect under reset (a running Zephyr image sleeps in WFI)
	"$STM32_PROGRAMMER_CLI" -c port=SWD mode=UR -w "$here/build/blinky.elf" -v -rst
fi
