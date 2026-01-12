# DOMES Unit Test Application
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

# Fetch GoogleTest
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
# Prevent GoogleTest from overriding compiler/linker options
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Add common library (uses its non-IDF CMake mode)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../common ${CMAKE_BINARY_DIR}/common)

# Test executable
add_executable(test_app
    main/test_main.cpp
    main/test_crc32.cpp
    main/test_frame_codec.cpp
    main/test_ota_protocol.cpp
    main/test_version_parser.cpp
)

target_link_libraries(test_app PRIVATE
    domes_common
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
)

target_compile_options(test_app PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wno-unused-parameter
)

# Enable CTest integration
include(GoogleTest)
gtest_discover_tests(test_app)
