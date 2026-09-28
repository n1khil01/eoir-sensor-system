# Week 6 — FreeRTOS module

Standalone FreeRTOS demonstration of the time-critical sensor-polling story
from the top-level [`PLAN.md`](../PLAN.md) Week 6 section, built per the
plan's own fallback: *"If full hardware integration threatens the timeline,
build this as a standalone FreeRTOS module that demonstrates the concepts
without wiring it into the main pipeline."* No STM32 board was used or
needed — the target is QEMU's `mps2-an385` machine, which emulates a real
Cortex-M3 closely enough that FreeRTOS runs on it unmodified, with genuine
priority-based preemptive scheduling and real interrupt handling.

See [`app/main.c`](app/main.c) for the full design writeup in its header
comment. In short: a hardware timer (TIMER0) fires a periodic interrupt
standing in for a sensor's data-ready line; a high-priority task blocks on a
semaphore given from that ISR and times its own response; a deliberate
low-priority CPU-hog task provides contention. The build reports, over
10,000+ periods each:

- Worst-case scheduling jitter on the periodic poll.
- Worst-case ISR-to-task response latency, with and without CPU contention.
- Stack high-water mark for the polling task (`uxTaskGetStackHighWaterMark`).

## Building

You need an `arm-none-eabi-gcc` toolchain **with newlib** and QEMU with
`qemu-system-arm`.

```bash
brew install qemu
```

Homebrew's `arm-none-eabi-gcc` bottle does not bundle newlib (no
`stdlib.h`/`stdio.h` for the target), so this project uses the official ARM
GNU Toolchain instead:

```bash
curl -L -o /tmp/arm-gnu-toolchain.tar.xz \
  "https://developer.arm.com/-/media/Files/downloads/gnu/13.2.rel1/binrel/arm-gnu-toolchain-13.2.rel1-darwin-arm64-arm-none-eabi.tar.xz"
mkdir -p ~/opt && tar xf /tmp/arm-gnu-toolchain.tar.xz -C ~/opt
xattr -dr com.apple.quarantine ~/opt/arm-gnu-toolchain-13.2.Rel1-darwin-arm64-arm-none-eabi
export PATH="$HOME/opt/arm-gnu-toolchain-13.2.Rel1-darwin-arm64-arm-none-eabi/bin:$PATH"
```

(Substitute the `darwin-x86_64` tarball on Intel Macs, or the appropriate
Linux tarball on Linux — see [ARM's toolchain downloads page](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads).)

Then:

```bash
cd freertos
make            # builds build/rtos_week6.elf
make run        # runs it under QEMU, headless, and prints the report
```

## The `-icount` finding

`make run` passes `-icount shift=auto` to QEMU. This is not cosmetic —
without it, QEMU's virtual clock tracks host wall-clock time, so on a shared
dev machine (this one, not an idle CI box) the guest's own hardware timer
appeared to jitter by over a millisecond on a 500us period. That is a QEMU
host-scheduling artifact, not a property of the firmware. `-icount` ties the
guest's virtual time to instructions retired instead of wall-clock time,
which decouples it from host noise; the identical build then reports
worst-case jitter in the tens of nanoseconds. Both raw runs are kept in
[`../benchmarks/`](../benchmarks/) — `week6_qemu_wallclock_timing.txt` vs.
`week6_qemu_icount_timing.txt` — because the discrepancy between them is a
better debugging story than either number alone.

## Vendored files

`app/CMSIS/`, `app/mps2_m3.ld`, `app/startup_gcc.c`, and
`app/printf-stdarg.c`, plus everything under `kernel/`, are vendored
third-party source (MIT-licensed) from the [FreeRTOS/FreeRTOS-Kernel](https://github.com/FreeRTOS/FreeRTOS-Kernel) repo and the
`FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC` demo in [FreeRTOS/FreeRTOS](https://github.com/FreeRTOS/FreeRTOS), trimmed to the files this module
actually needs. `app/main.c`, `app/FreeRTOSConfig.h`, and the `Makefile` are
this project's own.
