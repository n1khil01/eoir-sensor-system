# Week 4 concurrency bug repro

`include/ring_buffer.hpp` already uses the predicate-loop form of
`condition_variable::wait`, which is the correct pattern -- so the
production pipeline doesn't organically hit the "if instead of predicate
loop" defect the way a first draft of this code would have. This directory
proves *why* that form is required by deliberately breaking it, per the
Week 4 STAR-story tooling note in `PLAN.md` (ThreadSanitizer, `gdb`/`lldb`
thread backtraces, forcing contention). See `docs/engineering-log.md` for
the full write-up.

- `buggy_ring_buffer.hpp` -- copy of `RingBuffer` with `Pop()` changed to
  `if (count_ == 0) not_empty_.wait(lock);` instead of the predicate-loop
  form used in the real header.
- `repro_driver.cpp` -- one producer, two consumers, hammering a
  capacity-8 buffer with 200,000 items; builds a `BuggyRingBuffer` or the
  real `RingBuffer` depending on the `buggy`/`fixed` argument.

## Build and run

```bash
clang++ -std=c++17 -g -O1 -fsanitize=thread \
    -I../../include tools/week4_concurrency_repro/repro_driver.cpp \
    -o repro_tsan

TSAN_OPTIONS="history_size=7 halt_on_error=1" ./repro_tsan buggy   # hangs
./repro_tsan fixed                                                 # clean
```

On a hang: `lldb -p $(pgrep repro_tsan) -o "thread backtrace all" -o detach -o quit`
(or `gdb -p <pid> -ex "thread apply all bt" -ex detach -ex quit` on Linux).
