cmake_minimum_required(VERSION 3.16)

# Get version from git describe
execute_process(
    COMMAND git describe --tags --always --dirty
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE GIT_RESULT
)

if(NOT GIT_RESULT EQUAL 0 OR NOT GIT_VERSION)
    set(GIT_VERSION "v0.0.0-unknown")
endif()

# Parse version components
string(REGEX MATCH "^v?([0-9]+)\\.([0-9]+)\\.([0-9]+)" VERSION_MATCH "${GIT_VERSION}")
if(VERSION_MATCH)
    set(VERSION_MAJOR ${CMAKE_MATCH_1})
    set(VERSION_MINOR ${CMAKE_MATCH_2})
    set(VERSION_PATCH ${CMAKE_MATCH_3})
else()
    set(VERSION_MAJOR 0)
    set(VERSION_MINOR 0)
    set(VERSION_PATCH 0)
endif()

# Set project version for ESP-IDF
set(PROJECT_VER "${GIT_VERSION}")

message(STATUS "DOMES firmware version: ${GIT_VERSION}")

set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(domes)

# Make version available to code via compile definitions
add_compile_definitions(
    DOMES_VERSION_MAJOR=${VERSION_MAJOR}
    DOMES_VERSION_MINOR=${VERSION_MINOR}
    DOMES_VERSION_PATCH=${VERSION_PATCH}
    DOMES_VERSION_STRING="${GIT_VERSION}"
)
