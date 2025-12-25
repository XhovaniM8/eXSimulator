#!/bin/bash
# Run benchmarks and generate reports

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build"
OUTPUT_DIR="$PROJECT_DIR/data/benchmarks"

mkdir -p "$OUTPUT_DIR"

TIMESTAMP=$(date +%Y%m%d_%H%M%S)

echo "=== Exchange Simulator Benchmark Suite ==="
echo ""

# Build if needed
if [ ! -f "$BUILD_DIR/bin/exchange_sim" ]; then
    echo "Building project..."
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    cmake -DCMAKE_BUILD_TYPE=Release ..
    make -j$(nproc)
    cd "$PROJECT_DIR"
fi

# Run different configurations
run_benchmark() {
    local name="$1"
    local args="$2"
    local output="$OUTPUT_DIR/${name}_${TIMESTAMP}.json"
    
    echo "Running: $name"
    echo "Args: $args"
    
    "$BUILD_DIR/bin/exchange_sim" $args 2>&1 | tee "$OUTPUT_DIR/${name}_${TIMESTAMP}.log"
    
    echo ""
}

# Warmup run
echo "=== Warmup ==="
"$BUILD_DIR/bin/exchange_sim" --symbols 1 --agents 10 --ticks 1000 > /dev/null 2>&1

# Benchmark runs
echo "=== Benchmark Runs ==="

run_benchmark "single_symbol" "--symbols 1 --agents 100 --ticks 100000"
run_benchmark "multi_symbol" "--symbols 10 --agents 100 --ticks 100000"
run_benchmark "high_load" "--symbols 100 --agents 100 --ticks 50000"
run_benchmark "stress" "--symbols 10 --agents 1000 --ticks 10000"

echo "=== Summary ==="
echo "Results saved to: $OUTPUT_DIR"
ls -la "$OUTPUT_DIR"/*_${TIMESTAMP}.*

echo ""
echo "=== Complete ==="
