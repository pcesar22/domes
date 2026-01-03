# DOMES Firmware - Root CMakeLists.txt
# ESP-IDF v5.x project configuration

cmake_minimum_required(VERSION 3.16)

# Set default components
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/components"
)

# Include ESP-IDF project
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Project definition
project(domes-firmware)
