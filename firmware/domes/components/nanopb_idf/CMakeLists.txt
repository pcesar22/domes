# ESP-IDF component wrapper for nanopb submodule
#
# This wraps the nanopb library (in ../nanopb submodule) as an ESP-IDF component.

set(NANOPB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../third_party/nanopb)

idf_component_register(
    SRCS
        "${NANOPB_DIR}/pb_common.c"
        "${NANOPB_DIR}/pb_encode.c"
        "${NANOPB_DIR}/pb_decode.c"
    INCLUDE_DIRS
        "${NANOPB_DIR}"
)
