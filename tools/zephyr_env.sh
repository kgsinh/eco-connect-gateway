# Source from Git Bash (or any bash) before west:  source tools/zephyr_env.sh
#
# Picks the toolchain and the STM32CubeProgrammer CLI. Anything already set
# in the environment wins, so other machines only export their own paths.
#
# Toolchain, first match:
#   1. GNUARMEMB_TOOLCHAIN_PATH set: that Arm GNU toolchain (gnuarmemb).
#   2. ZEPHYR_SDK_INSTALL_DIR set and readable: the Zephyr SDK.
#   3. CubeIDE's arm-none-eabi-gcc under CUBEIDE_PLUGINS (gnuarmemb).
# Programmer: STM32_PROGRAMMER_CLI, else CubeIDE's copy, else the one on PATH.
#
# Defaults are this PC's CubeIDE 1.17.0 install; the plugin folder names
# change when CubeIDE updates. On this PC the Zephyr SDK folder is not
# readable (see NOTES.md), so step 2 is skipped and step 3 applies.

CUBEIDE_PLUGINS="${CUBEIDE_PLUGINS:-C:/ST/STM32CubeIDE_1.17.0/STM32CubeIDE/plugins}"
cubeide_gcc="$CUBEIDE_PLUGINS/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740/tools"
cubeide_cli="$CUBEIDE_PLUGINS/com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer.win32_2.2.500.202603051304/tools/bin/STM32_Programmer_CLI.exe"

if [ -n "${GNUARMEMB_TOOLCHAIN_PATH:-}" ]; then
	export ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb
elif [ -n "${ZEPHYR_SDK_INSTALL_DIR:-}" ] && head -c 1 "$ZEPHYR_SDK_INSTALL_DIR/sdk_version" >/dev/null 2>&1; then
	# A real read: on Windows, Git Bash's [ -r ] does not see ACLs
	export ZEPHYR_TOOLCHAIN_VARIANT=zephyr
elif [ -d "$cubeide_gcc" ]; then
	# A set but unreadable SDK path would make CMake insist on the SDK
	unset ZEPHYR_SDK_INSTALL_DIR
	export ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb
	export GNUARMEMB_TOOLCHAIN_PATH="$cubeide_gcc"
else
	echo "zephyr_env.sh: no toolchain found. Set GNUARMEMB_TOOLCHAIN_PATH (Arm GNU toolchain root)" >&2
	echo "  or ZEPHYR_SDK_INSTALL_DIR (Zephyr SDK), or CUBEIDE_PLUGINS (STM32CubeIDE plugins folder)." >&2
fi

if [ -z "${STM32_PROGRAMMER_CLI:-}" ]; then
	if [ -f "$cubeide_cli" ]; then
		export STM32_PROGRAMMER_CLI="$cubeide_cli"
	elif command -v STM32_Programmer_CLI >/dev/null 2>&1; then
		STM32_PROGRAMMER_CLI="$(command -v STM32_Programmer_CLI)"
		export STM32_PROGRAMMER_CLI
	else
		echo "zephyr_env.sh: STM32_Programmer_CLI not found; set STM32_PROGRAMMER_CLI to flash." >&2
	fi
fi

unset cubeide_gcc cubeide_cli
