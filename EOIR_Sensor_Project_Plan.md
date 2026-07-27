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

## STAR story capture

Metrics get you past the resume screen. Stories get you through the interview. The question that decides an entry-level technical loop is almost always some version of *"tell me about a difficult bug and how you tracked it down"* — and the difference between a candidate who has an answer and one who does not is rarely talent. It is whether they wrote it down at the time.

You will not remember this later. The specific symptom, the two hypotheses you eliminated first, the exact command whose output finally made it obvious — that detail is vivid for about three days and then compresses into "I had some I2C issues, but I fixed it," which is worth nothing in a room. So capture stories contemporaneously, in the same week the debugging happens.

**Where it lives.** Create `docs/engineering-log.md`. One entry per real problem, written the day you solve it, in this shape:

- **Situation** — what was happening, with the observable symptom. Not "the sensor didn't work." Something like "every third frame returned byte-identical pixel data while the scene was visibly changing."
- **Task** — what specifically had to be true for you to move on, and what constraint you were working under.
- **Action** — the diagnostic sequence, in order, *including the hypotheses you tested and discarded.* Name the tool and paste the actual command. This is the section interviewers dig into, and the discarded hypotheses are what make it sound like debugging instead of guessing.
- **Result** — the fix, the measurement that confirmed it, the commit hash, and one line on what you would do differently or what it changed about how you work.

**Two rules that make these usable.** First, name tools and commands exactly — "I used a profiler" is a non-answer, while "I ran `perf record -g` on the capture thread and the flame graph showed 60% of cycles inside `memcpy`" is the answer. Second, write down the wrong turns. A story where you went straight to the answer sounds rehearsed and teaches the interviewer nothing about how you think; a story where you spent two hours convinced it was the wiring before `i2cdetect` cleared it sounds like the job.

**Your commit messages are the backup index.** Messages like `Fix stale frame reads by using a combined I2C_RDWR transaction` are already doing this work — `git log --oneline` reconstructs the arc even when the log entry is thin. Keep writing them that way, and reference the hash in every entry.

Target six to eight solid entries by Week 8. Three will be strong enough to tell in an interview. The weeks below flag the moments most likely to generate one, but the best stories are unplanned — when something breaks that is not on this list, that is the entry worth writing.

**A note on Week 2, which you have already finished.** Two stories are sitting in your history right now and the details are still fresh enough to recover — write them up before starting Week 3, because in a month they are gone. The first is the stale-frame bug: the split write-then-read I2C sequence let other bus traffic interleave between the register-address write and the data read, and the fix was the combined `I2C_RDWR` transaction in `a84d789`. Reconstruct how you noticed it — that the center-pixel liveliness check in `562bbee` was what made it visible is itself the point of the story, since building the instrument that exposes a bug is a stronger narrative than stumbling onto it. The second is smaller but interviews well: the signed/unsigned defect behind `410f791` and `8d68f4a`, where raw pixel words are two's-complement `int16_t` on the wire and reading them as `uint16_t` wrapped small negative values up near 65535, corrupting the min/max calculation. It is a clean, specific C++ type-correctness story of exactly the kind a C++ interviewer enjoys, and the comment explaining it is already in `src/main.cpp`.

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

**STAR story to capture — "I found where the time was actually going":**

The likely story this week is a performance surprise. You will instrument the loop expecting the sensor read to dominate, and something else will — a per-frame allocation, a copy you did not know was happening, an accidental deep copy through a `std::function` or a by-value parameter. That gap between where you assumed the time went and where it actually went is the story, and it is a good one because the resolution is measurement rather than intuition.

Write the entry the moment the profile surprises you. Capture the *predicted* breakdown before you profile — a story that opens "I expected the I2C read to be 90% of the budget, and it was 40%" is immediately more interesting than one that opens with the answer.

**Tools and commands:**

```bash
perf record -g ./build/eoir_sensor_system && perf report --stdio
```

```bash
perf stat -e task-clock,cycles,instructions,cache-misses ./build/eoir_sensor_system
```

