# CMake toolchain file: Arm GNU toolchain from $GNUARMEMB_TOOLCHAIN_PATH
# (set by tools/zephyr_env.sh: CubeIDE's arm-none-eabi-gcc).
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# No startup files or C library at configure time: test a static library
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(TOOLCHAIN_BIN "$ENV{GNUARMEMB_TOOLCHAIN_PATH}/bin")
find_program(CMAKE_C_COMPILER arm-none-eabi-gcc HINTS "${TOOLCHAIN_BIN}" REQUIRED)
find_program(CMAKE_SIZE arm-none-eabi-size HINTS "${TOOLCHAIN_BIN}" REQUIRED)
