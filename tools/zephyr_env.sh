# Source from Git Bash before west:  source tools/zephyr_env.sh
#
# Builds with CubeIDE's arm-none-eabi-gcc (gnuarmemb) because the Zephyr SDK
# folder is unreadable for this user (see NOTES.md); the user-level
# ZEPHYR_SDK_INSTALL_DIR points at it and would force CMake to require it.
# Paths change when CubeIDE updates.

CUBEIDE_PLUGINS="C:/ST/STM32CubeIDE_1.17.0/STM32CubeIDE/plugins"

unset ZEPHYR_SDK_INSTALL_DIR
export ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb
export GNUARMEMB_TOOLCHAIN_PATH="$CUBEIDE_PLUGINS/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740/tools"
export STM32_PROGRAMMER_CLI="$CUBEIDE_PLUGINS/com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.win32_2.2.500.202603051304/tools/bin/STM32_Programmer_CLI.exe"
