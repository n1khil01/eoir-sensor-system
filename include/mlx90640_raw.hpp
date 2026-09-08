#pragma once

#include <array>
#include <cstdint>

#include "i2c_device.hpp"
#include "sensor_interface.hpp"

// Minimal MLX90640 driver that pulls raw, uncalibrated RAM frames.
// This deliberately stops short of applying the EEPROM calibration
// coefficients -- the goal here is only to prove the I2C link is stable
// across many reads and to stand in as the concrete ISensor for the
// single-threaded capture-to-process pipeline.
class Mlx90640Raw : public ISensor {
public:
    static constexpr uint8_t kDefaultAddress = 0x33;

    // Fps codes for the control register's refresh-rate field (bits 7-9):
    // 0=0.5Hz 1=1Hz 2=2Hz(power-on default) 3=4Hz 4=8Hz 5=16Hz 6=32Hz 7=64Hz.
    // Values above 8Hz need the I2C bus running in fast mode (400kHz) or
    // faster -- a full frame is 1664 bytes, which alone takes on the order
    // of 150ms at the Pi's default 100kHz clock and eats most of an 8Hz
    // period. Configure that via `dtparam=i2c_arm_baudrate=400000` in
    // /boot/firmware/config.txt and reboot before raising this further.
    enum class RefreshRate : uint8_t {
        kHz0_5 = 0,
        kHz1 = 1,
        kHz2 = 2,
        kHz4 = 3,
        kHz8 = 4,
        kHz16 = 5,
        kHz32 = 6,
        kHz64 = 7,
    };

    explicit Mlx90640Raw(const std::string& bus_path,
                          uint8_t address = kDefaultAddress,
                          RefreshRate refresh_rate = RefreshRate::kHz8);

    // Confirms the device responds by reading the control register.
    // Throws if the read fails or returns an implausible value.
    void CheckConnection() override;

    // Blocks (with light polling) until a new frame is available, then
    // reads it into `frame`. Returns false if the read timed out.
    bool CaptureFrame(std::array<uint16_t, kFrameWords>& frame) override;

private:
    // Read-modify-writes the control register's refresh-rate field, leaving
    // every other bit (resolution, subpage mode, ...) untouched.
    void SetRefreshRate(RefreshRate rate);

    I2CDevice device_;

    static constexpr uint16_t kStatusReg = 0x8000;
    static constexpr uint16_t kControlReg = 0x800D;
    static constexpr uint16_t kFrameStartReg = 0x0400;
    static constexpr uint16_t kNewDataReadyMask = 0x0008;
    // Clears bits 7-9 (the refresh-rate field) while preserving the rest of
    // the control register.
    static constexpr uint16_t kRefreshRateMask = 0xFC7F;
};
