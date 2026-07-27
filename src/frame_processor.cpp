#include "frame_processor.hpp"

#include <algorithm>

void FrameProcessor::Process(
    const std::array<uint16_t, ISensor::kFrameWords>& frame,
    FrameResult& out) const {
    // Pixel words are signed 16-bit two's complement counts on the wire.
    int16_t min_value = static_cast<int16_t>(frame[0]);
    int16_t max_value = min_value;
    long long sum = 0;

    for (size_t i = 0; i < kNumPixels; ++i) {
        int16_t value = static_cast<int16_t>(frame[i]);
        min_value = std::min(min_value, value);
        max_value = std::max(max_value, value);
        sum += value;
    }

    out.min_value = min_value;
    out.max_value = max_value;
    out.mean_value = static_cast<double>(sum) / static_cast<double>(kNumPixels);
    out.heat_signature_detected = max_value >= kDetectionThreshold;
}
