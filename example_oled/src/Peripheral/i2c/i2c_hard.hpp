#ifdef __GNUC__
#pragma once
#endif

#ifndef __I2C_HARD_H
#define __I2C_HARD_H

#include <string>
#include <cstdint>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <stdexcept>
#include <iostream>


class I2C_Hard {
public:

    /**
     * @brief I2C_Hard 类构造函数：打开I2C总线并设置从机地址
     * @param bus I2C总线设备路径，如 "/dev/i2c-1"
     * @param address I2C从机地址
     * @throw std::runtime_error 打开总线或设置地址失败时抛出
     */
    I2C_Hard(const std::string& _bus, uint8_t _address)
        : bus(_bus), address(_address), fd(-1) {
        
        // 打开I2C总线
        fd = open(bus.c_str(), O_RDWR);
        if (fd < 0) {
            throw std::runtime_error("无法打开 I2C 总线: " + bus);
        }

        // 设置从机地址
        if (ioctl(fd, I2C_SLAVE, address) < 0) {
            close(fd);
            throw std::runtime_error("无法设置 I2C 从机地址");
        }
    }

    /**
     * @brief I2C_Hard 类析构函数：关闭I2C总线文件描述符
     */
    ~I2C_Hard() {
        if (fd >= 0) {
            close(fd);
        }
    }

    /**
     * @brief 写入连续字节数据到I2C设备
     * @param data 待写入的数据缓冲区
     * @param length 数据长度
     * @return bool 写入成功返回true，失败返回false
     */
    bool writeBytes(const uint8_t* data, size_t length) {
        if (write(fd, data, length) != (ssize_t)length) {
            std::cerr << "I2C 写入失败: " << std::endl;
            return false;
        }
        return true;
    }

    /**
     * @brief 写入单字节数据到I2C设备
     * @param data 待写入的单字节数据
     * @return bool 写入成功返回true，失败返回false
     */
    bool writeByte(uint8_t data) {
        return writeBytes(&data, 1);
    }

private:
    std::string bus;
    uint8_t address;
    int fd;
};

#endif // __I2C_HARD_H