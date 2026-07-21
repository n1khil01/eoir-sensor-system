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

## Week-by-week timeline

### Week 1: Environment and repo foundation
Order hardware immediately since shipping can consume most of a week. While waiting, set up the Linux development environment, initialize the Git repository, and build a clean CMake structure. Configure the skeleton so an empty build compiles and runs. Set up the GitHub Actions workflow file early even before there is much to test.

**Deliverable:** a compiling skeleton project in a public repo with CI running.

### Week 2: Sensor communication
Once hardware arrives, establish reliable communication with the sensor over I2C or USB. Write a minimal capture routine that pulls raw frame data and confirms the connection is stable across many reads. Handle the failure cases early, since sensor wiring and driver quirks are where time gets lost.

**Deliverable:** raw frames flowing from the sensor into a simple C++ test harness.

### Week 3: C++ architecture and the core loop
Design the sensor abstraction properly. Create a base sensor interface class and a derived class for your specific hardware, demonstrating inheritance and polymorphism. Build the single-threaded capture-to-process loop first and instrument it with timing measurements to establish your per-frame budget, for example 100 milliseconds.

**Deliverable:** a working single-threaded pipeline with measured, documented timing.

### Week 4: Multithreading and the ring buffer
Split capture and processing onto separate threads. Hand-build a ring buffer for frames rather than using a standard container, so you have a real data-structure story. Protect it with a mutex and use a producer-consumer pattern with a semaphore or condition variable. This is the heart of the synchronization story, so build it deliberately and be able to explain every locking decision.

**Deliverable:** a stable multithreaded pipeline with a custom, thread-safe buffer.

### Week 5: Networking layer
Add a TCP server that streams detection events to a connecting client. Send a structured payload such as JSON when the processing thread flags motion or a heat signature. Optionally layer a minimal REST endpoint on top so you can speak to HTTP as well as raw TCP/IP.

**Deliverable:** a client can connect over TCP and receive live detection events.

### Week 6: FreeRTOS component
Move the time-critical sensor polling onto the STM32 running FreeRTOS, with the Pi handling higher-level processing. Structure it as a clear task with a defined priority so you can discuss scheduling, context switching, and deterministic timing. If full hardware integration threatens the timeline, build this as a standalone FreeRTOS module that demonstrates the concepts without wiring it into the main pipeline.

**Deliverable:** a FreeRTOS task demonstrating priority-based real-time scheduling.

### Week 7: Testing and validation
Write Google Test coverage for the processing logic and the ring buffer. Run Valgrind and AddressSanitizer to find and fix memory leaks in the buffer and threading code. Deliberately introduce a race condition, catch it with ThreadSanitizer, then fix it, and document the whole sequence. Capture a real Linux debugging session using ps, kill, netstat, and proc against the running application.

**Deliverable:** a passing test suite plus a written record of the memory, concurrency, and debugging work.

### Week 8: Documentation, demo, and buffer
Write a concise design document covering the architecture, the timing budget decisions, the synchronization approach, and the test results. Record a short demo video or GIF of live detection. Clean the README so a stranger can build and run the project in under five minutes. Reserve remaining time as slack, since something will need it.

**Deliverable:** a polished, documented, demonstrable project ready to link in an email.

---

## What to cut if the timeline slips

Protect the core in this priority order. The multithreaded C++ pipeline with the custom buffer, the testing and debugging work, and the documentation are non-negotiable, since they carry most of the interview value. The TCP networking layer is high value but can be simplified to raw socket streaming without the REST layer. The FreeRTOS component can drop to a standalone conceptual module rather than a fully integrated one. A finished, well-documented smaller project always beats an ambitious half-finished one when it gets evaluated.

---

## Honest expectations

This project will not make you look like an engineer with production embedded experience, and it should not try to. It cannot touch classified constraints, safety-critical standards, or the actual hardware Raytheon works with. What it does is prove trajectory and initiative in exactly the areas they named, and give you specific, credible answers instead of textbook ones. For an entry-level defense role graded on direction and demonstrated initiative, that is the right target, and it puts you ahead of most candidates who arrive with coursework alone.
