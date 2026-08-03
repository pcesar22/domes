#!/usr/bin/env bash
# Run the local equivalents of repository CI checks.

set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
VERIFY_TMP="$(mktemp -d)"
QUICK=false
IDF_EXPORT_SCRIPT="${IDF_EXPORT_SCRIPT:-$HOME/esp/esp-idf/export.sh}"

cleanup() {
    rm -rf "$VERIFY_TMP"
}
trap cleanup EXIT

case "${1:-}" in
    "") ;;
    --quick) QUICK=true ;;
    *)
        echo "Usage: scripts/verify.sh [--quick]" >&2
        exit 2
        ;;
esac

pass() { echo -e "${GREEN}PASS${NC} $1"; }
fail() {
    echo -e "${RED}FAIL${NC} $1" >&2
    exit 1
}
skip() { echo -e "${YELLOW}SKIP${NC} $1"; }
section() {
    echo ""
    echo "--- $1 ---"
}

tracked_existing_files() {
    local pattern=$1
    git ls-files -z -- "$pattern" | while IFS= read -r -d '' file; do
        if [[ -f "$file" ]]; then
            printf '%s\0' "$file"
        fi
    done
}

echo "=== DOMES Local Verification ==="

section "Pinned Toolchains"
rust_version=$(rustc --version)
if [[ "$rust_version" == "rustc 1.92.0 "* ]]; then
    pass "$rust_version"
else
    fail "Expected Rust 1.92.0, found: $rust_version"
fi

pre_commit_version=$(pre-commit --version)
if [[ "$pre_commit_version" == "pre-commit 4.6.1" ]]; then
    pass "$pre_commit_version"
else
    fail "Expected pre-commit 4.6.1, found: $pre_commit_version"
fi

flutter_version=$(flutter --version | sed -n '1p')
if [[ "$flutter_version" == "Flutter 3.44.8 "* ]]; then
    pass "$flutter_version"
else
    fail "Expected Flutter 3.44.8, found: $flutter_version"
fi

if dart pub global list | grep -qx 'protoc_plugin 25.0.0'; then
    pass "Dart protoc_plugin 25.0.0"
else
    fail "Expected globally activated Dart protoc_plugin 25.0.0"
fi

section "Generated Protocol Bindings"
if "$ROOT_DIR/tools/generate_protocols.sh" --check all; then
    pass "Nanopb and Dart bindings"
else
    fail "Generated protocol bindings"
fi

section "Host Firmware Tests"
if cmake \
    -S "$ROOT_DIR/firmware/test_app" \
    -B "$VERIFY_TMP/host-test-build" \
    -DCMAKE_BUILD_TYPE=Debug \
    && cmake --build "$VERIFY_TMP/host-test-build" -j"$(nproc)" \
    && ctest \
        --test-dir "$VERIFY_TMP/host-test-build" \
        --output-on-failure \
        --no-tests=error; then
    pass "Host firmware tests"
else
    fail "Host firmware tests"
fi

section "Rust CLI"
if (
    cd "$ROOT_DIR/tools/domes-cli" &&
        cargo fmt --check &&
        cargo clippy --locked --all-targets --all-features -- -D warnings &&
        cargo build --locked &&
        cargo build --locked --release &&
        cargo test --locked --all-targets --all-features
); then
    pass "CLI format, lint, debug/release builds, and tests"
else
    fail "Rust CLI"
fi

section "Host Tooling"
if (
    cd "$ROOT_DIR" &&
        pre-commit run --all-files --show-diff-on-failure &&
        python3 -m unittest discover -s tools/agent_eval -p 'test_*.py' -v &&
        python3 -m unittest discover -s tools/ci -p 'test_*.py' -v &&
        python3 -m unittest discover -s tools/docs -p 'test_*.py' -v &&
        python3 tools/docs/check_markdown_links.py &&
        python3 -m unittest discover -s tools/trace -p 'test_*.py' -v &&
        python3 tools/trace/generate_trace_names.py --check &&
        mapfile -d '' python_files < <(tracked_existing_files '*.py') &&
        python3 -m py_compile "${python_files[@]}" &&
        tracked_existing_files '*.sh' | xargs -0 -r -n1 bash -n &&
        tracked_existing_files '*.sh' | xargs -0 -r shellcheck &&
        go run github.com/rhysd/actionlint/cmd/actionlint@v1.7.10 -color
); then
    pass "Documentation links, Python tooling, shell lint, and workflows"
