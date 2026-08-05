#!/usr/bin/env bash
# Run full or change-aware local equivalents of repository CI checks.

set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PLAN_TOOL="$ROOT_DIR/tools/verify/verify_plan.py"
QUICK=false
DRY_RUN=false
CHANGED_BASE=""
JSON_SUMMARY=""
KEEP_ARTIFACTS=""
COMPONENTS=()
IDF_EXPORT_SCRIPT="${IDF_EXPORT_SCRIPT:-$HOME/esp/esp-idf/export.sh}"

usage() {
    cat <<'EOF'
Usage: scripts/verify.sh [options]

With no options, run the complete repository software gate.

  --quick                     Skip only the ESP-IDF firmware build
  --changed <base>            Select checks for changes since a Git revision
  --component <name>          Select firmware, cli, flutter, docs, tooling,
                              protocol, or workflow checks (repeatable)
  --json-summary <path>       Write a schema-versioned result document
  --keep-artifacts <dir>      Retain logs and build artifacts below this directory
  --dry-run                   Resolve and report selection without running checks
  -h, --help                  Show this help
EOF
}

while (($# > 0)); do
    case "$1" in
        --quick)
            QUICK=true
            shift
            ;;
        --dry-run)
            DRY_RUN=true
            shift
            ;;
        --changed|--component|--json-summary|--keep-artifacts)
            if (($# < 2)) || [[ -z "$2" ]]; then
                echo "Missing value for $1" >&2
                usage >&2
                exit 2
            fi
            case "$1" in
                --changed) CHANGED_BASE=$2 ;;
                --component) COMPONENTS+=("$2") ;;
                --json-summary) JSON_SUMMARY=$2 ;;
                --keep-artifacts) KEEP_ARTIFACTS=$2 ;;
            esac
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown argument: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

RETAIN_ARTIFACTS=false
if [[ -n "$KEEP_ARTIFACTS" ]]; then
    mkdir -p "$KEEP_ARTIFACTS"
    KEEP_ROOT="$(cd "$KEEP_ARTIFACTS" && pwd)"
    VERIFY_TMP="$KEEP_ROOT/verify-$(date -u +%Y%m%dT%H%M%SZ)-$$"
    mkdir "$VERIFY_TMP"
    RETAIN_ARTIFACTS=true
else
    VERIFY_TMP="$(mktemp -d)"
fi

# Called by the EXIT trap below; ShellCheck cannot resolve trap callbacks.
# shellcheck disable=SC2317,SC2329
cleanup() {
    if ! $RETAIN_ARTIFACTS; then
        rm -rf -- "${VERIFY_TMP:?}"
    fi
}
trap cleanup EXIT

LOG_DIR="$VERIFY_TMP/logs"
PLAN_FILE="$VERIFY_TMP/plan.json"
RESULTS_FILE="$VERIFY_TMP/results.tsv"
mkdir -p "$LOG_DIR"
: > "$RESULTS_FILE"

PLAN_ARGS=(plan --root "$ROOT_DIR" --output "$PLAN_FILE")
if $QUICK; then
    PLAN_ARGS+=(--quick)
fi
if [[ -n "$CHANGED_BASE" ]]; then
    PLAN_ARGS+=(--base "$CHANGED_BASE")
fi
for component in "${COMPONENTS[@]}"; do
    PLAN_ARGS+=(--component "$component")
done
python3 "$PLAN_TOOL" "${PLAN_ARGS[@]}"
python3 "$PLAN_TOOL" render-plan --input "$PLAN_FILE"
mapfile -t SELECTED_CHECKS < <(
    python3 "$PLAN_TOOL" selected --input "$PLAN_FILE"
)

pass() { echo -e "${GREEN}PASS${NC} $1"; }
mark_fail() { echo -e "${RED}FAIL${NC} $1" >&2; }
skip() { echo -e "${YELLOW}SKIP${NC} $1"; }
section() {
    echo ""
    echo "--- $1 ---"
}

is_selected() {
    local wanted=$1
    local selected
    for selected in "${SELECTED_CHECKS[@]}"; do
        if [[ "$selected" == "$wanted" ]]; then
            return 0
        fi
    done
    return 1
}

