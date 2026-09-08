#!/usr/bin/env bash
# Week 5 concurrent-client test: N clients connect to one bench_server run
# and each independently verifies its own sequence stream for gaps.
# Usage: run_concurrent_clients.sh <port> <num_events> <period_ms> <num_clients>
set -euo pipefail
PORT="${1:-5053}"
NUM_EVENTS="${2:-500}"
PERIOD_MS="${3:-50}"
NUM_CLIENTS="${4:-5}"

mkdir -p benchmarks
./build/bench_server "$PORT" "$NUM_EVENTS" "$PERIOD_MS" > /tmp/bench_concurrent.log 2>&1 &
BENCH_PID=$!
sleep 0.5

PIDS=()
for i in $(seq 1 "$NUM_CLIENTS"); do
    ./build/latency_client 127.0.0.1 "$PORT" "$NUM_EVENTS" \
        "benchmarks/week5_client${i}.csv" > "/tmp/client${i}.log" 2>&1 &
    PIDS+=($!)
done

for pid in "${PIDS[@]}"; do
    wait "$pid"
done
wait "$BENCH_PID"

echo "=== bench server ==="
cat /tmp/bench_concurrent.log
for i in $(seq 1 "$NUM_CLIENTS"); do
    echo "=== client $i ==="
    cat "/tmp/client${i}.log"
done
