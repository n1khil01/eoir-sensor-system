# Real-Time IR Sensor Processing System
### An embedded project built to map directly onto Raytheon EO/IR Software Engineer interview criteria

---

## Purpose

This project exists to demonstrate the specific skills Raytheon interviewers emphasized: real-time C++ on embedded Linux, RTOS concepts, multithreading and synchronization, buffer-based data transfer, networking, and a rigorous testing approach. Every design decision below is chosen because it produces a concrete, defensible answer to a question they are likely to ask again in a re-interview.

Target completion: 8 weeks, finished and documented by September so it can anchor a recruiter check-in email.

---

## What the system does

A C++ application runs on a Raspberry Pi under Linux. It reads frames from a thermal or IR sensor on a dedicated capture thread, passes those frames through a shared ring buffer to a processing thread that runs a motion or heat-signature detection algorithm, and streams detection events over TCP to a simple client. A companion microcontroller running FreeRTOS handles time-critical sensor polling, giving a legitimate RTOS story alongside the Linux work.

The system is deliberately simple in features and deliberately rich in the engineering concepts underneath it. The value is in the architecture and the process, not in flashy functionality.

---

## How each component maps to their interview list

| Interview topic they raised | Where it lives in this project |
|---|---|
| RTOS / VxWorks concepts | FreeRTOS task on the microcontroller handling sensor polling with priority-based scheduling |
| Linux debugging: ps, kill, netstat, proc | A documented debugging session using these tools on the running application |
| Embedded development | The whole system runs on constrained hardware talking to a real sensor |
| C++ fundamentals: struct vs class, inheritance, polymorphism, linked lists | Base sensor interface class with a derived concrete sensor, plus a hand-built ring buffer |
| Synchronization: threads, mutexes, semaphores, locks | Producer-consumer design with a mutex-protected shared buffer between capture and processing threads |
| Data transfers and buffers | The ring buffer moving frames between threads |
| Networking: TCP/IP, HTTP/HTTPS | TCP server streaming detection events, with an optional lightweight REST endpoint |
| Testing: unit, integration, memory leaks, race conditions | Google Test suite, Valgrind and AddressSanitizer for leaks, ThreadSanitizer for a deliberately introduced and then fixed race condition |

---

## Hardware and resources

**Core hardware**
- Raspberry Pi 4 or 5 with a standard Linux distribution
- MLX90640 thermal sensor over I2C, or a basic USB IR camera for simpler setup
- Optional: an STM32 development board for the FreeRTOS component
- Estimated total cost under 150 dollars

**Software and libraries**
- C++17 with a CMake build system
- Google Test for unit testing
- Valgrind, AddressSanitizer, ThreadSanitizer for memory and concurrency validation
- cppcheck or clang-tidy for static analysis
- GitHub Actions for continuous integration
- FreeRTOS for the microcontroller component

**Learning references to line up before you start**
- The MLX90640 datasheet for the I2C interface details
- FreeRTOS official documentation, specifically the sections on tasks, scheduling, and queues
- The Linux man pages for ps, kill, netstat, and the proc filesystem
- A TCP sockets primer for C++ using the Berkeley sockets API

---

## Measurement discipline

Some weeks produce a number a recruiter will stop on. Most do not, and instrumenting everything equally is how a resume ends up padded with build times. Metrics below are deliberately concentrated on **Weeks 2, 3, 4, 5, 6, and 7** — the weeks that yield latency, throughput, determinism, reliability, or defect-detection figures. Weeks 1 and 8 are scaffolding and documentation: real work, no quotable statistic, and each is marked accordingly so the omission is a decision rather than an oversight.

The pattern is **accomplished [X] as measured by [Y] by doing [Z]** — the outcome, the instrument that proves it, and the specific engineering work that caused it. A claim without an instrument is not a metric, so each block names where the number comes from: a timing log, a sanitizer report, a coverage run, a logic-analyzer capture.

Three rules keep this honest:

1. **Record a baseline before you optimize.** A number is only impact if there is a prior number to compare it against. Take the "before" measurement in the same week you write the code, not retroactively at the end.
2. **Store the raw evidence in the repo.** Create a `benchmarks/` directory and commit the actual output — timing logs, sanitizer reports, coverage summaries, `perf stat` dumps. If an interviewer asks how you measured it, you open the file.
3. **Only claim what the instrument shows.** "Sub-10 ms p99 latency measured over 10,000 frames" is defensible. "Massively improved performance" is not, and it invites the exact follow-up you cannot answer.

Keep a running `METRICS.md` at the repo root with one row per measurement: date, metric, value, how measured, commit hash. It is the raw material for the resume bullets at the end of this document.

---