tracked_existing_files() {
    local pattern=$1
    git ls-files -z -- "$pattern" | while IFS= read -r -d '' file; do
        if [[ -f "$file" ]]; then
            printf '%s\0' "$file"
        fi
    done
}

check_protocol() {
    local plugin_list
    plugin_list=$(dart pub global list) || return 1
    if ! grep -qx 'protoc_plugin 25.0.0' <<< "$plugin_list"; then
        echo "Expected globally activated Dart protoc_plugin 25.0.0" >&2
        return 1
    fi
    echo "Dart protoc_plugin 25.0.0"
    "$ROOT_DIR/tools/generate_protocols.sh" --check all
}

check_host_firmware() {
    cmake \
        -S "$ROOT_DIR/firmware/test_app" \
        -B "$VERIFY_TMP/host-test-build" \
        -DCMAKE_BUILD_TYPE=Debug &&
        cmake --build "$VERIFY_TMP/host-test-build" -j"$(nproc)" &&
        ctest \
            --test-dir "$VERIFY_TMP/host-test-build" \
            --output-on-failure \
            --no-tests=error
}

check_cli() {
    local rust_version
    rust_version=$(rustc --version) || return 1
    if [[ "$rust_version" != "rustc 1.92.0 "* ]]; then
        echo "Expected Rust 1.92.0, found: $rust_version" >&2
        return 1
    fi
    echo "$rust_version"
    (
        cd "$ROOT_DIR/tools/domes-cli" || exit 1
        if $RETAIN_ARTIFACTS; then
            export CARGO_TARGET_DIR="$VERIFY_TMP/cargo-target"
        fi
        cargo fmt --check &&
            cargo clippy --locked --all-targets --all-features -- -D warnings &&
            cargo build --locked &&
            cargo build --locked --release &&
            cargo test --locked --all-targets --all-features
    )
}

check_host_tooling() {
    local pre_commit_version
    pre_commit_version=$(pre-commit --version) || return 1
    if [[ "$pre_commit_version" != "pre-commit 4.6.1" ]]; then
        echo "Expected pre-commit 4.6.1, found: $pre_commit_version" >&2
        return 1
    fi
    echo "$pre_commit_version"
    (
        cd "$ROOT_DIR" &&
            pre-commit run --all-files --show-diff-on-failure &&
            python3 -m unittest discover -s tools/agent_eval -p 'test_*.py' -v &&
            python3 -m unittest discover -s tools/ci -p 'test_*.py' -v &&
            python3 -m unittest discover -s tools/doctor -p 'test_*.py' -v &&
            python3 -m unittest discover -s tools/docs -p 'test_*.py' -v &&
            python3 -m unittest discover -s tools/verify -p 'test_*.py' -v &&
            python3 -m unittest discover -s tools/simulation -p 'test_*.py' -v &&
            python3 tools/docs/check_markdown_links.py &&
            python3 -m unittest discover -s tools/trace -p 'test_*.py' -v &&
            python3 tools/trace/generate_trace_names.py --check &&
            mapfile -d '' python_files < <(tracked_existing_files '*.py') &&
            python3 -m py_compile "${python_files[@]}" &&
            tracked_existing_files '*.sh' | xargs -0 -r -n1 bash -n &&
            tracked_existing_files '*.sh' | xargs -0 -r shellcheck &&
            go run github.com/rhysd/actionlint/cmd/actionlint@v1.7.10 -color
    )
}

check_flutter() {
    local flutter_version
    flutter_version=$(flutter --version | sed -n '1p') || return 1
    if [[ "$flutter_version" != "Flutter 3.44.8 "* ]]; then
        echo "Expected Flutter 3.44.8, found: $flutter_version" >&2
        return 1
    fi
    echo "$flutter_version"
    (
        cd "$ROOT_DIR/ios/domes_app" &&
            : > "$VERIFY_TMP/flutter-build-started" &&
            flutter clean &&
            flutter pub get --enforce-lockfile &&
            flutter analyze --fatal-infos --fatal-warnings &&
            flutter test &&
            flutter build linux --release
    )
}