- `std::chrono::steady_clock` around each stage, written to CSV — your primary instrument. Use `steady_clock`, never `system_clock`, since the latter can jump under NTP and silently corrupt a timing run.
- `perf record -g` plus `perf report` to find where cycles actually land; add `--call-graph=dwarf` if the default frame-pointer unwind gives you a useless stack.
- `valgrind --tool=callgrind` when `perf` is unavailable or you want exact instruction counts rather than sampled ones.
- `strace -c -f ./build/eoir_sensor_system` to count syscalls per frame — an unexpectedly high `ioctl` or `read` count against the I2C device is a common and very explainable finding.
- `-fno-omit-frame-pointer` and `-g` in your profiling build, or the stacks will be unreadable. Note that you must profile a `-O2` build; timing a debug build produces numbers that mean nothing.
- `git log --oneline` and the commit hash of the fix, recorded in the log entry.

**The detail that makes this story land:** state the before and after numbers with the percentile, and name the specific mechanism. "Cut p99 per-frame latency from 41 ms to 12 ms by hoisting a per-frame heap allocation out of the loop into a preallocated buffer, confirmed by re-running the same 1,000-frame timing harness" is a complete answer that closes off the obvious follow-ups.

### Week 4: Multithreading and the ring buffer
Split capture and processing onto separate threads. Hand-build a ring buffer for frames rather than using a standard container, so you have a real data-structure story. Protect it with a mutex and use a producer-consumer pattern with a semaphore or condition variable. This is the heart of the synchronization story, so build it deliberately and be able to explain every locking decision.

**Deliverable:** a stable multithreaded pipeline with a custom, thread-safe buffer.

**Metrics to capture:**

The strongest week on the whole plan for a resume bullet. Concurrency work with a measured before and after is exactly what an embedded hiring manager is scanning for.

- Increased **sustained end-to-end throughput in frames per second**, as measured by a frame counter over a continuous 10-minute run compared directly against the Week 3 single-threaded baseline, by splitting capture and processing onto separate threads communicating through the custom ring buffer. Report it as "from X fps to Y fps" — the baseline is what makes it credible.
- Achieved **zero dropped frames across a sustained run**, as measured by a dedicated overflow counter incremented inside the ring buffer whenever the producer outruns the consumer, by sizing the buffer against the Week 3 p99 processing time and blocking the producer on a condition variable rather than silently overwriting. If the counter is not zero, report the actual drop rate under overload — a measured drop rate is a better answer than a claim of perfection.
- Reduced **p99 producer blocking time**, as measured by timing the critical section under `perf` or `std::chrono` and comparing against a naive implementation, by narrowing the lock to an index update and a move rather than holding the mutex across a full frame copy. This is the number that proves you understood *why* the locking is structured the way it is, which is the actual question behind every synchronization interview prompt.
- Demonstrated **stable memory under sustained concurrency**, as measured by RSS sampled from `/proc/<pid>/status` at intervals across the 10-minute run, by preallocating the ring buffer once at startup. A flat RSS line over a long run is a leak-free claim you can show rather than assert.

**STAR story to capture — the concurrency bug (this is the one to get right):**

If you tell one story in the interview, make it this week's. Concurrency bugs are the ones working engineers respect, because everyone who has written threaded code has been humbled by one. You are close to guaranteed to hit at least one of: a deadlock from lock ordering or from holding a lock across a `wait`, a lost wakeup from signaling a condition variable without holding the mutex, a spurious-wakeup bug from using `if` instead of a predicate loop on `wait`, or a torn read from an index updated outside the lock.

What makes this story strong is that these bugs are **intermittent**. Capture that honestly — "it passed a hundred runs and hung on the hundred and first" is the detail that makes the rest of the story credible, and it sets up the genuinely impressive part: that you stopped trying to reproduce it by luck and reached for a tool that finds it deterministically.

**Tools and commands:**

```bash
g++ -fsanitize=thread -g -O1 -std=c++17 ...   # then run the normal workload
```

```bash
gdb -p $(pidof eoir_sensor_system) -ex "thread apply all bt" -ex detach -ex quit
```