## Week-by-week timeline

### Week 1: Environment and repo foundation
Order hardware immediately since shipping can consume most of a week. While waiting, set up the Linux development environment, initialize the Git repository, and build a clean CMake structure. Configure the skeleton so an empty build compiles and runs. Set up the GitHub Actions workflow file early even before there is much to test.

**Deliverable:** a compiling skeleton project in a public repo with CI running.

*No metric here. Build setup and CI scaffolding are table stakes — a build time or a warning count does not read as impact on a resume, and quoting one invites the question of why that was the highlight. The CI pipeline earns its mention in Week 7, where it enforces coverage and sanitizer gates.*

### Week 2: Sensor communication
Once hardware arrives, establish reliable communication with the sensor over I2C or USB. Write a minimal capture routine that pulls raw frame data and confirms the connection is stable across many reads. Handle the failure cases early, since sensor wiring and driver quirks are where time gets lost.

**Deliverable:** raw frames flowing from the sensor into a simple C++ test harness.

**Metrics to capture (Week 2 — already complete; backfill these from the existing harness):**

Data-integrity numbers on a real hardware bus are worth stating, because "I found and fixed a class of silently corrupt reads" is a debugging story, not a setup story.

- Achieved a **frame-read success rate across 100 consecutive captures** (target: 100/100), as measured by the success/failure tally the Week 2 harness in `src/main.cpp` already prints, by widening the frame-poll timeout to match the MLX90640's default 2 Hz refresh rate instead of assuming a faster cadence. Run it, paste the `N successful reads, M failures out of 100` line into `METRICS.md`, and that ratio is the claim.
- Eliminated **stale and duplicated frame reads**, as measured by the distinct-value count on the center-pixel liveliness probe across a 100-frame run, by replacing the split write-then-read sequence with a single combined `I2C_RDWR` transaction so no other bus traffic could interleave between the register-address write and the data read. Record the before number (repeated identical center-pixel values) and the after number (distinct values across a moving scene) — that delta is the metric.

### Week 3: C++ architecture and the core loop
Design the sensor abstraction properly. Create a base sensor interface class and a derived class for your specific hardware, demonstrating inheritance and polymorphism. Build the single-threaded capture-to-process loop first and instrument it with timing measurements to establish your per-frame budget, for example 100 milliseconds.

**Deliverable:** a working single-threaded pipeline with measured, documented timing.

**Metrics to capture:**

This is the week the project acquires a baseline. Every throughput and latency claim in Weeks 4 through 7 is measured against the numbers you take here, so take them carefully and commit the raw logs.

- Established a **per-frame latency budget with mean, p95, and p99 reported over at least 1,000 frames**, as measured by `std::chrono::steady_clock` instrumentation around the capture and process stages written to a CSV in `benchmarks/`, by building the single-threaded loop first and timing each stage separately instead of only the loop as a whole. Quote the percentile, not the mean — p99 is the number that matters in a real-time context and the one an EO/IR interviewer will ask for.
- Reduced **per-frame processing cost against your own first working version**, as measured by the same instrumentation re-run after optimization, by removing per-frame heap allocation in favor of preallocated frame buffers and passing frames by reference through the pipeline. Record both numbers; the percentage reduction is the resume line and the allocation change is the defensible explanation.
- Quantified **headroom against the sensor's frame cadence**, as measured by processing time as a percentage of the available per-frame window at the configured refresh rate, by profiling the loop before adding any concurrency. This is what justifies the Week 4 threading decision with evidence rather than instinct.

### Week 4: Multithreading and the ring buffer
Split capture and processing onto separate threads. Hand-build a ring buffer for frames rather than using a standard container, so you have a real data-structure story. Protect it with a mutex and use a producer-consumer pattern with a semaphore or condition variable. This is the heart of the synchronization story, so build it deliberately and be able to explain every locking decision.

**Deliverable:** a stable multithreaded pipeline with a custom, thread-safe buffer.

**Metrics to capture:**

The strongest week on the whole plan for a resume bullet. Concurrency work with a measured before and after is exactly what an embedded hiring manager is scanning for.

