# DOMES Unit Test Application
#
# Build and run from the repository root:
#   cmake -S firmware/test_app -B firmware/test_app/build
#   cmake --build firmware/test_app/build
#   ctest --test-dir firmware/test_app/build --output-on-failure

cmake_minimum_required(VERSION 3.16)
project(domes_test_app CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch GoogleTest
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG f8d7d77c06936315286eb55f8de22cd23c188571
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

set(OTA_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../domes/main/services/firmwareVersion.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../domes/main/services/releaseMetadata.cpp
)

# Game module sources (from main firmware)
set(GAME_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../domes/main/game/gameEngine.cpp
)

# Trace module sources (HOST version, NOT firmware traceBuffer.cpp)
set(TRACE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/sim/hostTraceBuffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../domes/main/trace/kernelTrace.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../domes/main/trace/traceDumpSnapshot.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../domes/main/trace/traceCommandHandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../domes/main/trace/traceRecorder.cpp
)

# Test executable
add_executable(test_app
    main/test_main.cpp
    main/test_crc32.cpp
    main/test_frame_codec.cpp
    main/test_ota_protocol.cpp
    main/test_ota_boot_verification.cpp
    main/test_ota_session_coordinator.cpp
    main/test_ota_state_machine.cpp
    main/test_esp_now_protocol.cpp
    main/test_esp_now_transport.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../domes/main/transport/espNowTransport.cpp
    main/test_platform_inputs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../domes/main/platform/qemu/deterministicPlatformInputs.cpp
    main/test_qemu_peripheral_adapters.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../domes/main/platform/qemu/qemuPeripheralAdapters.cpp
    main/test_task_start_evidence.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../domes/main/infra/taskStartEvidence.cpp
    main/test_version_parser.cpp
    main/test_release_metadata.cpp
    main/test_restart_snapshot_record.cpp
    main/test_config_protocol.cpp
    main/test_feedback_controller.cpp
    main/test_feature_manager.cpp
    main/test_mode_manager.cpp
    main/test_game_engine.cpp
    main/test_trace_buffer.cpp
    main/test_trace_command_handler.cpp
    main/test_trace_stream_writer.cpp
    main/test_multi_pod_sim.cpp
    main/test_sim_drill.cpp
    ${CONFIG_SOURCES}
    ${OTA_SOURCES}
    ${GAME_SOURCES}
    ${TRACE_SOURCES}
)

target_include_directories(test_app PRIVATE
    # ESP-IDF stubs must come BEFORE firmware includes to override esp_log.h, esp_timer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
    ${CMAKE_CURRENT_SOURCE_DIR}/../domes/main
    ${CMAKE_CURRENT_SOURCE_DIR}
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
enable_testing()
include(GoogleTest)
gtest_discover_tests(test_app)

# Trace generator (standalone, no gtest)
add_executable(trace_generator
    main/generate_trace.cpp
    ${CONFIG_SOURCES}
    ${GAME_SOURCES}
    ${TRACE_SOURCES}
)

target_include_directories(trace_generator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
    ${CMAKE_CURRENT_SOURCE_DIR}/../domes/main
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(trace_generator PRIVATE domes_common)

target_compile_options(trace_generator PRIVATE
    -Wall -Wextra -Wpedantic -Wno-unused-parameter
)
