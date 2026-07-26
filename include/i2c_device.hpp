#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

// Thin wrapper around the Linux i2c-dev interface (/dev/i2c-N).
class I2CDevice {
public:
    I2CDevice(const std::string& bus_path, uint8_t device_address);
    ~I2CDevice();

    I2CDevice(const I2CDevice&) = delete;
    I2CDevice& operator=(const I2CDevice&) = delete;

    // Reads `count` 16-bit words starting at `reg_addr` (big-endian on the wire,
    // as used by the MLX90640) into `out`. Throws std::runtime_error on failure.
    void ReadWords(uint16_t reg_addr, uint16_t* out, size_t count);

    // Writes a single 16-bit word to `reg_addr`.
    void WriteWord(uint16_t reg_addr, uint16_t value);

private:
    int fd_;
    uint8_t address_;
};
