#!/usr/bin/env bash

set -Eeuo pipefail

readonly SPEC_REVISION="6f197670a49bc8b83753d1dfab0dd1f789b5f4db"
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
readonly REPO_ROOT
# shellcheck source=tools/scoring_validation/fs3_gate_failure.sh
source "$REPO_ROOT/tools/scoring_validation/fs3_gate_failure.sh"
RUN_ROOT="$(mktemp -d)"
readonly RUN_ROOT
readonly FIRMWARE_BUILD="$RUN_ROOT/firmware-test-app"
readonly CARGO_TARGET="$RUN_ROOT/cargo-target"
if [[ -n "${DOMES_FLUTTER_ROOT:-}" ]]; then
    readonly FLUTTER_ROOT="$DOMES_FLUTTER_ROOT"
else
    FLUTTER_ROOT="$(cd "$(dirname "$(command -v flutter)")/.." && pwd)"
    readonly FLUTTER_ROOT
fi
readonly PUB_CACHE_ROOT="${DOMES_PUB_CACHE:-${PUB_CACHE:-$HOME/.pub-cache}}"
readonly DART_BIN="$FLUTTER_ROOT/bin/cache/dart-sdk/bin/dart"
readonly FLUTTER_BIN="$FLUTTER_ROOT/bin/flutter"
export PATH="$FLUTTER_ROOT/bin:$PUB_CACHE_ROOT/bin:$PATH"
export CI=true
export DART_SUPPRESS_ANALYTICS=true
export PUB_CACHE="$PUB_CACHE_ROOT"

cleanup() {
    rm -rf "$RUN_ROOT"
}
trap cleanup EXIT
gate_status=0
trap 'gate_status=$?; printf "GATE_FAILURE status=%s line=%s command=%q\n" "$gate_status" "$LINENO" "$BASH_COMMAND" >&2; exit "$gate_status"' ERR

cd "$REPO_ROOT"

run() {
    printf '\n$'
    printf ' %q' "$@"
    printf '\n'
    "$@"
    printf 'EXIT_STATUS=0\n'
}

readonly -a PINNED_PATHS=(
    firmware/common/proto/peer_drill.proto
    firmware/common/proto/peer_drill.options
    firmware/common/proto/peer_drill.pb.c
    firmware/common/proto/peer_drill.pb.h
    firmware/domes/main/services/espNowProtocol.hpp
    firmware/domes/main/services/espNowService.cpp
    firmware/domes/main/services/espNowService.hpp
    firmware/domes/main/services/roundTokenSequence.hpp
    firmware/test_app/main/test_esp_now_protocol.cpp
    firmware/test_app/main/test_multi_pod_sim.cpp
    firmware/test_app/main/test_platform_inputs.cpp
    firmware/test_app/main/test_sim_drill.cpp
    firmware/test_app/sim/drillOrchestrator.hpp
    ios/domes_app/lib/application/providers/drill_provider.dart
    ios/domes_app/lib/data/proto/generated/peer_drill.pb.dart
    ios/domes_app/lib/data/proto/generated/peer_drill.pbenum.dart
    ios/domes_app/lib/data/proto/generated/peer_drill.pbjson.dart
    ios/domes_app/lib/data/protocol/peer_contract.dart
    ios/domes_app/lib/domain/models/drill_result.dart
    ios/domes_app/test/data/protocol/peer_contract_test.dart
    ios/domes_app/test/domain/models/drill_result_test.dart
    tools/domes-cli/build.rs
    tools/domes-cli/src/proto.rs
    tools/domes-cli/src/protocol/peer_contract.rs
    tools/scoring_validation/campaign.py
    tools/scoring_validation/fixtures/fixed_two_pod_v1.json
    tools/scoring_validation/test_campaign.py
    tools/scoring_validation/artifacts/verdict.json
)

printf 'FS3_CONTRACT_COMPATIBILITY_GATE\n'
printf 'SPEC_REVISION=%s\n' "$SPEC_REVISION"
printf 'EXECUTED_HEAD=%s\n' "$(git rev-parse HEAD)"
printf 'UTC_STARTED=%s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)"

