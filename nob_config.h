#ifndef NOB_CONFIG_H
#define NOB_CONFIG_H

// Configuration for the Nob build tool

// TODO add compiler choice
// #define COMPILER "gcc"
// #define COMPILER "clang"
// #define COMPILER "arm-none-eabi-gcc"

#define BUILD_DIR "build"
#define SRC "src"
#define INC "include"
#define TEST_DIR "tests"
#define EXAMPLE_DIR "examples"

// Handle platform specifics
// #define PLATFORM_LINUX
// #define PLATFORM_WINDOWS
// #define PLATFORM_MACOS
// #define PLATFORM_EMBEDDED

// #ifdef PLATFORM_LINUX
// #define PLATFORM_LIBS "-lrt", "-lpthread"
// #endif

#endif // NOB_CONFIG_H