check_firmware() {
    local idf_build_dir="$VERIFY_TMP/idf-build"
    local idf_qemu_build_dir="$VERIFY_TMP/idf-qemu-build"
    local idf_release_dir="$VERIFY_TMP/firmware-release"
    local idf_sdkconfig="$VERIFY_TMP/sdkconfig"
    local idf_qemu_sdkconfig="$VERIFY_TMP/sdkconfig-qemu"
    local idf_version
    local size
    local max_size=1966080

    cd "$ROOT_DIR/firmware/domes" || return 1
    if [[ ! -f "$IDF_EXPORT_SCRIPT" ]]; then
        echo "ESP-IDF export script not found: $IDF_EXPORT_SCRIPT" >&2
        return 1
    fi
    # shellcheck source=/dev/null
    . "$IDF_EXPORT_SCRIPT" || return 1
    idf_version=$(idf.py --version) || return 1
    echo "$idf_version"
    if [[ "$idf_version" != "ESP-IDF v5.4.4" ]]; then
        echo "Expected ESP-IDF v5.4.4, found: $idf_version" >&2
        return 1
    fi
    idf.py \
        -B "$idf_build_dir" \
        -D "IDF_TARGET=esp32s3" \
        -D "SDKCONFIG=$idf_sdkconfig" \
        build || return 1
    python3 - "$idf_build_dir/config/sdkconfig.json" <<'PY' || return 1
import json
import sys

with open(sys.argv[1], encoding="utf-8") as config_file:
    config = json.load(config_file)
if config.get("BOOTLOADER_APP_ROLLBACK_ENABLE") is not True:
    raise SystemExit("firmware build does not enable bootloader app rollback")
PY
    if [[ -n "$(git status --porcelain -- dependencies.lock)" ]]; then
        echo "ESP-IDF rewrote firmware/domes/dependencies.lock" >&2
        git diff -- dependencies.lock >&2
        return 1
    fi

    idf.py \
        -B "$idf_qemu_build_dir" \
        -D "IDF_TARGET=esp32s3" \
        -D "SDKCONFIG=$idf_qemu_sdkconfig" \
        -D "SDKCONFIG_DEFAULTS=$ROOT_DIR/firmware/domes/sdkconfig.qemu.defaults" \
        build || return 1
    python3 "$ROOT_DIR/tools/simulation/qemu_runtime.py" validate-builds \
        --physical-build "$idf_build_dir" \
        --qemu-build "$idf_qemu_build_dir" || return 1
    if [[ -n "$(git status --porcelain -- dependencies.lock)" ]]; then
        echo "ESP-IDF rewrote firmware/domes/dependencies.lock" >&2
        git diff -- dependencies.lock >&2
        return 1
    fi

    size=$(stat -c%s "$idf_build_dir/domes.bin") || return 1
    echo "Binary size: $size / $max_size bytes"
    if ((size > max_size)); then
        echo "Binary exceeds OTA partition size ($size > $max_size bytes)" >&2
        return 1
    fi

    mkdir -p "$idf_release_dir" || return 1
    cd "$idf_build_dir" || return 1
    python -m esptool --chip esp32s3 merge_bin \
        -o "$idf_release_dir/domes-factory.bin" @flash_args || return 1
    python -m esptool --chip esp32s3 image_info domes.bin || return 1
    python -m esptool --chip esp32s3 image_info \
        "$idf_release_dir/domes-factory.bin" || return 1

    cp domes.bin "$idf_release_dir/domes.bin" || return 1
    cp domes-fidelity-manifest.json \
        "$idf_release_dir/domes-fidelity-manifest.json" || return 1
    cp bootloader/bootloader.bin "$idf_release_dir/bootloader.bin" || return 1
    cp partition_table/partition-table.bin \
        "$idf_release_dir/partition-table.bin" || return 1
    cp ota_data_initial.bin "$idf_release_dir/ota_data_initial.bin" || return 1
    sed \
        -e 's#bootloader/bootloader.bin#bootloader.bin#' \
        -e 's#partition_table/partition-table.bin#partition-table.bin#' \
        flash_args > "$idf_release_dir/flash_args" || return 1

    cd "$idf_release_dir" || return 1
    test -s domes.bin || return 1
    test -s domes-fidelity-manifest.json || return 1
    test -s domes-factory.bin || return 1
    test -s bootloader.bin || return 1
    test -s partition-table.bin || return 1
    test -s ota_data_initial.bin || return 1
    sha256sum \
        domes.bin \
        domes-fidelity-manifest.json \
        domes-factory.bin \
        bootloader.bin \
        partition-table.bin \
        ota_data_initial.bin \
        flash_args > SHA256SUMS || return 1
    sha256sum --check SHA256SUMS
}

