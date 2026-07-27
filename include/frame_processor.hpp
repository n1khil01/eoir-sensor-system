#pragma once

#include <array>
#include <cstdint>

#include "sensor_interface.hpp"

// Result of processing one frame. Callers own the storage (stack or a
// preallocated member) so the pipeline never allocates per frame.
struct FrameResult {
    int16_t min_value = 0;
    int16_t max_value = 0;
    double mean_value = 0.0;
    bool heat_signature_detected = false;
};

// Stand-in for the Week 4+ motion/heat-signature detection algorithm.
// Scans the 32x24 pixel region (the frame's remaining words are
// auxiliary/control data with an unrelated scale) and reports a simple
// min/max/mean summary plus a threshold-based detection flag.
class FrameProcessor {
public:
    static constexpr size_t kNumPixels = 32 * 24;
    static constexpr int16_t kDetectionThreshold = 800;

    // Processes `frame` in place, writing the summary into `out`.
    // Takes both parameters by reference so no frame or result is ever
    // copied or heap-allocated on the hot path.
    void Process(const std::array<uint16_t, ISensor::kFrameWords>& frame,
                 FrameResult& out) const;

    // Deliberately naive stand-in for the "first working version" this
    // pipeline started from: it heap-allocates a copy of the pixel data
    // and returns the result by value instead of writing through a
    // preallocated reference. Kept only so the Week 3 processing-cost
    // metric has a real baseline to diff against -- do not call this on
    // the hot path.
    FrameResult ProcessNaive(
        const std::array<uint16_t, ISensor::kFrameWords>& frame) const;
};
