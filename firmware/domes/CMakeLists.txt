cmake_minimum_required(VERSION 3.16)

set(DOMES_FORCE_OTA_VERIFY_FAILURE OFF CACHE BOOL
    "Force pending OTA verification to fail for destructive rollback testing")

include(cmake/ResolveFirmwareVersion.cmake)
domes_resolve_firmware_version(
    GIT_VERSION
    "${CMAKE_SOURCE_DIR}"
    "${DOMES_VERSION_OVERRIDE}"
)

# Reconfigure incremental builds when the checked-out commit changes. Without
# this dependency, PROJECT_VER remains whatever commit first created build/.
execute_process(
    COMMAND git rev-parse --git-path HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_HEAD_PATH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(GIT_HEAD_PATH)
    get_filename_component(GIT_HEAD_PATH "${GIT_HEAD_PATH}" ABSOLUTE
                           BASE_DIR "${CMAKE_SOURCE_DIR}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${GIT_HEAD_PATH}")
endif()

execute_process(
    COMMAND git symbolic-ref -q HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_HEAD_REF
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(GIT_HEAD_REF)
    execute_process(
        COMMAND git rev-parse --git-path ${GIT_HEAD_REF}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_HEAD_REF_PATH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(GIT_HEAD_REF_PATH)
        get_filename_component(GIT_HEAD_REF_PATH "${GIT_HEAD_REF_PATH}" ABSOLUTE
                               BASE_DIR "${CMAKE_SOURCE_DIR}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${GIT_HEAD_REF_PATH}")
    endif()
endif()

# Set project version for ESP-IDF
set(PROJECT_VER "${GIT_VERSION}")

message(STATUS "DOMES firmware version: ${GIT_VERSION}")

# Add components directories
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/components"
    "${CMAKE_CURRENT_SOURCE_DIR}/../common"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(domes)