- **ThreadSanitizer** (`-fsanitize=thread`) is the headline tool. It finds races on executions that appear to pass, which is exactly the class of bug you cannot catch by rerunning. Tune with `TSAN_OPTIONS=history_size=7 halt_on_error=1`. Note that TSan and ASan cannot be enabled in the same build.
- **`gdb` against a hung process** is the deadlock workflow: attach by PID, run `thread apply all bt`, and read which threads are blocked in `pthread_mutex_lock` and which mutex each is holding. This single command is the answer to "how would you debug a deadlock," and having actually run it is what separates you from a candidate reciting it.
- `ps -eLf | grep eoir` or `top -H -p <pid>` to see per-thread state — an `S`-state thread that should be running, or one thread pinned at 100% CPU, localizes the problem fast.
- `cat /proc/<pid>/status` for `Threads:` and `VmRSS`, and `/proc/<pid>/task/<tid>/stack` for where a thread is parked in the kernel.
- `kill -SIGQUIT` or `gcore <pid>` to capture a core from a wedged process, then `gdb ./build/eoir_sensor_system core`.
- `perf stat -e context-switches,cpu-migrations` to show whether the threading actually bought you parallelism or just added handoff overhead — a genuinely good thing to have measured.
- Stress the system deliberately: `taskset -c 0` to force both threads onto one core, or `stress-ng --cpu 4` alongside, which surfaces timing-dependent bugs far faster than a quiet machine.

**The detail that makes this story land:** name the specific synchronization defect and why the fix is correct, not just that you added a lock. "The consumer used `if (empty) cv.wait(lock)` instead of `cv.wait(lock, [&]{ return !empty; })`, so a spurious wakeup let it read from an empty buffer. ThreadSanitizer flagged the race in seconds; the predicate form fixed it, and I re-ran the 10-minute soak clean" demonstrates that you understand the memory model rather than that you sprinkled mutexes until it stopped crashing.

### Week 5: Networking layer
Add a TCP server that streams detection events to a connecting client. Send a structured payload such as JSON when the processing thread flags motion or a heat signature. Optionally layer a minimal REST endpoint on top so you can speak to HTTP as well as raw TCP/IP.

**Deliverable:** a client can connect over TCP and receive live detection events.

**Metrics to capture:**

- Achieved **detection-to-client delivery latency at p95 over at least 1,000 events**, as measured by timestamping at the moment the processing thread flags an event and again at client receipt on a clock-synchronized host, by streaming over a persistent connection with `TCP_NODELAY` set instead of opening a socket per event. Nagle's algorithm will otherwise add tens of milliseconds to small payloads — measure it both ways and the delta is a genuinely interesting number to have on hand.
- Sustained **concurrent client connections at a measured event rate with no dropped events**, as measured by a per-client sequence number the client verifies for gaps across a sustained run, by handling each connection without blocking the processing thread. Report the client count and the events-per-second figure together; either alone is meaningless.

**STAR story to capture — "the packets said something different than the code did":**

The networking story worth telling is one where the application logs and the wire disagreed. Candidates: partial writes, because `send()` is not obligated to transmit everything you handed it and ignoring the return value truncates JSON payloads under load; message framing, where two events arrive coalesced in one `recv()` and the client's parser chokes because TCP is a byte stream and not a message stream; a `SIGPIPE` killing the process when a client disconnects mid-write; or latency that looked like your bug and turned out to be Nagle's algorithm interacting with delayed ACK.

The framing bug in particular is worth hoping for. It is the single most common real misunderstanding of TCP, the fix is a length prefix or a delimiter, and explaining it well signals that you know what the protocol actually guarantees.

**Tools and commands:**

```bash
sudo tcpdump -i any -n port 8080 -A -vv
```

```bash
ss -tnp | grep 8080          # socket state, or: netstat -tulpn | grep 8080
```

- **`tcpdump` or Wireshark** is the tool that resolves the "logs disagree with reality" class of bug — it shows you what left the machine, which is the ground truth your application logs only claim. Write a capture with `-w capture.pcap` and open it in Wireshark to follow the TCP stream.
- **`ss -tnp`** (or `netstat -tulpn`, which the interview list explicitly names) for connection state. `Send-Q` stuck nonzero means the client is not draining and your server will eventually block — a very explainable finding. Lingering `TIME_WAIT` or `CLOSE_WAIT` entries indicate sockets you are not closing.
- `nc -v localhost 8080` and `telnet` as a dumb client to isolate whether a bug is server-side or in your own client code. Always confirm which side is broken before debugging either.
- `strace -f -e trace=network ./build/eoir_sensor_system` to see the actual `send`/`recv` return values, which is how partial writes become visible.
- `lsof -p <pid>` or `ls -l /proc/<pid>/fd` to catch file-descriptor leaks across repeated client connections — a slow leak here is a great soak-test finding.
- `curl -v` if you build the optional REST endpoint, and `ip`/`ping` to rule out the network before suspecting your code.
- `setsockopt` with `TCP_NODELAY`, and `signal(SIGPIPE, SIG_IGN)` or `MSG_NOSIGNAL` on the send — both are fixes worth being able to explain rather than merely having applied.

