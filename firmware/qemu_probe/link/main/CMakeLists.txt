get_filename_component(DOMES_MAIN "${CMAKE_CURRENT_LIST_DIR}/../../../domes/main" ABSOLUTE)

idf_component_register(
    SRCS
        "qemuLinkSlice.cpp"
        "${DOMES_MAIN}/platform/qemu/qemuEspNowRadio.cpp"
        "${DOMES_MAIN}/transport/espNowTransport.cpp"
        "${DOMES_MAIN}/trace/traceBuffer.cpp"
        "${DOMES_MAIN}/trace/traceDumpSnapshot.cpp"
        "${DOMES_MAIN}/trace/kernelTrace.cpp"
        "${DOMES_MAIN}/trace/traceRecorder.cpp"
    INCLUDE_DIRS "${DOMES_MAIN}"
    REQUIRES esp_timer esp_system esp_ringbuf common
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE DOMES_RUNTIME_PROFILE_QEMU=1)
target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_20)
