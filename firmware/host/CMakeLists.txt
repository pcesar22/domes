cmake_minimum_required(VERSION 3.16)
project(domes-host-tools CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Include shared common library
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../common common)

# Find OpenSSL for SHA256
find_package(OpenSSL REQUIRED)

# Find GLib for BLE D-Bus transport
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0 gio-2.0)

# Serial transport library
add_library(domes_host_transport STATIC
    transport/serialTransport.cpp
)

target_include_directories(domes_host_transport PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(domes_host_transport PUBLIC
    domes_common
)

target_compile_options(domes_host_transport PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror=return-type
)

# BLE transport library (uses BlueZ via GLib D-Bus)
add_library(domes_ble_transport STATIC
    transport/bleTransport.cpp
)

target_include_directories(domes_ble_transport PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GLIB_INCLUDE_DIRS}
)

target_link_libraries(domes_ble_transport PUBLIC
    domes_common
    ${GLIB_LIBRARIES}
)

target_compile_options(domes_ble_transport PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror=return-type
)

# Simple OTA Sender CLI tool (Serial)
add_executable(simple_ota_sender
    main/simpleOtaSender.cpp
)

target_link_libraries(simple_ota_sender PRIVATE
    domes_host_transport
    domes_common
    OpenSSL::Crypto
)

target_compile_options(simple_ota_sender PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror=return-type
)

# BLE OTA Sender CLI tool
add_executable(ble_ota_sender
    main/bleOtaSender.cpp
)

target_link_libraries(ble_ota_sender PRIVATE
    domes_ble_transport
    domes_common
    OpenSSL::Crypto
)

target_compile_options(ble_ota_sender PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror=return-type
)

# Install targets
install(TARGETS simple_ota_sender ble_ota_sender DESTINATION bin)
