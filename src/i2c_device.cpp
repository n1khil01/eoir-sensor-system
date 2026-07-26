#include "i2c_device.hpp"

#include <fcntl.h>
#include <unistd.h>

#if defined(__linux__)
#include <linux/i2c.h>
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
    // Issuing the register-address write and the data read as two separate
    // read()/write() syscalls produces two separate I2C transactions with a
    // full STOP condition between them. The MLX90640 needs a genuine
    // repeated START (no STOP) between the address write and the burst
    // read, or its output can re-latch to a stale buffer -- so this uses a
    // single I2C_RDWR ioctl carrying both messages instead.
    uint8_t addr_buf[2] = {static_cast<uint8_t>(reg_addr >> 8),
                            static_cast<uint8_t>(reg_addr & 0xFF)};
    std::vector<uint8_t> raw(count * 2);

    i2c_msg msgs[2];
    msgs[0].addr = address_;
    msgs[0].flags = 0;
    msgs[0].len = sizeof(addr_buf);
    msgs[0].buf = addr_buf;

    msgs[1].addr = address_;
    msgs[1].flags = I2C_M_RD;
    msgs[1].len = static_cast<uint16_t>(raw.size());
    msgs[1].buf = raw.data();

    i2c_rdwr_ioctl_data ioctl_data;
    ioctl_data.msgs = msgs;
    ioctl_data.nmsgs = 2;

    if (ioctl(fd_, I2C_RDWR, &ioctl_data) < 0) {
        throw std::runtime_error("I2C combined write/read transaction failed");
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
