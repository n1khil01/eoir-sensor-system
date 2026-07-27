# Engineering log

STAR-format entries for real bugs, written the same week they were solved. See the "STAR story capture" section of `EOIR_Sensor_Project_Plan.md` for the shape and rules. Target six to eight entries by Week 8; the ones below are pre-filled with the known facts (symptoms, commits) — the narrative and reflection lines are marked `TODO` for you to fill in from memory while it's still fresh.

---

## Entry 1: Stale, byte-identical frame reads (Week 2)

- **Situation:** TODO — describe the observable symptom as you first saw it (known fact: the center-pixel liveliness probe added in `562bbee` showed the same raw value across many consecutive frames while the scene was visibly changing in front of the sensor).
- **Task:** TODO — what specifically had to be true before you could call the sensor link trustworthy, and any constraint you were under (e.g. no logic analyzer, working off the datasheet alone).
- **Action:** TODO — the diagnostic sequence in order, including hypotheses tried and discarded first (wiring? refresh-rate timing? polling too fast?) before landing on the real cause: the split write-then-read I2C sequence let other bus traffic interleave between the register-address write and the data read.
- **Result:** Fixed by replacing the split sequence with a single combined `I2C_RDWR` transaction (`a84d789`). Confirmed by the distinct-value count on the center-pixel probe going from 1 (frozen) to 18 across a 100-frame run — see `benchmarks/week2_sensor_communication.txt`. TODO — one line on what you'd do differently or what it changed about how you approach I2C work going forward.

---

## Entry 2: Signed/unsigned pixel-wrap corrupting min/max (Week 2)

- **Situation:** TODO — describe the observable symptom (known fact: min/max values computed from the frame looked implausible/corrupted for pixels that should have read as small negative values).
- **Task:** TODO — what had to be true to trust the min/max calculation.
- **Action:** TODO — the diagnostic sequence and any discarded hypotheses before identifying the real cause: raw pixel words are two's-complement `int16_t` on the wire, but reading them as `uint16_t` wrapped small negative values up near 65535.
- **Result:** Fixed across `410f791` (center calculation) and `8d68f4a` (min/max calculation, plus a clarifying comment in `src/main.cpp`). TODO — the confirming check you ran, and one line on the takeaway (e.g. always check the datasheet's word encoding before doing arithmetic on raw register data).

---

## Entry 3: [Week 3] "I found where the time was actually going"

- **Situation:** TODO — what you expected the capture/process split to look like before profiling (the plan's framing: "I expected the I2C read to be 90% of the budget").
- **Task:** Establish the real per-frame time budget with `std::chrono::steady_clock` instrumentation and `perf` before deciding anything about Week 4's threading design.
- **Action:** TODO — record actual commands run, e.g.:
  ```bash
  ./build/eoir_sensor_system            # optimized path -> benchmarks/week3_frame_timing.csv
  ./build/eoir_sensor_system --naive    # naive baseline -> benchmarks/week3_frame_timing_naive.csv
  perf record -g ./build/eoir_sensor_system && perf report --stdio
  perf stat -e task-clock,cycles,instructions,cache-misses ./build/eoir_sensor_system
  ```
  Note what the flame graph/perf stat actually showed (e.g. where cycles landed if it wasn't dominated by the I2C read).
- **Result:** TODO — the actual mean/p95/p99 numbers for both runs (from the CSVs and console output), the % reduction from naive to optimized, and the commit hash of the pipeline as measured. One line on what surprised you.

---

## Template for future entries

```markdown
## Entry N: <short title> (Week X)

- **Situation:**
- **Task:**
- **Action:**
- **Result:**
```
