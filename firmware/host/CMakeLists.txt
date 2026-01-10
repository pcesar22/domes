cmake_minimum_required(VERSION 3.16)
project(domes-host-tools CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Include shared common library
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../common common)

# Find OpenSSL for SHA256
find_package(OpenSSL REQUIRED)

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

# Simple OTA Sender CLI tool
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

# Install targets
install(TARGETS simple_ota_sender DESTINATION bin)