**The detail that makes this story land:** show that you distinguished between your bug and the protocol behaving as designed. "The client's JSON parser failed intermittently under load. tcpdump showed two events arriving in a single segment — TCP had coalesced them, correctly. I had assumed one `recv()` equals one message, so I added a newline delimiter and a buffering reader on the client" is a story about understanding a protocol, which is a level above a story about fixing a crash.

### Week 6: FreeRTOS component
Move the time-critical sensor polling onto the STM32 running FreeRTOS, with the Pi handling higher-level processing. Structure it as a clear task with a defined priority so you can discuss scheduling, context switching, and deterministic timing. If full hardware integration threatens the timeline, build this as a standalone FreeRTOS module that demonstrates the concepts without wiring it into the main pipeline.

**Deliverable:** a FreeRTOS task demonstrating priority-based real-time scheduling.

**Metrics to capture:**

Determinism numbers are the highest-signal metric in this entire plan for a defense EO/IR role, because bounded worst-case timing is the thing that separates real-time work from ordinary embedded work. These apply whether you integrate with the Pi or build the standalone FreeRTOS module described above — the simulation path still produces every number below.

- Achieved **bounded scheduling jitter on the periodic polling task, reported as worst-case deviation in microseconds over at least 10,000 periods**, as measured by toggling a GPIO pin at each task entry and capturing it on a logic analyzer, or by the DWT cycle counter if no analyzer is available, by driving the task from `vTaskDelayUntil` at a fixed priority rather than `vTaskDelay`. Report worst case, not average — averaging away the tail defeats the point of measuring determinism at all.
- Quantified **worst-case task response latency under contention**, as measured by timestamping from interrupt assertion to task execution while lower-priority tasks are deliberately loaded, by assigning the polling task a strictly higher priority and keeping the ISR short. This is the concrete evidence behind any claim about preemption and context switching.
- Established **stack and RAM headroom as a percentage**, as measured by `uxTaskGetStackHighWaterMark` after a sustained run against the allocated stack depth, by sizing task stacks from measured high-water marks rather than guesswork. On a constrained MCU this reads as real embedded discipline and takes about ten minutes to collect.

**STAR story to capture — "it worked until the timing changed":**

RTOS bugs are a different animal from Linux bugs, and saying so with a concrete example is what proves the FreeRTOS work was real rather than a tutorial you followed. The candidates here: a stack overflow that corrupts an adjacent task's memory and manifests as a bizarre unrelated fault; priority inversion, where a high-priority task blocks on a mutex held by a low-priority task that a medium-priority task keeps preempting; a task that never yields and starves everything below it; a queue overflowing because the producer's period is shorter than the consumer's worst-case execution time; or calling a non-ISR-safe API from an ISR instead of the `...FromISR` variant.

The story is stronger on this hardware precisely *because* debugging is harder. There is no `gdb attach`, no `printf` you can trust in an ISR, no sanitizers. Explaining how you localized a fault without those is a better demonstration of debugging skill than anything you can do on Linux, and interviewers on embedded teams know it.

**Tools and commands:**

- **GPIO toggle plus logic analyzer or oscilloscope** — set a pin high at task entry and low at exit. This is the fundamental embedded timing instrument: it costs nothing, perturbs timing far less than instrumentation does, and directly produces your jitter and worst-case-execution-time numbers. A cheap 8-channel analyzer with PulseView or Saleae Logic is enough.
- **`vApplicationStackOverflowHook`** with `configCHECK_FOR_STACK_OVERFLOW 2`, plus `uxTaskGetStackHighWaterMark` — this is how the stack-overflow story gets caught deterministically instead of by symptom.
- **`vTaskGetRunTimeStats` and `uxTaskGetSystemState`** with `configGENERATE_RUN_TIME_STATS` enabled, for per-task CPU share. This is what makes a starvation story concrete.
- **SEGGER SystemView or Tracealyzer** for a visual timeline of task switches, preemptions, and blocking. If you catch a priority inversion, the SystemView trace *is* the evidence, and it makes the story vivid.
- **`gdb` over OpenOCD or ST-Link** (`arm-none-eabi-gdb`, `target remote localhost:3333`) for on-target breakpoints and memory inspection; STM32CubeIDE wraps this if you prefer the GUI.
- **The DWT cycle counter** (`DWT->CYCCNT`) for microsecond-resolution timing when no analyzer is available.
- **The fault handlers** — implement `HardFault_Handler` to dump the stacked registers, then `arm-none-eabi-addr2line -e firmware.elf <PC>` to turn the faulting address into a source line. Recovering a source line from a hard fault is a genuinely impressive thing to have done.
- **`configUSE_MUTEXES` with priority inheritance**, which is the *fix* for priority inversion and worth being able to explain: FreeRTOS mutexes inherit priority, binary semaphores do not, and choosing the wrong one is the bug.

