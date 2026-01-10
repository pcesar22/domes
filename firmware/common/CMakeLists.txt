# Dual-mode CMakeLists.txt for DOMES common library
#
# Works as:
# 1. ESP-IDF component (when IDF_TARGET is defined)
# 2. Standard CMake library (for host tools)

# Collect source files (only .cpp files, headers are header-only)
set(COMMON_SRCS
    protocol/otaProtocol.cpp
)

# Include directories
set(COMMON_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/interfaces
    ${CMAKE_CURRENT_SOURCE_DIR}/protocol
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
)

if(IDF_TARGET)
    # ESP-IDF component mode
    # The parent project includes this via EXTRA_COMPONENT_DIRS
    idf_component_register(
        SRCS ${COMMON_SRCS}
        INCLUDE_DIRS ${COMMON_INCLUDE_DIRS}
    )
else()
    # Standard CMake library mode (for host tools)
    cmake_minimum_required(VERSION 3.16)

    # Only define project if we're the top-level
    if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
        project(domes_common CXX)
        set(CMAKE_CXX_STANDARD 20)
        set(CMAKE_CXX_STANDARD_REQUIRED ON)
    endif()

    add_library(domes_common STATIC ${COMMON_SRCS})

    target_include_directories(domes_common PUBLIC ${COMMON_INCLUDE_DIRS})

    target_compile_features(domes_common PUBLIC cxx_std_20)

    # Enable warnings
    target_compile_options(domes_common PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror=return-type
    )
endif()