else
    fail "Host tooling"
fi

section "Flutter App"
if (
    cd "$ROOT_DIR/ios/domes_app" &&
        flutter clean &&
        flutter pub get --enforce-lockfile &&
        flutter analyze --fatal-infos --fatal-warnings &&
        flutter test &&
        flutter build linux --release
); then
    pass "Flutter dependencies, analysis, tests, and Linux release build"
else
    fail "Flutter app"
fi

section "ESP-IDF Firmware"
if $QUICK; then
    skip "Firmware build (--quick)"
elif (
    IDF_BUILD_DIR="$VERIFY_TMP/idf-build"
    IDF_RELEASE_DIR="$VERIFY_TMP/firmware-release"
    IDF_SDKCONFIG="$VERIFY_TMP/sdkconfig"

    cd "$ROOT_DIR/firmware/domes" || exit 1
    if [[ ! -f "$IDF_EXPORT_SCRIPT" ]]; then
        echo "ESP-IDF export script not found: $IDF_EXPORT_SCRIPT" >&2
        exit 1
    fi
    # shellcheck source=/dev/null
    . "$IDF_EXPORT_SCRIPT" || exit 1
    idf_version=$(idf.py --version) || exit 1
    echo "$idf_version"
    if [[ "$idf_version" != "ESP-IDF v5.4.4" ]]; then
        echo "Expected ESP-IDF v5.4.4, found: $idf_version" >&2
        exit 1
    fi
    idf.py \
        -B "$IDF_BUILD_DIR" \
        -D "IDF_TARGET=esp32s3" \
        -D "SDKCONFIG=$IDF_SDKCONFIG" \
        build || exit 1
    python3 - "$IDF_BUILD_DIR/config/sdkconfig.json" <<'PY' || exit 1
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
        exit 1
    fi

    size=$(stat -c%s "$IDF_BUILD_DIR/domes.bin") || exit 1
    max_size=1966080
    echo "Binary size: $size / $max_size bytes"
    if ((size > max_size)); then
        echo "Binary exceeds OTA partition size ($size > $max_size bytes)" >&2
        exit 1
    fi

    mkdir -p "$IDF_RELEASE_DIR" || exit 1
    cd "$IDF_BUILD_DIR" || exit 1
    python -m esptool --chip esp32s3 merge_bin \
        -o "$IDF_RELEASE_DIR/domes-factory.bin" @flash_args || exit 1
    python -m esptool --chip esp32s3 image_info domes.bin || exit 1
    python -m esptool --chip esp32s3 image_info \
        "$IDF_RELEASE_DIR/domes-factory.bin" || exit 1

    cp domes.bin "$IDF_RELEASE_DIR/domes.bin" || exit 1
    cp bootloader/bootloader.bin "$IDF_RELEASE_DIR/bootloader.bin" || exit 1
    cp partition_table/partition-table.bin "$IDF_RELEASE_DIR/partition-table.bin" || exit 1
    cp ota_data_initial.bin "$IDF_RELEASE_DIR/ota_data_initial.bin" || exit 1
    sed \
        -e 's#bootloader/bootloader.bin#bootloader.bin#' \
        -e 's#partition_table/partition-table.bin#partition-table.bin#' \
        flash_args > "$IDF_RELEASE_DIR/flash_args" || exit 1

    cd "$IDF_RELEASE_DIR" || exit 1
    test -s domes.bin || exit 1
    test -s domes-factory.bin || exit 1
    test -s bootloader.bin || exit 1
    test -s partition-table.bin || exit 1
    test -s ota_data_initial.bin || exit 1
    sha256sum \
        domes.bin \
        domes-factory.bin \
        bootloader.bin \
        partition-table.bin \
        ota_data_initial.bin \
        flash_args > SHA256SUMS || exit 1
    sha256sum --check SHA256SUMS || exit 1
); then
    pass "Clean firmware build and release package"
else
    fail "Clean firmware build and release package"
fi

echo ""
echo -e "${GREEN}All configured checks passed.${NC}"
