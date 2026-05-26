#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <ifaddrs.h>

class SystemState {
public:

    /**
     * @brief 获取当前系统时间
     * @return 当前时间字符串，格式为 "HH:MM:SS"
     * @details 使用 time() 和 localtime() 获取当前时间，并格式化为字符串返回
     * @note 该函数每次调用都会获取最新的系统时间
     */
    static std::string getTime() {
        time_t now = time(0);
        struct tm tstruct;
        char buf[10];
        tstruct = *localtime(&now);
        strftime(buf, sizeof(buf), "%H:%M:%S", &tstruct);
        return std::string(buf);
    }

    /**
     * @brief 获取系统的 IP 地址
     * @return IP 地址字符串，如果没有有效 IP 则返回 "No IP"
     * @details 通过 getifaddrs 获取系统网络接口列表，查找第一个非回环接口的 IPv4 地址并返回
     */
    static std::string getIP() {
        struct ifaddrs *ifAddrStruct = NULL;
        void *tmpAddrPtr = NULL;
        std::string ipAddr = "No IP";
        getifaddrs(&ifAddrStruct);
        for (struct ifaddrs *ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr) continue;
            if (ifa->ifa_addr->sa_family == AF_INET) { // 检查 IPv4
                tmpAddrPtr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
                char addressBuffer[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
                std::string name(ifa->ifa_name);
                if (name != "lo") { // 排除回环地址
                    ipAddr = addressBuffer;
                    break;
                }
            }
        }
        if (ifAddrStruct) freeifaddrs(ifAddrStruct);
        return ipAddr;
    }

    /**
     * @brief 获取系统内存占用率
     * @return 内存占用率百分比
     * @throw std::runtime_error 读取 /proc/meminfo 失败时抛出
     * @details 通过解析 /proc/meminfo 文件获取总内存和可用内存，计算占用率 并返回
     */
    static float getMemUsage() {
        std::ifstream meminfo("/proc/meminfo");
        std::string label;
        long total, available;
        while (meminfo >> label) {
            if (label == "MemTotal:") meminfo >> total;
            else if (label == "MemAvailable:") {
                meminfo >> available;
                break;
            }
        }
        return (float)(total - available) / (float)total * 100.0f;
    }



    /**
     * @brief 获取系统 CPU 占用率
     * @return CPU 占用率百分比
     * @throw std::runtime_error 读取 /proc/stat 失败时抛出
     * @details 通过调用 getCPUReadings 获取两次 CPU 时间统计，计算两次快照的差值来计算 CPU 占用率，并返回
     * @note 该函数每次调用都会获取最新的 CPU 占用率，适合定时调用以监控系统状态
     */
    static float getCPUUsage() {
        static CPUData last = getCPUReadings();
        CPUData now = getCPUReadings();
        long lastTotal = last.user + last.nice + last.system + last.idle + last.iowait + last.irq + last.softirq;
        long nowTotal = now.user + now.nice + now.system + now.idle + now.iowait + now.irq + now.softirq;
        long totalDiff = nowTotal - lastTotal;
        long idleDiff = now.idle - last.idle;
        last = now;
        return (totalDiff > 0) ? (float)(totalDiff - idleDiff) / totalDiff * 100.0f : 0.0f;
    }

protected:
    struct CPUData { long user, nice, system, idle, iowait, irq, softirq; };
    static CPUData getCPUReadings() {
        std::ifstream stat("/proc/stat");
        std::string cpu;
        CPUData data;
        stat >> cpu >> data.user >> data.nice >> data.system >> data.idle >> data.iowait >> data.irq >> data.softirq;
        return data;
    }
};