#include "i2c_device.hpp"

#include <fcntl.h>
#include <unistd.h>

#if defined(__linux__)
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#endif

I2CDevice::I2CDevice(const std::string& bus_path, uint8_t device_address)
    : fd_(-1), address_(device_address) {
#if defined(__linux__)
    fd_ = open(bus_path.c_str(), O_RDWR);
    if (fd_ < 0) {
        throw std::runtime_error("Failed to open I2C bus: " + bus_path);
    }
    if (ioctl(fd_, I2C_SLAVE, address_) < 0) {
        close(fd_);
        fd_ = -1;
        throw std::runtime_error("Failed to set I2C slave address");
    }
#else
    throw std::runtime_error("I2CDevice requires Linux i2c-dev support");
#endif
}

I2CDevice::~I2CDevice() {
#if defined(__linux__)
    if (fd_ >= 0) {
        close(fd_);
    }
#endif
}

void I2CDevice::ReadWords(uint16_t reg_addr, uint16_t* out, size_t count) {
#if defined(__linux__)
    uint8_t addr_buf[2] = {static_cast<uint8_t>(reg_addr >> 8),
                            static_cast<uint8_t>(reg_addr & 0xFF)};

    if (write(fd_, addr_buf, 2) != 2) {
        throw std::runtime_error("I2C write (register address) failed");
    }

    std::vector<uint8_t> raw(count * 2);
    ssize_t n = read(fd_, raw.data(), raw.size());
    if (n < 0 || static_cast<size_t>(n) != raw.size()) {
        throw std::runtime_error("I2C read (frame data) failed or short read");
    }

    for (size_t i = 0; i < count; ++i) {
        out[i] = (static_cast<uint16_t>(raw[2 * i]) << 8) | raw[2 * i + 1];
    }
#else
    (void)reg_addr;
    (void)out;
    (void)count;
    throw std::runtime_error("I2CDevice requires Linux i2c-dev support");
#endif
}

void I2CDevice::WriteWord(uint16_t reg_addr, uint16_t value) {
#if defined(__linux__)
    uint8_t buf[4] = {
        static_cast<uint8_t>(reg_addr >> 8), static_cast<uint8_t>(reg_addr & 0xFF),
        static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value & 0xFF)};

    if (write(fd_, buf, 4) != 4) {
        throw std::runtime_error("I2C write (register value) failed");
    }
#else
    (void)reg_addr;
    (void)value;
    throw std::runtime_error("I2CDevice requires Linux i2c-dev support");
#endif
}
