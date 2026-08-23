#!/usr/bin/env bash

set -Eeuo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly SCRIPT_DIR
# shellcheck source=tools/scoring_validation/fs3_gate_failure.sh
source "$SCRIPT_DIR/fs3_gate_failure.sh"

assert_negative_record() {
    local expected_source="$1"
    local expected_check="$2"
    local output
    local status=0
    local expected_source_quoted

    printf -v expected_source_quoted '%q' "$expected_source"

    output="$(fail_gate 1 73 "$expected_source" "$expected_check" 2>&1)" || status=$?
    [[ "$status" -eq 1 ]]
    [[ "$output" == *'GATE_FAILURE status=1 line=73'* ]]
    [[ "$output" == *"source_or_artifact=$expected_source_quoted"* ]]
    [[ "$output" == *"command_or_check=$expected_check"* ]]
    [[ "$output" != *'GATE_VERDICT=ACCEPTED_SOFTWARE_COMPATIBILITY'* ]]
}

assert_negative_record \
    firmware/common/proto/peer_drill.proto \
    pinned_source_object_equality
assert_negative_record \
    'Cargo OUT_DIR/domes.peer.rs' \
    generated_prost_artifact_present

# Every deliberate fail-closed branch in the runner must use the shared path.
if rg -n '^[[:space:]]*exit 1$' "$SCRIPT_DIR/run_fs3_contract_gate.sh"; then
    printf 'runner contains an unrecorded explicit failure exit\n' >&2
    exit 1
fi
[[ "$(rg -c 'fail_gate 1' "$SCRIPT_DIR/run_fs3_contract_gate.sh")" -eq 2 ]]

printf 'FS3_GATE_NEGATIVE_PATH_TESTS=PASS cases=2\n'
