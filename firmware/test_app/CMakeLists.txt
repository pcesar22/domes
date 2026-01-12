# DOMES Unit Test Application (Standalone CMake build)
#
# Build and run:
#   cd firmware/test_app
#   mkdir -p build && cd build
#   cmake ..
#   make
#   ./test_app

cmake_minimum_required(VERSION 3.16)
project(domes_test_app CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add common library (uses its non-IDF CMake mode)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../common ${CMAKE_BINARY_DIR}/common)

# Unity test framework
add_library(unity STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/unity/unity.c
)
target_include_directories(unity PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/unity)

# Test executable
add_executable(test_app
    main/test_main_standalone.cpp
    main/test_crc32.cpp
    main/test_frame_codec.cpp
    main/test_ota_protocol.cpp
    main/test_version_parser.cpp
)

target_link_libraries(test_app PRIVATE domes_common unity)

target_compile_options(test_app PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wno-unused-parameter
)
