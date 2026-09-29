# Week 7 soak run + Linux debugging session

Per `PLAN.md`'s Week 7 deliverable: "capture a real Linux debugging session
using ps, kill, netstat, and proc against the running application," and the
sustained-soak-run stability metric ("reported in hours with flat RSS and
zero crashes... run this overnight").

`monitor_soak.sh` samples the running pipeline every 5 minutes (configurable)
using exactly those tools -- `/proc/<pid>/status` for RSS and thread count,
`ps` for process state, `kill -0` as the non-destructive liveness check, and
`ss`/`netstat` for the TCP listener state on the detection-event port -- and
writes one line per sample.

## Running it (on the Pi)

```bash
# From the repo root, with the pipeline already built (see main README).
./eoir_sensor_system --threaded 28800 5050 &   # 8-hour soak, background
PIPELINE_PID=$!
echo "pipeline pid: $PIPELINE_PID"

./tools/week7_soak_session/monitor_soak.sh "$PIPELINE_PID" 5050 300 \
    > benchmarks/week7_soak_session.txt
```

Leave both running overnight (SSH session under `tmux`/`screen`/`nohup` so it
survives a disconnect). At some point during the run, connect
`tools/week5_tcp_client/latency_client.cpp` briefly so a `netstat`/`ss`
sample during the session shows an `ESTABLISHED` connection on the port,
not just `LISTEN` -- that's the difference between "the socket exists" and
"the socket actually served a client" as debugging evidence.

At the end of the soak duration the pipeline exits on its own (the
`--threaded <duration>` argument is the run length in seconds); the monitor
script notices the PID is gone (`kill -0` starts failing) and stops itself.

## What to check afterward

- `grep vmrss_kb benchmarks/week7_soak_session.txt` -- first vs. last value
  should be close (flat RSS, no leak-shaped drift).
- `grep alive=0 benchmarks/week7_soak_session.txt` -- should only appear on
  the final line, at the expected elapsed time. If it appears earlier, the
  pipeline crashed mid-run and the debugging session just did its job:
  `dmesg`/`journalctl` and the pipeline's own stderr are the next places to
  look.
- The number of `SAMPLE` lines times the interval is the total monitored
  duration in hours -- that's the metric.
