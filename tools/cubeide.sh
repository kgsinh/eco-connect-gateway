#!/usr/bin/env bash
# Start STM32CubeIDE with the Zephyr build environment, so its "STM32 CMake
# Project" build of this repo uses the same toolchain as `west build`
# (gnuarmemb + CubeIDE GCC, ZEPHYR_SDK_INSTALL_DIR unset; see zephyr_env.sh).
# CubeIDE inherits the environment of the shell that starts it.
#
# Usage (from the repo root, Git Bash):  tools/cubeide.sh
# Close any running CubeIDE first: the new one must not reuse an instance
# started without this environment.

cd "$(dirname "$0")/.." || exit 1
source tools/zephyr_env.sh
command -v west > /dev/null || { echo "west not on PATH" >&2; exit 1; }

# -data: open the workspace that holds this repo (D:/Workspace/STM32H7), no picker
"/c/ST/STM32CubeIDE_1.17.0/STM32CubeIDE/stm32cubeide.exe" -data "$(cd .. && pwd -W)" > /dev/null 2>&1 &
disown
echo "STM32CubeIDE started with the Zephyr environment."
