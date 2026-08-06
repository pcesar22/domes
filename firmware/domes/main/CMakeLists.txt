if(CMAKE_BUILD_EARLY_EXPANSION)
    # ESP-IDF discovers the dependency graph before project Kconfig values are loaded.
    # Register the physical superset here; the normal pass below selects disjoint sources.
    idf_component_register(
        REQUIRES
            driver
            esp_timer
            led_strip
            nvs_flash
            esp_system
            esp_app_format
            esp_wifi
            esp_netif
            esp_http_client
            esp_https_ota
            json
            app_update
            mbedtls
            common
            bt
            mdns
    )
    return()
endif()

set(DOMES_SHARED_SRCS
    "infra/nvsConfig.cpp"
    "infra/diagnostics.cpp"
    "infra/memoryProfiler.cpp"
    "infra/taskStartEvidence.cpp"
    "runtime/runtimeAssembly.cpp"
    "trace/traceBuffer.cpp"
    "trace/traceDumpSnapshot.cpp"
    "trace/kernelTrace.cpp"
    "trace/traceAcceptanceProbe.cpp"
    "trace/traceRecorder.cpp"
    "config/featureManager.cpp"
    "config/modeManager.cpp"
    "game/gameEngine.cpp"
)

if(CONFIG_DOMES_RUNTIME_PROFILE_QEMU AND CONFIG_DOMES_RUNTIME_PROFILE_PHYSICAL)
    message(FATAL_ERROR "DOMES physical and QEMU runtime profiles are mutually exclusive")
elseif(CONFIG_DOMES_RUNTIME_PROFILE_QEMU)
    set(DOMES_PROFILE "qemu")
    set(DOMES_PROFILE_DEFINE "DOMES_RUNTIME_PROFILE_QEMU=1")
    set(DOMES_ROOT_SRCS
        "composition/qemuRoot.cpp"
        "platform/qemu/deterministicPlatformInputs.cpp"
        "platform/qemu/qemuPeripheralAdapters.cpp"
    )
    set(DOMES_PROFILE_REQUIRES
        esp_driver_gptimer
        esp_timer
        nvs_flash
        esp_system
        mbedtls
        common
    )
elseif(CONFIG_DOMES_RUNTIME_PROFILE_PHYSICAL)
    set(DOMES_PROFILE "physical")
    set(DOMES_PROFILE_DEFINE "DOMES_RUNTIME_PROFILE_PHYSICAL=1")
    set(DOMES_ROOT_SRCS
        "main.cpp"
        "infra/watchdog.cpp"
        "infra/taskManager.cpp"
        "infra/crashDumpHandler.cpp"
        "services/wifiManager.cpp"
        "services/firmwareVersion.cpp"
        "services/releaseMetadata.cpp"
        "services/githubClient.cpp"
        "services/otaManager.cpp"
        "transport/uartTransport.cpp"
        "transport/serialOtaReceiver.cpp"
        "transport/bleOtaService.cpp"
        "transport/tcpTransport.cpp"
        "transport/tcpConfigServer.cpp"
        "transport/espNowTransport.cpp"
        "services/espNowService.cpp"
        "platform/physical/espPlatformInputs.cpp"
        "runtime/runtimeEspNowAssembly.cpp"
        "trace/traceCommandHandler.cpp"
        "trace/traceStreamServer.cpp"
        "config/configCommandHandler.cpp"
    )
    set(DOMES_PROFILE_REQUIRES
        driver
        esp_timer
        led_strip
        nvs_flash
        esp_system
        esp_app_format
        esp_wifi
        esp_netif
        esp_http_client
        esp_https_ota
        json
        app_update
        mbedtls
        common
        bt
        mdns
    )
else()
    message(FATAL_ERROR "Exactly one DOMES runtime profile must be selected")
endif()

get_filename_component(DOMES_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(DOMES_PROFILE_SPEC "${CMAKE_CURRENT_LIST_DIR}/../profiles/runtime_profiles.json")
set(DOMES_PROFILE_GENERATOR "${DOMES_REPO_ROOT}/tools/simulation/generate_runtime_profile.py")
set(DOMES_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated/domes")
set(DOMES_GENERATED_HEADER "${DOMES_GENERATED_DIR}/domesRuntimeProfile.generated.hpp")
set(DOMES_FIDELITY_MANIFEST "${CMAKE_BINARY_DIR}/domes-fidelity-manifest.json")
file(MAKE_DIRECTORY "${DOMES_GENERATED_DIR}")

execute_process(
    COMMAND "${PYTHON}" "${DOMES_PROFILE_GENERATOR}"
        --spec "${DOMES_PROFILE_SPEC}"
        --profile "${DOMES_PROFILE}"
        --sdkconfig "${SDKCONFIG}"
        --header "${DOMES_GENERATED_HEADER}"
        --manifest "${DOMES_FIDELITY_MANIFEST}"
    WORKING_DIRECTORY "${DOMES_REPO_ROOT}"
    RESULT_VARIABLE DOMES_PROFILE_RESULT
    OUTPUT_VARIABLE DOMES_PROFILE_OUTPUT
    ERROR_VARIABLE DOMES_PROFILE_ERROR
)
if(NOT DOMES_PROFILE_RESULT EQUAL 0)
    message(FATAL_ERROR
        "DOMES runtime profile generation failed (${DOMES_PROFILE_RESULT})\n"
        "${DOMES_PROFILE_OUTPUT}${DOMES_PROFILE_ERROR}")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${DOMES_PROFILE_SPEC}"
    "${DOMES_PROFILE_GENERATOR}"
)

idf_component_register(
    SRCS
        ${DOMES_SHARED_SRCS}
        ${DOMES_ROOT_SRCS}
    INCLUDE_DIRS
        "."
        "interfaces"
        "drivers"
        "infra"
        "services"
        "utils"
        "transport"
        "trace"
        "config"
        "game"
        "${DOMES_GENERATED_DIR}"
    REQUIRES
        ${DOMES_PROFILE_REQUIRES}
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE ${DOMES_PROFILE_DEFINE})
target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_20)

# ESP-IDF owns FreeRTOSConfig.h, so inject the bounded DOMES hook macros into
# the kernel target without modifying the pinned toolchain checkout.
idf_component_get_property(DOMES_FREERTOS_LIB freertos COMPONENT_LIB)
target_compile_options(${DOMES_FREERTOS_LIB} PRIVATE
    "-include${CMAKE_CURRENT_LIST_DIR}/trace/freertosTraceHooks.h")

if(DOMES_FORCE_OTA_VERIFY_FAILURE)
    if(CONFIG_DOMES_RUNTIME_PROFILE_QEMU)
        message(FATAL_ERROR "DOMES_FORCE_OTA_VERIFY_FAILURE is physical-only")
    endif()
    target_compile_definitions(${COMPONENT_LIB} PRIVATE DOMES_FORCE_OTA_VERIFY_FAILURE=1)
endif()
