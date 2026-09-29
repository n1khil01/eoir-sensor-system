#!/usr/bin/env bash
# Week 7 soak-run debugging session: samples the running pipeline with ps,
# /proc/<pid>/status, and netstat/ss on an interval, and does a liveness
# check with `kill -0` each round -- the real-Linux-debugging-tools
# deliverable PLAN.md calls for, run alongside an overnight soak.
#
# Usage (on the Pi, from the repo root):
#   ./eoir_sensor_system --threaded 28800 5050 &   # 8-hour run, background
#   PIPELINE_PID=$!
#   ./tools/week7_soak_session/monitor_soak.sh "$PIPELINE_PID" 5050 \
#       > benchmarks/week7_soak_session.txt
#
# Sampling interval defaults to 300s (5 min); override with a 3rd argument.
# Stop the script with Ctrl+C once the pipeline itself has exited (or once
# you've collected enough samples) -- it does not kill the pipeline.
set -u

PID="${1:?usage: monitor_soak.sh <pid> <port> [interval_seconds]}"
PORT="${2:?usage: monitor_soak.sh <pid> <port> [interval_seconds]}"
INTERVAL="${3:-300}"

echo "# Week 7 soak session: monitoring PID $PID on port $PORT every ${INTERVAL}s"
echo "# start: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "# columns: elapsed_s epoch_s vmrss_kb threads state alive netstat_line"

start_epoch=$(date +%s)
sample_num=0

while true; do
    now_epoch=$(date +%s)
    elapsed=$((now_epoch - start_epoch))

    # kill -0 sends no signal; it only checks the PID still exists and is
    # ours to signal -- the standard non-destructive liveness check.
    if kill -0 "$PID" 2>/dev/null; then
        alive=1
    else
        alive=0
    fi

    if [ -r "/proc/$PID/status" ]; then
        vmrss=$(awk '/VmRSS/ {print $2}' "/proc/$PID/status")
        threads=$(awk '/Threads/ {print $2}' "/proc/$PID/status")
        state=$(awk '/State/ {print $2, $3}' "/proc/$PID/status")
    else
        vmrss="NA"; threads="NA"; state="NA"
    fi

    # Prefer ss (netstat is often absent on minimal Pi images); fall back
    # if ss isn't available either.
    if command -v ss >/dev/null 2>&1; then
        netstat_line=$(ss -tnp 2>/dev/null | grep ":$PORT" || echo "no-socket")
    elif command -v netstat >/dev/null 2>&1; then
        netstat_line=$(netstat -tnp 2>/dev/null | grep ":$PORT" || echo "no-socket")
    else
        netstat_line="ss/netstat-not-found"
    fi

    ps_line=$(ps -o pid,ppid,stat,%cpu,%mem,etime,cmd -p "$PID" 2>/dev/null | tail -n +2)

    printf 'SAMPLE %d | elapsed=%ss epoch=%s vmrss_kb=%s threads=%s state="%s" alive=%s\n' \
        "$sample_num" "$elapsed" "$now_epoch" "$vmrss" "$threads" "$state" "$alive"
    printf '  ps:      %s\n' "${ps_line:-<process not found>}"
    printf '  netstat: %s\n' "$netstat_line"

    sample_num=$((sample_num + 1))

    if [ "$alive" -eq 0 ]; then
        echo "# process $PID no longer running -- stopping monitor at elapsed=${elapsed}s"
        break
    fi

    sleep "$INTERVAL"
done

echo "# end: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
