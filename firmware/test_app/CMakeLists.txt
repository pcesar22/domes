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

# Config module sources (from main firmware)
set(CONFIG_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../domes/main/config/featureManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../domes/main/config/modeManager.cpp
)

# Game module sources (from main firmware)
set(GAME_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../domes/main/game/gameEngine.cpp
)

# Test executable
add_executable(test_app
    main/test_main.cpp
    main/test_crc32.cpp
    main/test_frame_codec.cpp
    main/test_ota_protocol.cpp
    main/test_version_parser.cpp
    main/test_config_protocol.cpp
    main/test_feature_manager.cpp
    main/test_mode_manager.cpp
    main/test_game_engine.cpp
    ${CONFIG_SOURCES}
    ${GAME_SOURCES}
)

target_include_directories(test_app PRIVATE
    # ESP-IDF stubs must come BEFORE firmware includes to override esp_log.h, esp_timer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
    ${CMAKE_CURRENT_SOURCE_DIR}/../domes/main
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