- Increased **sustained end-to-end throughput in frames per second**, as measured by a frame counter over a continuous 10-minute run compared directly against the Week 3 single-threaded baseline, by splitting capture and processing onto separate threads communicating through the custom ring buffer. Report it as "from X fps to Y fps" — the baseline is what makes it credible.
- Achieved **zero dropped frames across a sustained run**, as measured by a dedicated overflow counter incremented inside the ring buffer whenever the producer outruns the consumer, by sizing the buffer against the Week 3 p99 processing time and blocking the producer on a condition variable rather than silently overwriting. If the counter is not zero, report the actual drop rate under overload — a measured drop rate is a better answer than a claim of perfection.
- Reduced **p99 producer blocking time**, as measured by timing the critical section under `perf` or `std::chrono` and comparing against a naive implementation, by narrowing the lock to an index update and a move rather than holding the mutex across a full frame copy. This is the number that proves you understood *why* the locking is structured the way it is, which is the actual question behind every synchronization interview prompt.
- Demonstrated **stable memory under sustained concurrency**, as measured by RSS sampled from `/proc/<pid>/status` at intervals across the 10-minute run, by preallocating the ring buffer once at startup. A flat RSS line over a long run is a leak-free claim you can show rather than assert.

### Week 5: Networking layer
Add a TCP server that streams detection events to a connecting client. Send a structured payload such as JSON when the processing thread flags motion or a heat signature. Optionally layer a minimal REST endpoint on top so you can speak to HTTP as well as raw TCP/IP.

**Deliverable:** a client can connect over TCP and receive live detection events.

**Metrics to capture:**

- Achieved **detection-to-client delivery latency at p95 over at least 1,000 events**, as measured by timestamping at the moment the processing thread flags an event and again at client receipt on a clock-synchronized host, by streaming over a persistent connection with `TCP_NODELAY` set instead of opening a socket per event. Nagle's algorithm will otherwise add tens of milliseconds to small payloads — measure it both ways and the delta is a genuinely interesting number to have on hand.
- Sustained **concurrent client connections at a measured event rate with no dropped events**, as measured by a per-client sequence number the client verifies for gaps across a sustained run, by handling each connection without blocking the processing thread. Report the client count and the events-per-second figure together; either alone is meaningless.

### Week 6: FreeRTOS component
Move the time-critical sensor polling onto the STM32 running FreeRTOS, with the Pi handling higher-level processing. Structure it as a clear task with a defined priority so you can discuss scheduling, context switching, and deterministic timing. If full hardware integration threatens the timeline, build this as a standalone FreeRTOS module that demonstrates the concepts without wiring it into the main pipeline.

**Deliverable:** a FreeRTOS task demonstrating priority-based real-time scheduling.

**Metrics to capture:**

Determinism numbers are the highest-signal metric in this entire plan for a defense EO/IR role, because bounded worst-case timing is the thing that separates real-time work from ordinary embedded work. These apply whether you integrate with the Pi or build the standalone FreeRTOS module described above — the simulation path still produces every number below.

- Achieved **bounded scheduling jitter on the periodic polling task, reported as worst-case deviation in microseconds over at least 10,000 periods**, as measured by toggling a GPIO pin at each task entry and capturing it on a logic analyzer, or by the DWT cycle counter if no analyzer is available, by driving the task from `vTaskDelayUntil` at a fixed priority rather than `vTaskDelay`. Report worst case, not average — averaging away the tail defeats the point of measuring determinism at all.
- Quantified **worst-case task response latency under contention**, as measured by timestamping from interrupt assertion to task execution while lower-priority tasks are deliberately loaded, by assigning the polling task a strictly higher priority and keeping the ISR short. This is the concrete evidence behind any claim about preemption and context switching.
- Established **stack and RAM headroom as a percentage**, as measured by `uxTaskGetStackHighWaterMark` after a sustained run against the allocated stack depth, by sizing task stacks from measured high-water marks rather than guesswork. On a constrained MCU this reads as real embedded discipline and takes about ten minutes to collect.

### Week 7: Testing and validation
Write Google Test coverage for the processing logic and the ring buffer. Run Valgrind and AddressSanitizer to find and fix memory leaks in the buffer and threading code. Deliberately introduce a race condition, catch it with ThreadSanitizer, then fix it, and document the whole sequence. Capture a real Linux debugging session using ps, kill, netstat, and proc against the running application.

**Deliverable:** a passing test suite plus a written record of the memory, concurrency, and debugging work.

**Metrics to capture:**

Defect-detection numbers survive recruiter screening better than almost anything else here, because they are unambiguous and every reader understands them.

