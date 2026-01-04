# DOMES Firmware - Host Test Build
# Standalone CMake for running unit tests on Linux/macOS

cmake_minimum_required(VERSION 3.16)
project(domes-firmware-tests CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compiler warnings
add_compile_options(-Wall -Wextra -Wpedantic)

# Define we're building for host (not ESP32)
add_compile_definitions(HOST_BUILD=1)

# Project root
set(FIRMWARE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# Unity Test Framework
add_library(unity STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../unity/unity.c
)
target_include_directories(unity PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../unity)

# Mock ESP-IDF headers
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../mocks/esp_idf)

# Main component headers
include_directories(
    ${FIRMWARE_ROOT}/main
    ${FIRMWARE_ROOT}/main/config
    ${FIRMWARE_ROOT}/main/platform
    ${FIRMWARE_ROOT}/main/interfaces
    ${FIRMWARE_ROOT}/main/utils
)

# Test executable
add_executable(run_tests
    test_main.cpp
    test_constants.cpp
    test_expected.cpp
    test_color.cpp
)

target_link_libraries(run_tests unity)

# Enable running tests with ctest
enable_testing()
add_test(NAME unit_tests COMMAND run_tests)
