#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#include "frame_processor.hpp"
#include "mlx90640_raw.hpp"
#include "ring_buffer.hpp"
#include "rss_sampler.hpp"
#include "sensor_interface.hpp"
#include "timing_stats.hpp"

namespace {
constexpr int kNumTimedFrames = 1000;
// Mlx90640Raw now configures the sensor for 8 Hz on construction (see
// mlx90640_raw.hpp) instead of running at its 2 Hz power-on default, so a
// 125 ms frame period is the per-frame budget the pipeline is measured
// against. This requires the I2C bus running at fast mode (400kHz) or
// faster -- see the RefreshRate comment in mlx90640_raw.hpp.
constexpr double kFrameBudgetUs = 125'000.0;

using Frame = std::array<uint16_t, ISensor::kFrameWords>;

// Week 3's p99 total per-frame time was well under the ~500ms sensor
// period, so the processing side is never the bottleneck; a handful of
// slots is enough to absorb normal scheduling jitter between the capture
// and process threads without the buffer ever running deep. Sized generously
// relative to that measured headroom rather than guessed.
constexpr size_t kRingBufferCapacity = 8;

int RunThreaded(std::unique_ptr<ISensor> sensor, int duration_seconds) {
    std::cout << "EOIR sensor system - Week 4 multithreaded pipeline "
                 "(capture thread -> ring buffer -> process thread)\n";
    std::cout << "Running for " << duration_seconds << "s...\n";

    RingBuffer<Frame, kRingBufferCapacity> ring;
    FrameProcessor processor;

    std::atomic<bool> stop{false};
    std::atomic<uint64_t> frames_captured{0};
    std::atomic<uint64_t> frames_processed{0};
    std::atomic<uint64_t> capture_failures{0};

    // Only the capture thread writes push_block_us and only the process
    // thread writes process_us, so neither vector needs its own lock.
    std::vector<double> push_block_us;
    std::vector<double> process_us;
    push_block_us.reserve(100000);
    process_us.reserve(100000);

    std::filesystem::create_directories("benchmarks");
    std::ofstream rss_csv("benchmarks/week4_rss_samples.csv");
    rss_csv << "elapsed_s,rss_kb,frames_captured,frames_processed,"
               "ring_overflow_count\n";

    std::thread capture_thread([&] {
        Frame frame{};
        while (!stop.load(std::memory_order_relaxed)) {
            bool ok = sensor->CaptureFrame(frame);
            if (!ok) {
                capture_failures.fetch_add(1, std::memory_order_relaxed);
                continue;
            }
            double block_us = ring.Push(frame);
            push_block_us.push_back(block_us);
            frames_captured.fetch_add(1, std::memory_order_relaxed);
        }
    });

    std::thread process_thread([&] {
        FrameResult result;
        while (!stop.load(std::memory_order_relaxed) || ring.Size() > 0) {
            Frame frame = ring.Pop();
            auto start = std::chrono::steady_clock::now();
            processor.Process(frame, result);
            auto end = std::chrono::steady_clock::now();
            process_us.push_back(
                std::chrono::duration<double, std::micro>(end - start)
                    .count());
            frames_processed.fetch_add(1, std::memory_order_relaxed);
        }
    });

    auto run_start = std::chrono::steady_clock::now();
    int elapsed_s = 0;
    while (elapsed_s < duration_seconds) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        ++elapsed_s;
        rss_csv << elapsed_s << "," << CurrentRssKb() << ","
                << frames_captured.load() << "," << frames_processed.load()
                << "," << ring.overflow_count() << "\n";
        rss_csv.flush();
    }

    stop.store(true, std::memory_order_relaxed);
    // Processing thread blocks in ring.Pop() on an empty buffer; push one
    // more no-op wake so it can observe `stop` and exit its wait predicate.
    ring.Push(Frame{});
    capture_thread.join();
    process_thread.join();

    double actual_seconds = std::chrono::duration<double>(
                                 std::chrono::steady_clock::now() - run_start)
                                 .count();

    LatencyStats block_stats = ComputeLatencyStats(push_block_us);
    LatencyStats process_stats = ComputeLatencyStats(process_us);

    double fps = static_cast<double>(frames_processed.load()) / actual_seconds;

    std::cout << "\nResult after " << actual_seconds << "s:\n";
    std::cout << "  frames captured:  " << frames_captured.load() << "\n";
    std::cout << "  frames processed: " << frames_processed.load() << "\n";
    std::cout << "  capture failures: " << capture_failures.load() << "\n";
    std::cout << "  sustained throughput: " << fps << " fps\n";
    std::cout << "  ring buffer overflow count (producer outran consumer): "
              << ring.overflow_count() << "\n";
    std::cout << "  producer critical-section time (us): mean="
              << block_stats.mean_us << " p95=" << block_stats.p95_us
              << " p99=" << block_stats.p99_us << "\n";
    std::cout << "  process stage (us): mean=" << process_stats.mean_us
              << " p95=" << process_stats.p95_us
              << " p99=" << process_stats.p99_us << "\n";
    std::cout << "  RSS samples written to benchmarks/week4_rss_samples.csv\n";

    return 0;
}
}  // namespace