- Achieved **line and branch coverage across the processing logic and ring buffer, reported as a percentage over a stated number of Google Test cases**, as measured by `gcov`/`lcov` output published from the CI run, by writing tests against the buffer's boundary conditions — empty, full, single-element, wraparound — rather than only the happy path. Quote both numbers and the test count; branch coverage on a concurrent data structure is the harder and more impressive of the two.
- Eliminated **memory leaks, reported as bytes definitely lost going to zero**, as measured by Valgrind and AddressSanitizer reports committed before and after in `benchmarks/`, by fixing the ownership issues the tools surfaced in the buffer and threading code. The before report is not an embarrassment to hide — it is the evidence that the after number means something.
- Detected and fixed a **data race, reported as ThreadSanitizer warnings going from N to zero**, as measured by the TSan output on both the deliberately broken and the corrected build, by introducing the race intentionally, catching it with tooling, then correcting the synchronization. Document the sequence in full; the deliberate-introduction step is what makes this a methodology story rather than a lucky catch.
- Demonstrated **stability across a sustained soak run, reported in hours with flat RSS and zero crashes**, as measured by `/proc/<pid>/status` sampled on an interval alongside the `ps` and `netstat` observations from the documented debugging session, by running the full pipeline continuously under load. Run this overnight — it costs you nothing and converts into a reliability figure you would otherwise not have.

### Week 8: Documentation, demo, and buffer
Write a concise design document covering the architecture, the timing budget decisions, the synchronization approach, and the test results. Record a short demo video or GIF of live detection. Clean the README so a stranger can build and run the project in under five minutes. Reserve remaining time as slack, since something will need it.

**Deliverable:** a polished, documented, demonstrable project ready to link in an email.

*No new metric here. Documentation weeks do not generate statistics worth quoting, and a fabricated one ("cut onboarding time by 80%") is the kind of claim that collapses under one question. What this week does instead is consolidate: fold every row from `METRICS.md` into the design document with its measurement method attached, and lead the README with the three strongest numbers so a reader hits them before the build instructions.*

---

## Turning the measurements into resume bullets

By Week 8 `METRICS.md` should hold the raw numbers. Convert them at the end, not as you go — you will have better numbers by then, and the strongest bullets combine results across weeks.

A resume line keeps the same structure as the measurements but compresses it: the accomplishment leads, the measurement is embedded, and the mechanism closes it. Fill the brackets from `METRICS.md`; do not write the bullet first and hunt for a number to justify it.

- *Built a real-time thermal imaging pipeline in C++17 on embedded Linux, sustaining **[X] fps** end-to-end with **[Y] ms p99 per-frame latency** and zero dropped frames across a **[Z]-minute** run, by splitting capture and processing across threads coordinated through a hand-built, mutex-protected ring buffer.*
- *Increased processing throughput **[X]% over a measured single-threaded baseline** by restructuring the pipeline into a producer-consumer design and narrowing the critical section to an index update rather than a full frame copy.*
- *Achieved **[X]% branch coverage across [N] Google Test cases** and eliminated **[Y] memory leaks and [Z] data races**, verified by Valgrind, AddressSanitizer, and ThreadSanitizer gates enforced in CI on every push.*
- *Delivered deterministic sensor polling on an STM32 under FreeRTOS with **worst-case scheduling jitter under [X] µs across [N] periods**, measured by logic-analyzer capture, by driving a fixed-priority periodic task from `vTaskDelayUntil`.*
- *Eliminated silently corrupt sensor reads across a **100-capture validation run** by replacing a split write-then-read I2C sequence with a single combined `I2C_RDWR` transaction, verified against a per-frame liveliness probe.*

Two cautions worth internalizing before you use any of these. First, every bracket must trace to a row in `METRICS.md` with a commit hash — a bullet you cannot reproduce on request is worse than no bullet. Second, keep the mechanism clause attached. "Increased throughput 40%" invites skepticism; "increased throughput 40% by moving to a producer-consumer design with a mutex-protected ring buffer" answers the follow-up before it is asked, and that clause is where the actual engineering is visible.

---

## What to cut if the timeline slips

Protect the core in this priority order. The multithreaded C++ pipeline with the custom buffer, the testing and debugging work, and the documentation are non-negotiable, since they carry most of the interview value. The TCP networking layer is high value but can be simplified to raw socket streaming without the REST layer. The FreeRTOS component can drop to a standalone conceptual module rather than a fully integrated one. A finished, well-documented smaller project always beats an ambitious half-finished one when it gets evaluated.

One thing that does not get cut: the measurements attached to whatever you do build. Instrumentation is cheap relative to the features it measures, and a reduced-scope pipeline with hard latency and coverage numbers presents far better than a fuller one with nothing quantified. If Week 6 drops to the standalone FreeRTOS module, note that the jitter, response-latency, and stack-headroom figures are all still collectible from it — the simulation path costs you integration, not metrics.

---

## Honest expectations

This project will not make you look like an engineer with production embedded experience, and it should not try to. It cannot touch classified constraints, safety-critical standards, or the actual hardware Raytheon works with. What it does is prove trajectory and initiative in exactly the areas they named, and give you specific, credible answers instead of textbook ones. For an entry-level defense role graded on direction and demonstrated initiative, that is the right target, and it puts you ahead of most candidates who arrive with coursework alone.