run git cat-file -e "$SPEC_REVISION^{commit}"
run git merge-base --is-ancestor "$SPEC_REVISION" HEAD

printf '\nPINNED_SOURCE_OBJECTS\n'
for path in "${PINNED_PATHS[@]}"; do
    pinned_object="$(git rev-parse "$SPEC_REVISION:$path")"
    current_object="$(git hash-object "$path")"
    printf '%s pinned=%s current=%s\n' "$path" "$pinned_object" "$current_object"
    if [[ "$pinned_object" != "$current_object" ]]; then
        printf 'PINNED_SOURCE_DRIFT path=%s pinned=%s current=%s\n' \
            "$path" "$pinned_object" "$current_object" >&2
        fail_gate 1 "$LINENO" "$path" pinned_source_object_equality
    fi
done

printf '\nTOOL_VERSIONS\n'
run git --version
run python3 --version
run cmake --version
run c++ --version
run protoc --version
run cargo --version
run rustc --version
run "$DART_BIN" --version
run env CI=true DART_SUPPRESS_ANALYTICS=true "$FLUTTER_BIN" --suppress-analytics --version

run tools/generate_protocols.sh --check all

run cmake -S firmware/test_app -B "$FIRMWARE_BUILD" -DCMAKE_BUILD_TYPE=Release
run cmake --build "$FIRMWARE_BUILD" --parallel
run "$FIRMWARE_BUILD/test_app" \
    '--gtest_filter=EspNowProtocolTest.*:PlatformInputsTest.RoundTokenSequencePreservesSeedPlusOneAndSkipsZero:MultiPodSimTest.*:SimDrillTest.*'

run env CARGO_TARGET_DIR="$CARGO_TARGET" cargo test \
    --manifest-path tools/domes-cli/Cargo.toml --locked --bin domes-cli \
    protocol::peer_contract::tests
generated_prost="$(find "$CARGO_TARGET" -type f -path '*/out/domes.peer.rs' -print -quit)"
if [[ -z "$generated_prost" ]]; then
    printf 'GENERATED_PROST_MISSING target=%s\n' "$CARGO_TARGET" >&2
    fail_gate 1 "$LINENO" 'Cargo OUT_DIR/domes.peer.rs' generated_prost_artifact_present
fi
printf 'GENERATED_PROST sha256=%s artifact=%s\n' \
    "$(sha256sum "$generated_prost" | cut -d' ' -f1)" \
    'Cargo OUT_DIR/domes.peer.rs (ephemeral build artifact)'

run env CI=true DART_SUPPRESS_ANALYTICS=true "$FLUTTER_BIN" --suppress-analytics \
    pub get --directory ios/domes_app --enforce-lockfile
run bash -c 'cd ios/domes_app && exec "$@"' _ \
    env CI=true DART_SUPPRESS_ANALYTICS=true "$FLUTTER_BIN" --suppress-analytics test \
    test/data/protocol/peer_contract_test.dart
run bash -c 'cd ios/domes_app && exec "$@"' _ \
    env CI=true DART_SUPPRESS_ANALYTICS=true "$FLUTTER_BIN" --suppress-analytics test \
    test/domain/models/drill_result_test.dart

run python3 tools/scoring_validation/generate_fixed_fixture.py \
    --fixture tools/scoring_validation/fixtures/fixed_two_pod_v1.json \
    --output tools/scoring_validation/generated/fixed_two_pod_v1.hpp --check
run python3 -m unittest discover -s tools/scoring_validation -p 'test_*.py' -v
run bash tools/scoring_validation/test_fs3_gate_failure.sh

printf '\nGATE_VERDICT=ACCEPTED_SOFTWARE_COMPATIBILITY\n'
printf 'PHYSICAL_EVIDENCE=UNVERIFIED\n'
printf 'UTC_FINISHED=%s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
