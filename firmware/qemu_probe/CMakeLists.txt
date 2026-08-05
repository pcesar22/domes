cmake_minimum_required(VERSION 3.16)

set(IDF_TARGET "esp32s3" CACHE STRING "QEMU feasibility target")
set(COMPONENTS main)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(domes_qemu_probe)
