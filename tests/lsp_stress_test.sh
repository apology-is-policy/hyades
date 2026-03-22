#!/bin/bash
# LSP Stress Test - Tests parser resilience with incomplete/malformed input
# This test helped find and verify fixes for heap-buffer-overflow and infinite loop bugs
# in box_layout_parser.c when handling incomplete constructs during LSP editing.
#
# Usage:
#   ./tests/lsp_stress_test.sh              # Run once
#   ./tests/lsp_stress_test.sh --repeat N   # Run N times
#
# The test uses build-debug/cassilda which should be built with ASan enabled.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
CASSILDA="$PROJECT_ROOT/build-debug/cassilda"
BASE_FILE="$PROJECT_ROOT/tests/test6.cld"

# Parse arguments
REPEAT=1
while [[ $# -gt 0 ]]; do
    case $1 in
        --repeat)
            REPEAT="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [--repeat N]"
            exit 1
            ;;
    esac
done

# Check if debug build exists
if [ ! -x "$CASSILDA" ]; then
    echo "Error: Debug build not found at $CASSILDA"
    echo "Run: cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug && cmake --build build-debug"
    exit 1
fi

# Check if base file exists
if [ ! -f "$BASE_FILE" ]; then
    echo "Error: Base test file not found at $BASE_FILE"
    exit 1
fi

echo "LSP Stress Test"
echo "==============="
echo "Using cassilda: $CASSILDA"
echo "Base file: $BASE_FILE"
echo "Repeat count: $REPEAT"
echo ""

run_test() {
    local run_num=$1

    if [ "$REPEAT" -gt 1 ]; then
        echo "=== Run $run_num of $REPEAT ==="
    fi

    # Test 1: Rapid parsing of incomplete hbox
    echo "Test 1: 100 rapid parses of incomplete hbox"
    for i in {1..100}; do
        output=$(echo '\begin[30]{hbox}
    \child{
' | "$CASSILDA" lsp-debug - 2>&1)
        exit_code=$?
        if [ $exit_code -ne 0 ]; then
            echo "CRASH at iteration $i, exit code: $exit_code"
            echo "Output: $output"
            exit 1
        fi
    done
    echo "  PASS"

    # Test 2: Parsing with lone backslash at various positions
    echo "Test 2: 100 parses with trailing backslash"
    for i in {1..100}; do
        echo '\begin[30]{hbox}\' | "$CASSILDA" lsp-debug - >/dev/null 2>&1 || { echo "CRASH at iteration $i"; exit 1; }
    done
    echo "  PASS"

    # Test 3: Parse large file with various appended incomplete constructs
    echo "Test 3: Large file with incomplete constructs appended"
    for suffix in '\' '\begin' '\begin[' '\begin[30' '\begin[30]' '\begin[30]{' '\begin[30]{h' '\begin[30]{hbox' '\begin[30]{hbox}'; do
        cat "$BASE_FILE" > /tmp/stress_test_file.cld
        echo "$suffix" >> /tmp/stress_test_file.cld
        for i in {1..10}; do
            "$CASSILDA" lsp-debug /tmp/stress_test_file.cld >/dev/null 2>&1 || { echo "CRASH with suffix '$suffix' at iteration $i"; exit 1; }
        done
    done
    rm -f /tmp/stress_test_file.cld
    echo "  PASS"

    # Test 4: Nested incomplete structures
    echo "Test 4: Nested incomplete structures"
    for i in {1..50}; do
        echo '\begin{vbox}
\child{
    \begin[30]{hbox}
        \child{
            test
' | "$CASSILDA" lsp-debug - >/dev/null 2>&1 || { echo "CRASH at iteration $i"; exit 1; }
    done
    echo "  PASS"

    # Test 5: Rapid alternating complete/incomplete
    echo "Test 5: 200 rapid alternating complete/incomplete"
    for i in {1..200}; do
        if [ $((i % 2)) -eq 0 ]; then
            echo '\begin{vbox}\child{test}\end{vbox}' | "$CASSILDA" lsp-debug - >/dev/null 2>&1
        else
            echo '\begin{vbox}\child{' | "$CASSILDA" lsp-debug - >/dev/null 2>&1
        fi || { echo "CRASH at iteration $i"; exit 1; }
    done
    echo "  PASS"

    echo ""
}

# Run tests
for run in $(seq 1 $REPEAT); do
    run_test $run
done

echo "All stress tests passed!"