int main(int argc, char** argv) {
    // --naive re-runs the loop through FrameProcessor::ProcessNaive, the
    // deliberately unoptimized per-frame-allocation baseline, so its CSV
    // can be diffed against the default optimized run for the Week 3
    // "reduced processing cost vs. first version" metric.
    bool naive = argc > 1 && std::strcmp(argv[1], "--naive") == 0;
    bool live = argc > 1 && std::strcmp(argv[1], "--live") == 0;
    bool threaded = argc > 1 && std::strcmp(argv[1], "--threaded") == 0;
    int threaded_duration_s = 600;  // 10-minute soak run by default
    if (threaded && argc > 2) {
        threaded_duration_s = std::atoi(argv[2]);
    }
    const char* timing_csv_path = naive
        ? "benchmarks/week3_frame_timing_naive.csv"
        : "benchmarks/week3_frame_timing.csv";

    std::cout << "EOIR sensor system - Week 3 single-threaded capture-to-process pipeline"
              << (naive ? " (naive baseline)\n" : "\n");

    try {
        // ISensor is the base interface (polymorphism); Mlx90640Raw is the
        // concrete derived sensor. The pipeline below only ever talks to
        // the base pointer.
        std::unique_ptr<ISensor> sensor =
            std::make_unique<Mlx90640Raw>("/dev/i2c-1");
        sensor->CheckConnection();
        std::cout << "MLX90640 responded on the I2C bus.\n";

        if (threaded) {
            return RunThreaded(std::move(sensor), threaded_duration_s);
        }

        FrameProcessor processor;

        // Preallocated once, outside the loop, and passed by reference on
        // every iteration so the timed pipeline never allocates per frame.
        std::array<uint16_t, ISensor::kFrameWords> frame{};
        FrameResult result;

        // --live is an ad hoc manual test, not a timed benchmark: capture
        // and print continuously at the sensor's own cadence so waving a
        // hand in front of it is visible in the terminal immediately.
        if (live) {
            std::cout << "Live mode: printing min/max/mean and detection "
                          "flag at each frame. Ctrl+C to stop.\n";
            while (true) {
                bool ok = sensor->CaptureFrame(frame);
                if (!ok) {
                    std::cout << "  capture failed\n";
                } else {
                    processor.Process(frame, result);
                    std::cout << "min=" << result.min_value
                              << " max=" << result.max_value
                              << " mean=" << result.mean_value
                              << " heat_signature_detected="
                              << (result.heat_signature_detected ? "true"
                                                                  : "false")
                              << "\n";
                }
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(500));
            }
        }

        // benchmarks/ is gitignored, so it never gets materialized by a
        // fresh clone -- create it explicitly rather than silently
        // failing to write the CSV.
        std::filesystem::create_directories(
            std::filesystem::path(timing_csv_path).parent_path());

        std::ofstream csv(timing_csv_path);
        if (!csv.is_open()) {
            throw std::runtime_error(
                std::string("failed to open ") + timing_csv_path +
                " for writing");
        }
        csv << "frame,capture_us,process_us,total_us\n";

        std::vector<double> capture_us;
        std::vector<double> process_us;
        std::vector<double> total_us;
        capture_us.reserve(kNumTimedFrames);
        process_us.reserve(kNumTimedFrames);
        total_us.reserve(kNumTimedFrames);

        int failures = 0;
        for (int i = 0; i < kNumTimedFrames; ++i) {
            auto capture_start = std::chrono::steady_clock::now();
            bool ok = sensor->CaptureFrame(frame);
            auto capture_end = std::chrono::steady_clock::now();
            if (!ok) {
                ++failures;
                continue;
            }

            if (naive) {
                result = processor.ProcessNaive(frame);
            } else {
                processor.Process(frame, result);
            }
            auto process_end = std::chrono::steady_clock::now();

            double c_us = std::chrono::duration<double, std::micro>(
                              capture_end - capture_start)
                              .count();
            double p_us = std::chrono::duration<double, std::micro>(
                              process_end - capture_end)
                              .count();
            double t_us = c_us + p_us;

            capture_us.push_back(c_us);
            process_us.push_back(p_us);
            total_us.push_back(t_us);
            csv << i << "," << c_us << "," << p_us << "," << t_us << "\n";

            if (i % 100 == 0) {
                std::cout << "  frame " << i << ": capture=" << c_us
                          << "us process=" << p_us << "us total=" << t_us
                          << "us\n";
            }
        }
        csv.close();

        LatencyStats capture_stats = ComputeLatencyStats(capture_us);
        LatencyStats process_stats = ComputeLatencyStats(process_us);
        LatencyStats total_stats = ComputeLatencyStats(total_us);

        std::cout << "\nResult: " << (kNumTimedFrames - failures)
                  << " timed frames, " << failures << " failures out of "
                  << kNumTimedFrames << ".\n";
        std::cout << "Capture   (us): mean=" << capture_stats.mean_us
                  << " p95=" << capture_stats.p95_us
                  << " p99=" << capture_stats.p99_us << "\n";
        std::cout << "Process   (us): mean=" << process_stats.mean_us
                  << " p95=" << process_stats.p95_us
                  << " p99=" << process_stats.p99_us << "\n";
        std::cout << "Total     (us): mean=" << total_stats.mean_us
                  << " p95=" << total_stats.p95_us
                  << " p99=" << total_stats.p99_us << "\n";
        std::cout << "Headroom at p99 against " << kFrameBudgetUs
                  << "us (2 Hz) budget: "
                  << (100.0 * total_stats.p99_us / kFrameBudgetUs)
                  << "% of budget consumed.\n";
        std::cout << "Raw per-frame timings written to " << timing_csv_path
                  << "\n";

        return failures == 0 ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "Sensor communication failed: " << e.what() << "\n";
        std::cerr << "Check wiring (3.3V/GND/SDA/SCL), that I2C is enabled "
                      "(raspi-config), and run 'i2cdetect -y 1' to confirm "
                      "the device appears at 0x33.\n";
        return 1;
    }
}
