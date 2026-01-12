# Test app main component
#
# Contains test entry point and all unit test files.

idf_component_register(
    SRCS
        "test_main.cpp"
        "test_crc32.cpp"
        "test_frame_codec.cpp"
        "test_ota_protocol.cpp"
        "test_version_parser.cpp"
    INCLUDE_DIRS
        "."
    PRIV_REQUIRES
        unity
        common
)

# Suppress warnings about unused parameters in test code
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-parameter)