**The detail that makes this story land:** identify the mechanism by name and explain why the fix addresses it. "The high-priority poll task missed its deadline intermittently. The SystemView trace showed it blocked on a semaphore held by the low-priority logger while a medium-priority task ran — textbook priority inversion. I switched the semaphore to a FreeRTOS mutex to get priority inheritance, and worst-case jitter dropped from 900 µs to under 40 µs across 10,000 periods on the analyzer" is close to an ideal embedded interview answer: named failure mode, real instrument, measured result.

### Week 7: Testing and validation
Write Google Test coverage for the processing logic and the ring buffer. Run Valgrind and AddressSanitizer to find and fix memory leaks in the buffer and threading code. Deliberately introduce a race condition, catch it with ThreadSanitizer, then fix it, and document the whole sequence. Capture a real Linux debugging session using ps, kill, netstat, and proc against the running application.

**Deliverable:** a passing test suite plus a written record of the memory, concurrency, and debugging work.

**Metrics to capture:**

Defect-detection numbers survive recruiter screening better than almost anything else here, because they are unambiguous and every reader understands them.

- Achieved **line and branch coverage across the processing logic and ring buffer, reported as a percentage over a stated number of Google Test cases**, as measured by `gcov`/`lcov` output published from the CI run, by writing tests against the buffer's boundary conditions — empty, full, single-element, wraparound — rather than only the happy path. Quote both numbers and the test count; branch coverage on a concurrent data structure is the harder and more impressive of the two.
- Eliminated **memory leaks, reported as bytes definitely lost going to zero**, as measured by Valgrind and AddressSanitizer reports committed before and after in `benchmarks/`, by fixing the ownership issues the tools surfaced in the buffer and threading code. The before report is not an embarrassment to hide — it is the evidence that the after number means something.
- Detected and fixed a **data race, reported as ThreadSanitizer warnings going from N to zero**, as measured by the TSan output on both the deliberately broken and the corrected build, by introducing the race intentionally, catching it with tooling, then correcting the synchronization. Document the sequence in full; the deliberate-introduction step is what makes this a methodology story rather than a lucky catch.
- Demonstrated **stability across a sustained soak run, reported in hours with flat RSS and zero crashes**, as measured by `/proc/<pid>/status` sampled on an interval alongside the `ps` and `netstat` observations from the documented debugging session, by running the full pipeline continuously under load. Run this overnight — it costs you nothing and converts into a reliability figure you would otherwise not have.

**STAR story to capture — the deliberate race, plus whatever the tools find:**

This week has a guaranteed story, because the plan already tells you to manufacture one: introduce a race, catch it with ThreadSanitizer, fix it, document the sequence. Tell that as a *methodology* story rather than a bug story — the point is not that you fixed a race you created, it is that you validated your tooling could catch a class of defect before trusting it. That framing is much stronger, and it is honest.

The unplanned story here is usually a leak Valgrind finds in a path you were sure was clean, or a test that passes locally and fails in CI. The CI-versus-local divergence is worth writing up carefully: different core count, different timing, no attached terminal, different compiler version. Environment-dependent failures are a real professional skill and most junior candidates have never chased one.

**Tools and commands:**

```bash
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./build/eoir_sensor_system
```

```bash
ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 ./build/eoir_sensor_system
```

