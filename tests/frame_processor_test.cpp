#include "frame_processor.hpp"

#include <array>
#include <cstdint>

#include <gtest/gtest.h>

namespace {

// Builds a frame with every pixel word set to `pixel_value` and the
// trailing auxiliary words zeroed (they're outside the 32x24 pixel region
// FrameProcessor scans and shouldn't influence the result).
std::array<uint16_t, ISensor::kFrameWords> MakeUniformFrame(uint16_t pixel_value) {
    std::array<uint16_t, ISensor::kFrameWords> frame{};
    for (size_t i = 0; i < FrameProcessor::kNumPixels; ++i) {
        frame[i] = pixel_value;
    }
    return frame;
}

}  // namespace

TEST(FrameProcessorTest, UniformFrameHasEqualMinMaxMean) {
    auto frame = MakeUniformFrame(100);
    FrameProcessor processor;
    FrameResult result;
    processor.Process(frame, result);

    EXPECT_EQ(result.min_value, 100);
    EXPECT_EQ(result.max_value, 100);
    EXPECT_DOUBLE_EQ(result.mean_value, 100.0);
}

TEST(FrameProcessorTest, ComputesMinMaxMeanAcrossVaryingPixels) {
    auto frame = MakeUniformFrame(0);
    frame[0] = 10;
    frame[1] = 300;
    frame[2] = 50;

    FrameProcessor processor;
    FrameResult result;
    processor.Process(frame, result);

    EXPECT_EQ(result.min_value, 0);
    EXPECT_EQ(result.max_value, 300);
}

TEST(FrameProcessorTest, HeatSignatureNotDetectedBelowThreshold) {
    auto frame = MakeUniformFrame(FrameProcessor::kDetectionThreshold - 1);
    FrameProcessor processor;
    FrameResult result;
    processor.Process(frame, result);

    EXPECT_FALSE(result.heat_signature_detected);
}

TEST(FrameProcessorTest, HeatSignatureDetectedAtThreshold) {
    auto frame = MakeUniformFrame(FrameProcessor::kDetectionThreshold);
    FrameProcessor processor;
    FrameResult result;
    processor.Process(frame, result);

    EXPECT_TRUE(result.heat_signature_detected);
}

// Pixel words are signed 16-bit two's complement on the wire (per
// frame_processor.hpp); 0xFFFF must read back as -1, not 65535.
TEST(FrameProcessorTest, InterpretsPixelWordsAsTwosComplement) {
    auto frame = MakeUniformFrame(0xFFFF);
    FrameProcessor processor;
    FrameResult result;
    processor.Process(frame, result);

    EXPECT_EQ(result.min_value, -1);
    EXPECT_EQ(result.max_value, -1);
    EXPECT_DOUBLE_EQ(result.mean_value, -1.0);
}

// ProcessNaive is kept only as the Week 3 baseline for the allocation-cost
// comparison, but it must still be functionally equivalent to the
// optimized Process() -- a baseline that computes a different answer isn't
// a valid diff target.
TEST(FrameProcessorTest, NaiveAndOptimizedAgreeOnResult) {
    auto frame = MakeUniformFrame(0);
    frame[5] = 900;
    frame[6] = 10;

    FrameProcessor processor;
    FrameResult optimized;
    processor.Process(frame, optimized);
    FrameResult naive = processor.ProcessNaive(frame);

    EXPECT_EQ(optimized.min_value, naive.min_value);
    EXPECT_EQ(optimized.max_value, naive.max_value);
    EXPECT_DOUBLE_EQ(optimized.mean_value, naive.mean_value);
    EXPECT_EQ(optimized.heat_signature_detected, naive.heat_signature_detected);
}
