# DOMES Firmware - Main Component CMakeLists.txt

idf_component_register(
    SRCS
        "main.cpp"
    INCLUDE_DIRS
        "."
        "config"
        "platform"
        "interfaces"
        "drivers"
        "services"
        "game"
        "utils"
    REQUIRES
        driver
        esp_timer
        esp_wifi
        esp_event
        nvs_flash
        esp_adc
        esp_netif
        bt
        freertos
)

# Enable C++20
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)

# Compiler warnings
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall
    -Wextra
    -Werror
    -Wno-missing-field-initializers
)
