function(domes_resolve_firmware_version output_var source_dir override)
    set(version_pattern
        "^[vV]?[0-9]+\\.[0-9]+\\.[0-9]+(-dirty|-[0-9]+-g[0-9A-Fa-f]+(-dirty)?)?$"
    )

    if(NOT "${override}" STREQUAL "")
        set(resolved_version "${override}")
        set(version_source "DOMES firmware version override")
    else()
        execute_process(
            COMMAND git describe --tags --always --dirty --match "v[0-9]*.[0-9]*.[0-9]*"
            WORKING_DIRECTORY "${source_dir}"
            OUTPUT_VARIABLE described_version
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            RESULT_VARIABLE describe_result
        )
        if(describe_result EQUAL 0 AND described_version MATCHES "${version_pattern}")
            set(resolved_version "${described_version}")
            set(version_source "git-derived firmware version")
        else()
            execute_process(
                COMMAND git rev-parse --short=12 HEAD
                WORKING_DIRECTORY "${source_dir}"
                OUTPUT_VARIABLE commit_hash
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
                RESULT_VARIABLE hash_result
            )
            if(NOT hash_result EQUAL 0 OR NOT commit_hash MATCHES "^[0-9A-Fa-f]+$")
                set(resolved_version "v0.0.0-dirty")
            else()
                execute_process(
                    COMMAND git status --porcelain --untracked-files=no
                    WORKING_DIRECTORY "${source_dir}"
                    OUTPUT_VARIABLE tracked_changes
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    ERROR_QUIET
                )
                set(resolved_version "v0.0.0-0-g${commit_hash}")
                if(NOT tracked_changes STREQUAL "")
                    string(APPEND resolved_version "-dirty")
                endif()
            endif()
            set(version_source "fallback firmware version")
        endif()
    endif()

    if("${resolved_version}" STREQUAL "" OR
       NOT "${resolved_version}" MATCHES "${version_pattern}")
        message(FATAL_ERROR "Invalid ${version_source}: ${resolved_version}")
    endif()

    # esp_app_desc_t reserves 32 bytes including the terminating NUL. Enforce
    # that limit before ESP-IDF can silently truncate the embedded version.
    string(LENGTH "${resolved_version}" version_length)
    if(version_length GREATER 31)
        message(FATAL_ERROR
            "Invalid ${version_source}: '${resolved_version}' is ${version_length} bytes; "
            "the ESP app descriptor limit is 31 bytes"
        )
    endif()

    set(${output_var} "${resolved_version}" PARENT_SCOPE)
endfunction()
