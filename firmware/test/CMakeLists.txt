# DOMES Firmware - Test Component CMakeLists.txt
# Unit tests run on Linux host target

idf_component_register(
    SRCS
        "test_main.cpp"
    INCLUDE_DIRS
        "."
        "mocks"
    REQUIRES
        unity
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
