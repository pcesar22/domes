cmake_minimum_required(VERSION 3.16)

# Add components directory if it exists
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/components")
    set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/components")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(domes)
