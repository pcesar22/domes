#!/usr/bin/env bash

# Emit the complete retained failure record before terminating the gate. Callers
# supply the source line because BASH_LINENO inside a function points at the
# function call only indirectly and is harder to audit in retained logs.
fail_gate() {
    local status="$1"
    local source_line="$2"
    local source_or_artifact="$3"
    local check="$4"

    printf 'GATE_FAILURE status=%s line=%s source_or_artifact=%q command_or_check=%q\n' \
        "$status" "$source_line" "$source_or_artifact" "$check" >&2
    exit "$status"
}
