idf_component_register(
    SRCS "qemuFeasibility.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_gptimer
)

target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_20)
