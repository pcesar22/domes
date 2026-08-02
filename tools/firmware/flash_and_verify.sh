#!/usr/bin/env bash
# Build once, flash one or more DOMES boards, and verify the framed UART protocol.
# Usage: flash_and_verify.sh [project_dir] [comma-separated ports]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
PROJECT_ARG="${1:-firmware/domes}"
PORTS="${2:-}"
EXPECTED_IDF_VERSION="ESP-IDF v5.4.4"
IDF_EXPORT_SCRIPT="${IDF_EXPORT_SCRIPT:-$HOME/esp/esp-idf/export.sh}"

if [[ "$PROJECT_ARG" = /* ]]; then
    PROJECT_DIR="$PROJECT_ARG"
else
    PROJECT_DIR="$(cd "$REPO_ROOT" && realpath -m "$PROJECT_ARG")"
fi

if [[ $# -ge 3 ]]; then
    echo "Warning: the legacy verify-string argument is ignored; verification uses domes-cli system info." >&2
fi

if [[ ! -f "$PROJECT_DIR/CMakeLists.txt" ]]; then
    echo "Firmware project not found: $PROJECT_DIR" >&2
    exit 2
fi

if [[ -z "$PORTS" ]]; then
    mapfile -t DETECTED_PORTS < <(
        find /dev/serial/by-id -maxdepth 1 -type l -name '*CP2102N*' -print 2>/dev/null | sort
    )
    if [[ "${#DETECTED_PORTS[@]}" -eq 1 ]]; then
        PORTS="${DETECTED_PORTS[0]}"
        echo "Auto-detected CP2102N device: $PORTS"
    else
        echo "Pass a comma-separated list of stable CP2102N /dev/serial/by-id paths." >&2
        echo "Detected ${#DETECTED_PORTS[@]} candidate devices; refusing to guess." >&2
        exit 2
    fi
fi

CLI_MANIFEST="$REPO_ROOT/tools/domes-cli/Cargo.toml"
CLI_BIN="$REPO_ROOT/tools/domes-cli/target/debug/domes-cli"

echo "=== DOMES build, flash, and verify ==="
echo "Project: $PROJECT_DIR"
echo "Ports:   $PORTS"

echo "=== Building host CLI ==="
cargo build --locked --manifest-path "$CLI_MANIFEST"

echo "=== Building firmware ==="
if [[ ! -f "$IDF_EXPORT_SCRIPT" ]]; then
    echo "ESP-IDF export script not found: $IDF_EXPORT_SCRIPT" >&2
    exit 2
fi
# shellcheck source=/dev/null
. "$IDF_EXPORT_SCRIPT" >/dev/null
IDF_VERSION="$(idf.py --version)"
if [[ "$IDF_VERSION" != "$EXPECTED_IDF_VERSION" ]]; then
    echo "Expected $EXPECTED_IDF_VERSION, found $IDF_VERSION" >&2
    exit 2
fi

# An ignored project-local sdkconfig may predate sdkconfig.defaults. Build from a
# new configuration every time so flashing cannot silently reuse stale settings.
TEMP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/domes-flash.XXXXXX")"
trap 'rm -rf "$TEMP_DIR"' EXIT
BUILD_DIR="$TEMP_DIR/build"
SDKCONFIG_FILE="$TEMP_DIR/sdkconfig"
idf.py -C "$PROJECT_DIR" -B "$BUILD_DIR" \
    -D "IDF_TARGET=esp32s3" -D "SDKCONFIG=$SDKCONFIG_FILE" build
EXPECTED_FIRMWARE_VERSION=$(python3 - "$BUILD_DIR/project_description.json" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as metadata_file:
    print(json.load(metadata_file)["project_version"])
PY
)
if [[ -z "$EXPECTED_FIRMWARE_VERSION" ]]; then
    echo "Firmware build did not report an embedded version" >&2
    exit 1
fi
echo "Embedded version: $EXPECTED_FIRMWARE_VERSION"

IFS=',' read -r -a PORT_ARRAY <<< "$PORTS"
PASSED=0
FAILED=0

for raw_port in "${PORT_ARRAY[@]}"; do
    PORT="${raw_port//[[:space:]]/}"
    if [[ -z "$PORT" ]]; then
        continue
    fi

    echo "=== [$PORT] Flashing firmware ==="
    if ! idf.py -C "$PROJECT_DIR" -B "$BUILD_DIR" \
        -D "IDF_TARGET=esp32s3" -D "SDKCONFIG=$SDKCONFIG_FILE" -p "$PORT" flash; then
        echo "=== [$PORT] FLASH FAILED ===" >&2
        FAILED=$((FAILED + 1))
        continue
    fi

    echo "=== [$PORT] Verifying framed UART protocol ==="
    # The image initializes BLE and ESP-NOW before the UART command task.
    sleep 2
    VERIFIED=0
    OUTPUT=""
    for attempt in {1..10}; do
        if OUTPUT="$(timeout 10s "$CLI_BIN" --port "$PORT" system info 2>&1)" &&
            grep -Fq "Firmware:   $EXPECTED_FIRMWARE_VERSION" <<< "$OUTPUT"; then
            HEALTH_OUTPUT=""
            SELF_TEST_OUTPUT=""
            if HEALTH_OUTPUT="$(timeout 20s "$CLI_BIN" --port "$PORT" system health 2>&1)" &&
                SELF_TEST_OUTPUT="$(timeout 30s "$CLI_BIN" --port "$PORT" system self-test 2>&1)"; then
                printf '%s\n%s\n%s\n' "$OUTPUT" "$HEALTH_OUTPUT" "$SELF_TEST_OUTPUT"
                VERIFIED=1
                break
            fi
            OUTPUT=$(printf '%s\n%s\n%s' "$OUTPUT" "$HEALTH_OUTPUT" "$SELF_TEST_OUTPUT")
        fi
        if [[ "$attempt" -lt 10 ]]; then
            sleep 1
        fi
    done

    if [[ "$VERIFIED" -eq 1 ]]; then
        echo "=== [$PORT] VERIFIED ==="
        PASSED=$((PASSED + 1))
    else
        printf '%s\n' "$OUTPUT" >&2
        echo "=== [$PORT] PROTOCOL VERIFICATION FAILED ===" >&2
        FAILED=$((FAILED + 1))
    fi
done

TOTAL=$((PASSED + FAILED))
echo "=== Summary: $PASSED/$TOTAL verified, $FAILED failed ==="

if [[ "$FAILED" -ne 0 || "$TOTAL" -eq 0 ]]; then
    exit 1
fi
