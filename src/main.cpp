#include <algorithm>
#include <array>
#include <iostream>
#include <unordered_set>

#include "mlx90640_raw.hpp"

namespace {
constexpr int kNumTestReads = 100;
// Roughly the center of the 32x24 pixel grid, used as a liveliness probe:
// its raw value should change frame to frame as the scene changes (e.g.
// waving a hand near the sensor), unlike a stuck/cached read.
constexpr size_t kCenterPixelIndex = 12 * 32 + 16;
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
        std::unordered_set<uint16_t> center_pixel_values;

        std::cout << "Watching the center pixel (index " << kCenterPixelIndex
                  << ") across frames -- move a warm object (e.g. your hand) "
                     "near the sensor and the value should change.\n";

        for (int i = 0; i < kNumTestReads; ++i) {
            if (sensor.CaptureFrame(frame)) {
                ++successes;
                // Pixel words are signed 16-bit two's complement counts on
                // the wire; reading them as uint16_t wraps small negative
                // values up near 65535, so reinterpret before reporting.
                int16_t center_signed = static_cast<int16_t>(frame[kCenterPixelIndex]);
                center_pixel_values.insert(frame[kCenterPixelIndex]);
                if (i == 0 || i == kNumTestReads - 1) {
                    auto [min_it, max_it] = std::minmax_element(
                        frame.begin(), frame.end(), [](uint16_t a, uint16_t b) {
                            return static_cast<int16_t>(a) < static_cast<int16_t>(b);
                        });
                    std::cout << "  frame " << i
                              << ": min=" << static_cast<int16_t>(*min_it)
                              << " max=" << static_cast<int16_t>(*max_it)
                              << " center=" << center_signed << "\n";
                } else if (i % 10 == 0) {
                    std::cout << "  frame " << i
                              << ": center=" << center_signed << "\n";
                }
            } else {
                ++failures;
                std::cout << "  frame " << i << ": timed out waiting for new data\n";
            }
        }

        std::cout << "Result: " << successes << " successful reads, " << failures
                   << " failures out of " << kNumTestReads << ".\n";
        std::cout << "Center pixel took " << center_pixel_values.size()
                   << " distinct value(s) across the run"
                   << (center_pixel_values.size() > 1
                           ? " -- sensor is live.\n"
                           : " -- suspicious if the scene wasn't static.\n");
        return failures == 0 ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "Sensor communication failed: " << e.what() << "\n";
        std::cerr << "Check wiring (3.3V/GND/SDA/SCL), that I2C is enabled "
                      "(raspi-config), and run 'i2cdetect -y 1' to confirm "
                      "the device appears at 0x33.\n";
        return 1;
    }
}
