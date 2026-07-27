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

    explicit Mlx90640Raw(const std::string& bus_path,
                          uint8_t address = kDefaultAddress);

    // Confirms the device responds by reading the control register.
    // Throws if the read fails or returns an implausible value.
    void CheckConnection() override;

    // Blocks (with light polling) until a new frame is available, then
    // reads it into `frame`. Returns false if the read timed out.
    bool CaptureFrame(std::array<uint16_t, kFrameWords>& frame) override;

private:
    I2CDevice device_;

    static constexpr uint16_t kStatusReg = 0x8000;
    static constexpr uint16_t kControlReg = 0x800D;
    static constexpr uint16_t kFrameStartReg = 0x0400;
    static constexpr uint16_t kNewDataReadyMask = 0x0008;
};
