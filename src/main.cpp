#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

#include "frame_processor.hpp"
#include "mlx90640_raw.hpp"
#include "sensor_interface.hpp"
#include "timing_stats.hpp"

namespace {
constexpr int kNumTimedFrames = 1000;
// MLX90640 default refresh rate is 2 Hz, i.e. a 500 ms frame period; that
// period is the per-frame budget the single-threaded loop is measured
// against.
constexpr double kFrameBudgetUs = 500'000.0;
}  // namespace

int main(int argc, char** argv) {
    // --naive re-runs the loop through FrameProcessor::ProcessNaive, the
    // deliberately unoptimized per-frame-allocation baseline, so its CSV
    // can be diffed against the default optimized run for the Week 3
    // "reduced processing cost vs. first version" metric.
    bool naive = argc > 1 && std::strcmp(argv[1], "--naive") == 0;
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

        FrameProcessor processor;

        // Preallocated once, outside the loop, and passed by reference on
        // every iteration so the timed pipeline never allocates per frame.
        std::array<uint16_t, ISensor::kFrameWords> frame{};
        FrameResult result;

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
