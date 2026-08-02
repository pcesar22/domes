#!/usr/bin/env bash
# Run the local equivalents of repository CI checks.

set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
QUICK=false

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

echo "=== DOMES Local Verification ==="

section "Generated Protocol Bindings"
if "$ROOT_DIR/tools/generate_protocols.sh" --check all; then
    pass "Nanopb and Dart bindings"
else
    fail "Generated protocol bindings"
fi

section "Host Firmware Tests"
if cmake \
    -S "$ROOT_DIR/firmware/test_app" \
    -B "$ROOT_DIR/firmware/test_app/build" \
    -DCMAKE_BUILD_TYPE=Debug \
    && cmake --build "$ROOT_DIR/firmware/test_app/build" -j"$(nproc)" \
    && ctest --test-dir "$ROOT_DIR/firmware/test_app/build" --output-on-failure; then
    pass "Host firmware tests"
else
    fail "Host firmware tests"
fi

section "Rust CLI"
if (
    cd "$ROOT_DIR/tools/domes-cli" &&
        cargo fmt --check &&
        cargo clippy --all-targets --all-features &&
        cargo build --release &&
        cargo test
); then
    pass "CLI format, lint, release build, and tests"
else
    fail "Rust CLI"
fi

section "Host Tooling"
if (
    cd "$ROOT_DIR" &&
        python3 -m unittest tools.trace.test_trace_merge &&
        python3 -m py_compile \
            tools/trace/trace_merge.py \
            tools/trace/test_trace_merge.py \
            tools/firmware/monitor_serial.py \
            .codex/skills/domes-esp32-firmware/scripts/monitor_serial.py \
            .claude/skills/esp32-firmware/scripts/monitor_serial.py &&
        bash -n \
            scripts/verify.sh \
            tools/generate_protocols.sh \
            tools/firmware/flash_and_verify.sh \
            .codex/skills/domes-esp32-firmware/scripts/flash_and_verify.sh \
            .claude/skills/esp32-firmware/scripts/flash_and_verify.sh
); then
    pass "Trace tests and script syntax"
else
    fail "Host tooling"
fi

section "Flutter App"
if (
    cd "$ROOT_DIR/ios/domes_app" &&
        flutter pub get &&
        flutter analyze &&
        flutter test
); then
    pass "Flutter dependencies, analysis, and tests"
else
    fail "Flutter app"
fi

section "ESP-IDF Firmware"
if $QUICK; then
    skip "Firmware build (--quick)"
elif (
    cd "$ROOT_DIR/firmware/domes" &&
        . ~/esp/esp-idf/export.sh &&
        idf.py build
); then
    SIZE=$(stat -c%s "$ROOT_DIR/firmware/domes/build/domes.bin")
    MAX_SIZE=1966080
    if ((SIZE > MAX_SIZE)); then
        fail "Binary too large: $SIZE > $MAX_SIZE"
    fi
    pass "Firmware build ($SIZE / $MAX_SIZE bytes)"
else
    fail "Firmware build"
fi

echo ""
echo -e "${GREEN}All configured checks passed.${NC}"
