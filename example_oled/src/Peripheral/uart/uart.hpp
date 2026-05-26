
/****************************************************************
 * @file uart.hpp
 * @brief UART 串口通信封装类
 * @details 提供基于 RAII 机制的串口资源管理，支持移动语义
 * @version 1.0.0
 * @date 2026-05-01
 * @note 使用方法:
 *       @code
 *       Uart uart("/dev/ttyS0", 115200);
 *       uart.write("Hello\n");
 *       auto response = uart.read();
 *       @endcode
 ***************************************************************/

// 现代 C++ 头文件保护机制，防止重复包含
#ifdef __GNUC__
#pragma once
#endif

#ifndef __UART_H
#define __UART_H

/**  **/
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <system_error>
#include <stdexcept>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

class Uart {
public:
    /**
     * @brief 构造函数：RAII 机制，对象创建即打开并配置硬件
     * @param port_name 串口设备路径，如 "/dev/ttyS0"
     * @param baud_rate 波特率，默认 115200
     * @throw std::system_error 串口打开或配置失败时抛出
     */
    explicit Uart(const std::string& port_name, int baud_rate = 115200) {
        // 打开串口 (O_RDWR 可读写 | O_NOCTTY 不作为控制终端 | O_NONBLOCK 非阻塞模式)
        fd_ = ::open(port_name.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (fd_ < 0) {
            throw std::system_error(errno, std::generic_category(), 
                                    "Failed to open serial port: " + port_name);
        }
        configure(baud_rate);
    }

    /**
     * @brief 析构函数：离开作用域自动安全关闭串口
     * @details 关闭文件描述符，释放系统资源
     */
    ~Uart() {
        if (fd_ >= 0) {::close(fd_);}
    }

    /**
     * @brief 禁用拷贝语义：物理串口是独占资源，不允许被复制
     */
    Uart(const Uart&) = delete;
    Uart& operator=(const Uart&) = delete;

    /**
     * @brief 启用移动语义：允许所有权转移
     * @param other 待移动的右值引用
     */
    Uart(Uart&& other) noexcept : fd_(other.fd_) {
        other.fd_ = -1;
    }

    /**
     * @brief 移动赋值运算符
     * @param other 右值引用
     * @return Uart& 返回自身引用以支持链式调用
     */
    Uart& operator=(Uart&& other) noexcept {
        if (this != &other) {
            if (fd_ >= 0) ::close(fd_);
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    /**
     * @brief 写入数据到串口
     * @param data 待写入的数据，使用 std::string_view 避免拷贝开销
     * @throw std::system_error 写入失败时抛出
     */
    void write(std::string_view data) {
        if (data.empty()) return;
        if (::write(fd_, data.data(), data.size()) < 0) {
            throw std::system_error(errno, std::generic_category(), "Error writing to port");
        }
    }

    /**
     * @brief 从串口读取数据
     * @param buffer_size 读取缓冲区大小，默认 256 字节
     * @return std::string 读取到的数据
     * @throw std::system_error 读取失败时抛出
     */
    std::string read(size_t buffer_size = 256) {
        std::string buffer(buffer_size, '\0');
        ssize_t bytes_read = ::read(fd_, buffer.data(), buffer_size);
        
        if (bytes_read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return "";
            throw std::system_error(errno, std::generic_category(), "Error reading from port");
        }
        if (bytes_read == 0) return ""; 

        buffer.resize(bytes_read);
        return buffer;
    }

    /**
     * @brief 获取串口文件描述符
     * @return int 串口文件描述符
     */
    int get_fd() const { return fd_; }

private:
    int fd_{-1};

    /**
     * @brief 内部配置方法：设置串口参数
     * @param baud_rate 波特率
     * @throw std::system_error 配置失败时抛出
     */
    void configure(int baud_rate) {
        struct termios tty;
        if (::tcgetattr(fd_, &tty) != 0) {
            throw std::system_error(errno, std::generic_category(), "Error from tcgetattr");
        }

        speed_t speed = get_baud_rate(baud_rate);
        ::cfsetospeed(&tty, speed);
        ::cfsetispeed(&tty, speed);

        // 8N1 配置：8个数据位，无校验，1个停止位
        tty.c_cflag &= ~PARENB; // 无校验位
        tty.c_cflag &= ~CSTOPB; // 1个停止位
        tty.c_cflag &= ~CSIZE;  // 清除数据位掩码
        tty.c_cflag |= CS8;     // 8个数据位

        // 建议增加的现代串口鲁棒性配置（防止被特殊字符阻塞）
        tty.c_cflag |= CREAD | CLOCAL; // 打开接收器，忽略调制解调器控制线
        tty.c_lflag &= ~ICANON;        // 开启原始模式（Raw mode），逐字节读取
        tty.c_lflag &= ~(ECHO | ECHOE | ISIG); // 禁用回显和信号字符
        
        // 设置非阻塞读取：VMIN=0 和 VTIME=0 表示 read() 调用立即返回，无论是否有数据
        tty.c_cc[VMIN]  = 0;  
        tty.c_cc[VTIME] = 0;

        if (::tcsetattr(fd_, TCSANOW, &tty) != 0) {
            throw std::system_error(errno, std::generic_category(), "Error from tcsetattr");
        }
    }

    /**
     * @brief 波特率映射：将数值转换为 POSIX 波特率常量
     * @param baud_rate 目标波特率
     * @return speed_t POSIX 波特率常量
     * @throw std::invalid_argument 不支持的波特率时抛出
     */
    speed_t get_baud_rate(int baud_rate) const {
        switch (baud_rate) {
            case 9600: return B9600;
            case 19200: return B19200;
            case 38400: return B38400;
            case 57600: return B57600;
            case 115200: return B115200;
            default: throw std::invalid_argument("Unsupported baud rate: " + std::to_string(baud_rate));
        }
        return B9600; // 默认返回值，满足编译器
    }
};

#endif // __UART_HPP