record_result() {
    printf '%s\t%s\t%s\t%s\t%s\n' "$1" "$2" "$3" "$4" "$5" >> "$RESULTS_FILE"
}

retain_component_artifacts() {
    local identifier=$1
    if [[
        "$identifier" == "flutter" &&
        -f "$VERIFY_TMP/flutter-build-started" &&
        -d "$ROOT_DIR/ios/domes_app/build"
    ]]; then
        cp -a "$ROOT_DIR/ios/domes_app/build" "$VERIFY_TMP/flutter-build"
    fi
}

execute_check() {
    case "$1" in
        protocol) check_protocol ;;
        host_firmware) check_host_firmware ;;
        cli) check_cli ;;
        host_tooling) check_host_tooling ;;
        flutter) check_flutter ;;
        firmware) check_firmware ;;
        *)
            echo "Unknown verification check: $1" >&2
            return 2
            ;;
    esac
}

run_check() {
    local identifier=$1
    local title=$2
    local log_file="$LOG_DIR/$identifier.log"
    local recorded_log=""
    local started=$SECONDS
    local duration
    local exit_code

    section "$title"
    if ! is_selected "$identifier"; then
        skip "$title (not selected)"
        record_result "$identifier" skipped 0 0 ""
        return
    fi
    if $DRY_RUN; then
        skip "$title (dry run; selected)"
        record_result "$identifier" skipped 0 0 ""
        return
    fi

    set +e
    execute_check "$identifier" 2>&1 | tee "$log_file"
    exit_code=${PIPESTATUS[0]}
    set -e
    duration=$((SECONDS - started))
    if $RETAIN_ARTIFACTS; then
        recorded_log=$log_file
        if ! retain_component_artifacts "$identifier" >> "$log_file" 2>&1; then
            echo "Failed to retain $identifier build artifacts" | tee -a "$log_file" >&2
            exit_code=1
        fi
    fi
    if ((exit_code == 0)); then
        pass "$title (${duration}s)"
        record_result "$identifier" passed 0 "$duration" "$recorded_log"
    else
        mark_fail "$title (${duration}s, exit $exit_code)"
        record_result "$identifier" failed "$exit_code" "$duration" "$recorded_log"
    fi
}

echo "=== DOMES Local Verification ==="
run_check protocol "Generated Protocol Bindings"
run_check host_firmware "Host Firmware Tests"
run_check cli "Rust CLI"
run_check host_tooling "Host Tooling"
run_check flutter "Flutter App"
run_check firmware "ESP-IDF Firmware"

if [[ -n "$JSON_SUMMARY" ]]; then
    SUMMARY_FILE=$JSON_SUMMARY
else
    SUMMARY_FILE="$VERIFY_TMP/summary.json"
fi
SUMMARY_ARGS=(
    summarize
    --plan "$PLAN_FILE"
    --results "$RESULTS_FILE"
    --output "$SUMMARY_FILE"
)
if $RETAIN_ARTIFACTS; then
    SUMMARY_ARGS+=(--artifacts "$VERIFY_TMP")
fi
set +e
python3 "$PLAN_TOOL" "${SUMMARY_ARGS[@]}"
SUMMARY_EXIT=$?
set -e
python3 "$PLAN_TOOL" render-summary --input "$SUMMARY_FILE"

if [[ -n "$JSON_SUMMARY" ]]; then
    echo "JSON summary: $SUMMARY_FILE"
fi
if $DRY_RUN; then
    echo "Dry-run selection completed; no checks were executed."
elif ((SUMMARY_EXIT == 0)); then
    echo -e "${GREEN}All selected checks passed.${NC}"
else
    echo -e "${RED}One or more selected checks failed.${NC}" >&2
fi
exit "$SUMMARY_EXIT"
