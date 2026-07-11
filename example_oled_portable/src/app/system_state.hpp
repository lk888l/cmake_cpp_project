#pragma once

#include <cstdint>
#include <string>

namespace app {

struct SystemSnapshot {
    std::string time;
    std::string ip_address;
    float cpu_percent = 0.0F;
    float memory_percent = 0.0F;
};

class SystemState {
public:
    SystemSnapshot sample();

private:
    struct CpuCounters {
        std::uint64_t total = 0;
        std::uint64_t idle = 0;
        bool valid = false;
    };

    static std::string currentTime();
    static std::string firstIpv4Address();
    static float memoryUsage();
    static CpuCounters readCpuCounters();
    float cpuUsage();

    CpuCounters previous_cpu_{};
};

} // namespace app

