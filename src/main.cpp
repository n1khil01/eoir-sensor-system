#include <algorithm>
#include <array>
#include <iostream>

#include "mlx90640_raw.hpp"

namespace {
constexpr int kNumTestReads = 100;
}

int main() {
    std::cout << "EOIR sensor system - Week 2 sensor communication test\n";

    try {
        Mlx90640Raw sensor("/dev/i2c-1");
        sensor.CheckConnection();
        std::cout << "MLX90640 responded on the I2C bus.\n";

        int successes = 0;
        int failures = 0;
        std::array<uint16_t, Mlx90640Raw::kFrameWords> frame{};

        for (int i = 0; i < kNumTestReads; ++i) {
            if (sensor.CaptureFrame(frame)) {
                ++successes;
                if (i == 0 || i == kNumTestReads - 1) {
                    auto [min_it, max_it] = std::minmax_element(frame.begin(), frame.end());
                    std::cout << "  frame " << i << ": min=" << *min_it
                              << " max=" << *max_it << "\n";
                }
            } else {
                ++failures;
                std::cout << "  frame " << i << ": timed out waiting for new data\n";
            }
        }

        std::cout << "Result: " << successes << " successful reads, " << failures
                   << " failures out of " << kNumTestReads << ".\n";
        return failures == 0 ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "Sensor communication failed: " << e.what() << "\n";
        std::cerr << "Check wiring (3.3V/GND/SDA/SCL), that I2C is enabled "
                      "(raspi-config), and run 'i2cdetect -y 1' to confirm "
                      "the device appears at 0x33.\n";
        return 1;
    }
}
