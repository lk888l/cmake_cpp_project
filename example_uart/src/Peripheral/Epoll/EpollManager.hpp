/****************************************************************
 * @file EpollManager.hpp
 * @brief epoll 管理器类定义
 * @version 1.0.0
 * @date 2026-05-01
 * @note 该类封装了 epoll 的创建、事件注册和等待机制，提供了一个简单的接口来监控多个文件描述符的事件。
 *      使用方法:
 *      @code
 *     EpollManager epoll;
 *    epoll.add_watch(uart.get_fd()); // 将 UART 文件描述符添加到 epoll 监控列表
 *   while (true) {
 *      auto ready_fds = epoll.wait(); // 阻塞等待事件发生
 *     for (int fd : ready_fds) {
 *       if (fd == uart.get_fd()) {
 *         auto data = uart.read(); // 处理 UART 可读事件
 *      }
 *    }
 *  }
 *     @endcode
 ***************************************************************/

#ifdef __GNUC__
#pragma once
#endif

#ifndef __EPOLL_HPP
#define __EPOLL_HPP
#include <sys/epoll.h> // 引入 epoll
#include <vector>
#include <system_error>
#include <stdexcept>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>


class EpollManager {
public:
    /**
     * @brief 构造函数：创建 epoll 实例
     * @throw std::system_error 创建失败时抛出
     */
    EpollManager() {
        epoll_fd_ = ::epoll_create1(0);
        if (epoll_fd_ < 0) {
            throw std::system_error(errno, std::generic_category(), "Failed to create epoll");
        }
    }

    ~EpollManager() {
        if (epoll_fd_ >= 0) ::close(epoll_fd_);
    }

    /**
     * @brief 禁用拷贝构造函数
     */
    EpollManager(const EpollManager&) = delete;

    /**
     * @brief 禁用赋值操作符
     */
    EpollManager& operator=(const EpollManager&) = delete;

    // 将需要监控的文件描述符添加到 epoll (默认监控可读事件 EPOLLIN)
    /**
     * @brief 添加文件描述符到 epoll 监控列表
     * @param fd 文件描述符
     * @param events 监控的事件类型
     * @throw std::system_error 添加失败时抛出
     */
    void add_watch(int fd, uint32_t events = EPOLLIN) {
        struct epoll_event ev;
        ev.events = events;
        ev.data.fd = fd; // 将 fd 存入事件数据中，以便唤醒时知道是谁

        if (::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) < 0) {
            throw std::system_error(errno, std::generic_category(), "Failed to add fd to epoll");
        }
    }

    /**
     * @brief 等待事件发生
     * @param timeout_ms 超时时间（毫秒）
     * @param max_events 最大事件数
     * @return 发生了事件的文件描述符列表
     * @throw std::system_error 等待失败时抛出
     */
    std::vector<int> wait(int timeout_ms = -1, int max_events = 10) {
        std::vector<struct epoll_event> events(max_events);
        
        // 核心阻塞点：这里会让出 CPU 调度，直到有事件发生或超时
        int num_ready = ::epoll_wait(epoll_fd_, events.data(), max_events, timeout_ms);
        
        if (num_ready < 0) {
            if (errno == EINTR) return {}; // 被系统信号打断，不是致命错误
            throw std::system_error(errno, std::generic_category(), "epoll_wait failed");
        }

        std::vector<int> ready_fds;
        for (int i = 0; i < num_ready; ++i) {
            ready_fds.push_back(events[i].data.fd);
        }
        return ready_fds;
    }

private:
    int epoll_fd_{-1};
};

#endif // __EPOLL_HPP
