#!/bin/bash
# DOMES Firmware - Host Test Runner
# Builds and runs unit tests on the host machine
#
# Usage: ./run_tests.sh
# Requirements: g++ (with C++20 support), make

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEST_DIR="${SCRIPT_DIR}/test/host"

echo "======================================="
echo "DOMES Firmware Host Tests"
echo "======================================="
echo ""

# Check requirements
if ! command -v g++ &> /dev/null; then
    echo "ERROR: g++ not found. Please install build-essential."
    exit 1
fi

if ! command -v make &> /dev/null; then
    echo "ERROR: make not found. Please install build-essential."
    exit 1
fi

# Build and run
cd "${TEST_DIR}"

echo "Building..."
make clean 2>/dev/null || true
make

echo ""
echo "Running tests..."
echo ""
make run

echo ""
echo "======================================="
echo "Tests Complete"
echo "======================================="
