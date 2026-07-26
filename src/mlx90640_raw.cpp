#include "mlx90640_raw.hpp"

#include <chrono>
#include <thread>

Mlx90640Raw::Mlx90640Raw(const std::string& bus_path, uint8_t address)
    : device_(bus_path, address) {}

void Mlx90640Raw::CheckConnection() {
    uint16_t control_value = 0;
    device_.ReadWords(kControlReg, &control_value, 1);
    // The control register's reset value has bit 10 set (refresh rate field
    // is non-zero by default); an all-zero or all-ones read almost always
    // means nothing is actually answering on the bus.
    if (control_value == 0x0000 || control_value == 0xFFFF) {
        throw std::runtime_error(
            "MLX90640 control register read implausible value; check wiring");
    }
}

bool Mlx90640Raw::CaptureFrame(std::array<uint16_t, kFrameWords>& frame) {
    // The MLX90640's default refresh rate after power-up can be as slow as
    // 0.5 Hz (one frame every 2 seconds), so give it generous headroom
    // rather than tuning this tightly to a specific configured rate.
    constexpr int kMaxPolls = 600;
    constexpr auto kPollDelay = std::chrono::milliseconds(5);

    for (int i = 0; i < kMaxPolls; ++i) {
        uint16_t status = 0;
        device_.ReadWords(kStatusReg, &status, 1);
        if (status & kNewDataReadyMask) {
            device_.ReadWords(kFrameStartReg, frame.data(), kFrameWords);
            // Clear the new-data-ready flag so the next poll doesn't see a
            // stale frame.
            device_.WriteWord(kStatusReg, status & ~kNewDataReadyMask);
            return true;
        }
        std::this_thread::sleep_for(kPollDelay);
    }
    return false;
}
