# This file exists only to give CLion (and other IDEs) accurate code
# insight (header resolution, macros, etc.) for the source tree.
# The actual ROM is built via the project's Makefile (`make modern`),
# not via CMake. The target below is EXCLUDE_FROM_ALL so it is never
# built as part of a normal CMake build.

cmake_minimum_required(VERSION 3.16)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_C_COMPILER /opt/devkitpro/devkitARM/bin/arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER /opt/devkitpro/devkitARM/bin/arm-none-eabi-g++)

project(pokeemerald_ide C CXX)

file(GLOB_RECURSE POKEEMERALD_IDE_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.c" "${CMAKE_SOURCE_DIR}/include/*.h")

add_library(pokeemerald_ide EXCLUDE_FROM_ALL ${POKEEMERALD_IDE_SOURCES})

target_compile_options(pokeemerald_ide PRIVATE
    -iquote ${CMAKE_SOURCE_DIR}/include
    -mthumb
    -mthumb-interwork
    -mabi=apcs-gnu
    -mtune=arm7tdmi
    -march=armv4t
    -DMODERN=1
)

# Delegates the actual build to the project's own Makefile.
add_custom_target(modern ALL
    COMMAND make modern -j$$(nproc)
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
