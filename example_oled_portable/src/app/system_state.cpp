#include "system_state.hpp"

#include <arpa/inet.h>
#include <ctime>
#include <fstream>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <string>

namespace app {

std::string SystemState::currentTime()
{
    const std::time_t now = std::time(nullptr);
    std::tm local{};
    localtime_r(&now, &local);
    char buffer[16]{};
    std::strftime(buffer, sizeof(buffer), "%H:%M:%S", &local);
    return buffer;
}

std::string SystemState::firstIpv4Address()
{
    ifaddrs* interfaces = nullptr;
    if (getifaddrs(&interfaces) != 0) { return "No IP"; }
    std::string result = "No IP";
    for (const ifaddrs* item = interfaces; item != nullptr; item = item->ifa_next) {
        if (item->ifa_addr == nullptr || item->ifa_addr->sa_family != AF_INET ||
            (item->ifa_flags & IFF_LOOPBACK) != 0) {
            continue;
        }
        char address[INET_ADDRSTRLEN]{};
        const auto* ipv4 = reinterpret_cast<const sockaddr_in*>(item->ifa_addr);
        if (inet_ntop(AF_INET, &ipv4->sin_addr, address, sizeof(address)) != nullptr) {
            result = address;
            break;
        }
    }
    freeifaddrs(interfaces);
    return result;
}

float SystemState::memoryUsage()
{
    std::ifstream input("/proc/meminfo");
    std::string label;
    std::uint64_t total = 0;
    std::uint64_t available = 0;
    std::uint64_t value = 0;
    std::string unit;
    while (input >> label >> value >> unit) {
        if (label == "MemTotal:") { total = value; }
        else if (label == "MemAvailable:") { available = value; }
        if (total != 0 && available != 0) { break; }
    }
    if (total == 0 || available > total) { return 0.0F; }
    return static_cast<float>(total - available) * 100.0F / static_cast<float>(total);
}

SystemState::CpuCounters SystemState::readCpuCounters()
{
    std::ifstream input("/proc/stat");
    std::string cpu;
    std::uint64_t user = 0, nice = 0, system = 0, idle = 0, iowait = 0;
    std::uint64_t irq = 0, softirq = 0, steal = 0;
    if (!(input >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal) ||
        cpu != "cpu") {
        return {};
    }
    return {user + nice + system + idle + iowait + irq + softirq + steal,
            idle + iowait, true};
}

float SystemState::cpuUsage()
{
    const CpuCounters current = readCpuCounters();
    if (!current.valid) { return 0.0F; }
    if (!previous_cpu_.valid || current.total <= previous_cpu_.total) {
        previous_cpu_ = current;
        return 0.0F;
    }
    const std::uint64_t total_delta = current.total - previous_cpu_.total;
    const std::uint64_t idle_delta = current.idle - previous_cpu_.idle;
    previous_cpu_ = current;
    return static_cast<float>(total_delta - idle_delta) * 100.0F /
           static_cast<float>(total_delta);
}

SystemSnapshot SystemState::sample()
{
    return {currentTime(), firstIpv4Address(), cpuUsage(), memoryUsage()};
}

} // namespace app

