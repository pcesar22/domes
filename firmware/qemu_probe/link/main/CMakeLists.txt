get_filename_component(DOMES_MAIN "${CMAKE_CURRENT_LIST_DIR}/../../../domes/main" ABSOLUTE)
get_filename_component(DOMES_ROOT "${DOMES_MAIN}/../../.." ABSOLUTE)
if(CMAKE_BUILD_EARLY_EXPANSION)
    idf_component_register(REQUIRES esp_timer esp_system esp_ringbuf esp_wifi nvs_flash common)
    return()
endif()
set(DOMES_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated/domes")
file(MAKE_DIRECTORY "${DOMES_GENERATED_DIR}")
execute_process(
    COMMAND "${PYTHON}" "${DOMES_ROOT}/tools/simulation/generate_runtime_profile.py"
        --spec "${DOMES_MAIN}/../profiles/runtime_profiles.json"
        --profile qemu
        --sdkconfig "${SDKCONFIG}"
        --header "${DOMES_GENERATED_DIR}/domesRuntimeProfile.generated.hpp"
        --manifest "${CMAKE_BINARY_DIR}/domes-fidelity-manifest.json"
    RESULT_VARIABLE DOMES_PROFILE_RESULT
)
if(NOT DOMES_PROFILE_RESULT EQUAL 0)
    message(FATAL_ERROR "QEMU link runtime profile generation failed")
endif()

idf_component_register(
    SRCS
        "qemuLinkSlice.cpp"
        "${DOMES_MAIN}/config/featureManager.cpp"
        "${DOMES_MAIN}/config/modeManager.cpp"
        "${DOMES_MAIN}/game/gameEngine.cpp"
        "${DOMES_MAIN}/infra/nvsConfig.cpp"
        "${DOMES_MAIN}/platform/qemu/deterministicPlatformInputs.cpp"
        "${DOMES_MAIN}/platform/qemu/qemuEspNowRadio.cpp"
        "${DOMES_MAIN}/services/espNowService.cpp"
        "${DOMES_MAIN}/transport/espNowTransport.cpp"
        "${DOMES_MAIN}/trace/traceBuffer.cpp"
        "${DOMES_MAIN}/trace/traceDumpSnapshot.cpp"
        "${DOMES_MAIN}/trace/kernelTrace.cpp"
        "${DOMES_MAIN}/trace/traceRecorder.cpp"
    INCLUDE_DIRS "${DOMES_MAIN}" "${DOMES_GENERATED_DIR}"
    REQUIRES esp_timer esp_system esp_ringbuf esp_wifi nvs_flash common
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE DOMES_RUNTIME_PROFILE_QEMU=1)
target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_20)
