#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

// Base interface for a frame-producing sensor. The MLX90640 driver is the
// only concrete implementation today, but keeping capture behind a
// polymorphic interface is what lets the Week 4 capture thread and the
// Week 7 test suite depend on an abstraction instead of a specific part.
class ISensor {
public:
    static constexpr size_t kFrameWords = 832;  // 32 x 24 pixels + 32 aux words

    virtual ~ISensor() = default;

    // Confirms the device responds and is producing plausible data.
    // Throws if the check fails.
    virtual void CheckConnection() = 0;

    // Blocks until a new frame is available, then reads it into `frame`.
    // Returns false if the read timed out.
    virtual bool CaptureFrame(std::array<uint16_t, kFrameWords>& frame) = 0;
};
