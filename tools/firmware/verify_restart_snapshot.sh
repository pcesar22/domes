#!/usr/bin/env bash
# Verify a clean-restart snapshot against its expected boot and version-matched ELF.

set -euo pipefail

usage() {
    echo "Usage: $0 <serial-port> <expected-boot-count> <expected-version> <pre-restart-elf> [domes-cli]" >&2
}

if [[ $# -lt 4 || $# -gt 5 ]]; then
    usage
    exit 2
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
PORT=$1
EXPECTED_BOOT_COUNT=$2
EXPECTED_VERSION=$3
ELF=$4
CLI=${5:-"$REPO_ROOT/tools/domes-cli/target/release/domes-cli"}

if [[ ! "$EXPECTED_BOOT_COUNT" =~ ^[1-9][0-9]*$ ]]; then
    echo "Expected boot count must be a positive integer: $EXPECTED_BOOT_COUNT" >&2
    exit 2
fi
if [[ ! -f "$ELF" ]]; then
    echo "Pre-restart ELF not found: $ELF" >&2
    exit 2
fi
METADATA="$(dirname "$ELF")/project_description.json"
if [[ ! -f "$METADATA" ]]; then
    echo "ESP-IDF project metadata not found beside ELF: $METADATA" >&2
    exit 2
fi
if [[ ! -x "$CLI" ]]; then
    echo "domes-cli not found or not executable: $CLI" >&2
    exit 2
fi
if ! command -v xtensa-esp32s3-elf-addr2line >/dev/null 2>&1; then
    echo "xtensa-esp32s3-elf-addr2line is required" >&2
    exit 2
fi
if ! command -v sha256sum >/dev/null 2>&1; then
    echo "sha256sum is required" >&2
    exit 2
fi

python3 - "$METADATA" "$ELF" "$EXPECTED_VERSION" <<'PY'
import json
import pathlib
import sys

metadata_path = pathlib.Path(sys.argv[1])
elf_path = pathlib.Path(sys.argv[2]).resolve()
expected_version = sys.argv[3]
metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
metadata_elf = (metadata_path.parent / metadata.get("app_elf", "")).resolve()
if metadata_elf != elf_path:
    raise SystemExit(
        f"ELF does not match ESP-IDF project metadata: {elf_path} != {metadata_elf}"
    )
if metadata.get("project_version") != expected_version:
    raise SystemExit(
        "ELF project version mismatch: expected "
        f"{expected_version}, got {metadata.get('project_version', 'missing')}"
    )
PY

snapshot=$("$CLI" --port "$PORT" system crash-dump)
printf '%s\n' "$snapshot"

boot_count=$(awk '/Boot count:/ {print $3; exit}' <<< "$snapshot")
if [[ "$boot_count" != "$EXPECTED_BOOT_COUNT" ]]; then
    echo "Restart snapshot boot count mismatch: expected $EXPECTED_BOOT_COUNT, got ${boot_count:-missing}" >&2
    exit 1
fi

firmware_version=$(awk '/Firmware:/ {print $2; exit}' <<< "$snapshot")
if [[ "$firmware_version" != "$EXPECTED_VERSION" ]]; then
    echo "Restart snapshot firmware mismatch: expected $EXPECTED_VERSION, got ${firmware_version:-missing}" >&2
    exit 1
fi

format_version=$(awk '/Snapshot format:/ {print $3; exit}' <<< "$snapshot")
if [[ "$format_version" != "2" ]]; then
    echo "Restart snapshot format mismatch: expected 2, got ${format_version:-missing}" >&2
    exit 1
fi

snapshot_elf_sha=$(awk '/ELF SHA256:/ {print $3; exit}' <<< "$snapshot")
expected_elf_sha=$(sha256sum "$ELF" | awk '{print $1}')
if [[ ! "$snapshot_elf_sha" =~ ^[0-9a-f]{64}$ ]] ||
   [[ "$snapshot_elf_sha" != "$expected_elf_sha" ]]; then
    echo "Restart snapshot ELF mismatch: expected $expected_elf_sha, got ${snapshot_elf_sha:-missing}" >&2
    exit 1
fi

free_heap=$(awk '/Internal free heap:/ {print $4; exit}' <<< "$snapshot")
if [[ ! "$free_heap" =~ ^[1-9][0-9]*$ ]] || ((free_heap >= 1000000)); then
    echo "Restart snapshot internal heap is invalid: ${free_heap:-missing}" >&2
    exit 1
fi

mapfile -t pcs < <(awk '/#[0-9]+:/ {print $2}' <<< "$snapshot")
if ((${#pcs[@]} == 0)); then
    echo "Restart snapshot did not contain a backtrace" >&2
    exit 1
fi
for i in "${!pcs[@]}"; do
    if [[ ! "${pcs[$i]}" =~ ^0x[0-9A-Fa-f]{8}$ ]]; then
        echo "Restart snapshot contained an invalid PC at entry $i: ${pcs[$i]}" >&2
        exit 1
    fi
    if [[ "${pcs[$i]}" == "0x00000000" || "${pcs[$i],,}" == "0xffffffff" ]]; then
        echo "Restart snapshot contained an invalid PC at entry $i: ${pcs[$i]}" >&2
        exit 1
    fi
    if ((i > 0)) && [[ "${pcs[$((i - 1))]}" == "${pcs[$i]}" ]]; then
        echo "Restart snapshot repeated adjacent frame ${pcs[$i]} at entries $((i - 1)) and $i" >&2
        exit 1
    fi
done

for i in "${!pcs[@]}"; do
    symbol=$(xtensa-esp32s3-elf-addr2line -pfiaC -e "$ELF" "${pcs[$i]}")
    printf '%s\n' "$symbol"
    if ! grep -Eqi "^${pcs[$i]}:[[:space:]]+[^?]" <<< "$symbol"; then
        echo "Restart snapshot entry $i (${pcs[$i]}) did not resolve against the supplied pre-restart ELF" >&2
        exit 1
    fi
done

echo "Restart snapshot verified for boot $EXPECTED_BOOT_COUNT ($EXPECTED_VERSION)"