- **Valgrind** as above — `--track-origins=yes` is the flag that turns "uninitialized value" into a usable answer by telling you where it came from. Add `--gen-suppressions=all` when third-party noise drowns your own findings.
- **AddressSanitizer** (`-fsanitize=address,undefined -fno-omit-frame-pointer`) for a fast build you can run constantly; Valgrind is thorough but roughly 20× slower, so use ASan during development and Valgrind before you commit a number. UBSan is nearly free to add and catches the signed-overflow and alignment issues that embedded code attracts.
- **ThreadSanitizer** for the deliberate race, per Week 4.
- **`gcov`/`lcov`** for the coverage number: build with `--coverage`, then `lcov --capture --directory build --output-file coverage.info && genhtml coverage.info --output-directory coverage-report`. Publish the summary as a CI artifact.
- **Google Test** with `--gtest_filter`, `--gtest_repeat=1000 --gtest_shuffle` to shake out order-dependent and flaky tests, and `--gtest_break_on_failure` to drop into the debugger at the failure point.
- **`gdb`** with a watchpoint (`watch some_var`) when you need to know *what* corrupted a value rather than where it was read — the technique most candidates have never used and the one that resolves memory corruption fastest.
- **The Linux debugging session the plan requires**: `ps -eLf`, `top -H`, `kill -SIGTERM` versus `-SIGKILL` to demonstrate graceful shutdown, `netstat -tulpn`/`ss`, `cat /proc/<pid>/status`, `/proc/<pid>/fd`, `/proc/<pid>/maps`, `/proc/<pid>/limits`. Do this against the live application and capture the terminal output verbatim — the plan already calls for it, and it doubles as story material.
- **`clang-tidy`** and **`cppcheck`** in CI, plus `git bisect run ./build/tests` when a regression appears and you do not know which commit introduced it. Having actually used `git bisect` is a small, specific, credible detail.
- **`ulimit -c unlimited`** and `gdb ./build/eoir_sensor_system core` for post-mortem analysis of a crash you cannot reproduce interactively.

**The detail that makes this story land:** show the tool changed your belief about the code. "I was confident the ring buffer was clean because every test passed. ThreadSanitizer flagged a race on the write index — the tests passed because a single-core test runner never interleaved the threads that way. I fixed the index update to happen under the lock and added a `--gtest_repeat=1000` stress test to CI" tells an interviewer that you know passing tests are evidence of nothing in a concurrent system, which is a genuinely senior insight to arrive at.

### Week 8: Documentation, demo, and buffer
Write a concise design document covering the architecture, the timing budget decisions, the synchronization approach, and the test results. Record a short demo video or GIF of live detection. Clean the README so a stranger can build and run the project in under five minutes. Reserve remaining time as slack, since something will need it.

**Deliverable:** a polished, documented, demonstrable project ready to link in an email.

*No new metric here. Documentation weeks do not generate statistics worth quoting, and a fabricated one ("cut onboarding time by 80%") is the kind of claim that collapses under one question. What this week does instead is consolidate: fold every row from `METRICS.md` into the design document with its measurement method attached, and lead the README with the three strongest numbers so a reader hits them before the build instructions.*

**STAR consolidation — do this before the interview, not during it:**

Go through `docs/engineering-log.md` and pick the three strongest entries. Aim for variety over drama: one concurrency story, one hardware or protocol story, one methodology story about testing or tooling. Three stories covering different skills beat three versions of the same debugging session.

For each, rehearse it out loud to about ninety seconds. This is the step people skip, and it is the one that matters — written entries are shapeless when spoken, and the failure mode in a real interview is either a thirty-second answer with no substance or a five-minute wander through irrelevant detail. Ninety seconds forces the compression: symptom, what you thought first and why you were wrong, the tool that resolved it, the measured result.

Have the log open and searchable during the conversation. When someone asks about a bug you have not rehearsed, the entry gives you the specific numbers and command names instead of a vague recollection — and being able to say "let me pull up the actual commit" is a strong signal in itself.

One last pass: check that every story ends with a *result you measured*, not just "and then it worked." The entries that trace back to a row in `METRICS.md` are the ones to lead with, because the story and the resume bullet then reinforce each other — the interviewer sees the claim on paper, hears the debugging narrative behind it, and the two match.

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

The engineering log is the same. Ten minutes writing up a bug the day you fixed it is the highest-return time in this entire plan, and it is the first thing that gets skipped when the schedule tightens. Skip a feature instead. A smaller project you can talk about in specific detail interviews far better than a larger one you half-remember.

---

## Honest expectations

This project will not make you look like an engineer with production embedded experience, and it should not try to. It cannot touch classified constraints, safety-critical standards, or the actual hardware Raytheon works with. What it does is prove trajectory and initiative in exactly the areas they named, and give you specific, credible answers instead of textbook ones. For an entry-level defense role graded on direction and demonstrated initiative, that is the right target, and it puts you ahead of most candidates who arrive with coursework alone.
