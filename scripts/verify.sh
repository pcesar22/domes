#!/usr/bin/env bash
# Local verification script that mirrors CI checks.
# Run this before pushing to catch issues CI would catch.
#
# Usage:
#   ./scripts/verify.sh          # Run all checks
#   ./scripts/verify.sh --quick  # Skip firmware build (slow)

set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
QUICK=false

if [[ "${1:-}" == "--quick" ]]; then
    QUICK=true
fi

pass() { echo -e "${GREEN}PASS${NC} $1"; }
fail() { echo -e "${RED}FAIL${NC} $1"; exit 1; }
skip() { echo -e "${YELLOW}SKIP${NC} $1"; }

echo "=== DOMES Local Verification ==="
echo ""

# 1. Unit tests
echo "--- Unit Tests ---"
cd "$ROOT_DIR/firmware/test_app"
rm -rf build
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug > /dev/null 2>&1
if make -j"$(nproc)" > /dev/null 2>&1; then
    if ./test_app > /dev/null 2>&1; then
        pass "Unit tests ($(./test_app --gtest_list_tests 2>/dev/null | grep -c '\.') suites)"
    else
        fail "Unit tests"
    fi
else
    fail "Unit test build"
fi

# 2. CLI build
echo "--- CLI Build ---"
cd "$ROOT_DIR/tools/domes-cli"
if cargo build --release 2>&1 | tail -1 | grep -q "Finished"; then
    pass "CLI build"
else
    fail "CLI build"
fi

# 3. CLI tests
if cargo test 2>&1 | grep -q "test result: ok"; then
    pass "CLI tests"
else
    fail "CLI tests"
fi

# 4. Firmware build (slow, skip with --quick)
if $QUICK; then
    skip "Firmware build (--quick)"
else
    echo "--- Firmware Build ---"
    cd "$ROOT_DIR/firmware/domes"
    if bash -c '. ~/esp/esp-idf/export.sh > /dev/null 2>&1 && idf.py build' > /dev/null 2>&1; then
        SIZE=$(stat -c%s build/domes.bin 2>/dev/null || echo 0)
        MAX_SIZE=1966080
        if [ "$SIZE" -gt "$MAX_SIZE" ]; then
            fail "Binary too large: $SIZE > $MAX_SIZE"
        fi
        pass "Firmware build ($(numfmt --to=iec "$SIZE" 2>/dev/null || echo "${SIZE}B"))"
    else
        fail "Firmware build"
    fi
fi

echo ""
echo -e "${GREEN}All checks passed.${NC}"
