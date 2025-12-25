#!/bin/bash
# Generate flamegraph for performance analysis
# Requires: perf, FlameGraph tools (https://github.com/brendangregg/FlameGraph)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
FLAMEGRAPH_DIR="${FLAMEGRAPH_DIR:-$HOME/FlameGraph}"

if [ $# -lt 1 ]; then
    echo "Usage: $0 <binary> [args...]"
    echo "Example: $0 ./bin/exchange_sim --symbols 10 --ticks 100000"
    exit 1
fi

BINARY="$1"
shift
ARGS="$@"

# Check for perf
if ! command -v perf &> /dev/null; then
    echo "Error: perf not found. Install with: sudo apt-get install linux-tools-generic"
    exit 1
fi

# Check for FlameGraph
if [ ! -d "$FLAMEGRAPH_DIR" ]; then
    echo "FlameGraph not found at $FLAMEGRAPH_DIR"
    echo "Clone it with: git clone https://github.com/brendangregg/FlameGraph.git ~/FlameGraph"
    exit 1
fi

# Create output directory
OUTPUT_DIR="$PROJECT_DIR/data/perf"
mkdir -p "$OUTPUT_DIR"

TIMESTAMP=$(date +%Y%m%d_%H%M%S)
PERF_DATA="$OUTPUT_DIR/perf_${TIMESTAMP}.data"
PERF_FOLDED="$OUTPUT_DIR/perf_${TIMESTAMP}.folded"
FLAMEGRAPH="$OUTPUT_DIR/flamegraph_${TIMESTAMP}.svg"

echo "=== Recording performance data ==="
echo "Binary: $BINARY"
echo "Args: $ARGS"
echo ""

# Record with perf (adjust frequency as needed)
sudo perf record -F 997 -g --call-graph dwarf -o "$PERF_DATA" -- "$BINARY" $ARGS

echo ""
echo "=== Generating flamegraph ==="

# Generate flamegraph
sudo perf script -i "$PERF_DATA" | \
    "$FLAMEGRAPH_DIR/stackcollapse-perf.pl" > "$PERF_FOLDED"

"$FLAMEGRAPH_DIR/flamegraph.pl" "$PERF_FOLDED" > "$FLAMEGRAPH"

echo ""
echo "=== Results ==="
echo "Perf data: $PERF_DATA"
echo "Folded stacks: $PERF_FOLDED"
echo "Flamegraph: $FLAMEGRAPH"
echo ""
echo "Open flamegraph in browser: firefox $FLAMEGRAPH"
