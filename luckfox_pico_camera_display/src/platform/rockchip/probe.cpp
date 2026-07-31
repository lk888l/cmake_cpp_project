#include "platform/rockchip/probe.hpp"

#include <im2d.h>

#include <array>
#include <dirent.h>
#include <dlfcn.h>
#include <fstream>
#include <glob.h>
#include <iomanip>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <vector>

namespace camera_display {
namespace {

bool exists(const std::string& path)
{
    struct stat info {};
    return ::stat(path.c_str(), &info) == 0;
}

std::string readText(const std::string& path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return {};
    std::string value((std::istreambuf_iterator<char>(stream)),
                      std::istreambuf_iterator<char>());
    while (!value.empty()
           && (value.back() == '\0' || value.back() == '\n'
               || value.back() == '\r')) {
        value.pop_back();
    }
    return value;
}

std::vector<std::string> paths(const char* pattern)
{
    glob_t matches{};
    std::vector<std::string> result;
    if (::glob(pattern, GLOB_NOSORT, nullptr, &matches) == 0) {
        result.reserve(matches.gl_pathc);
        for (std::size_t index = 0; index < matches.gl_pathc; ++index) {
            result.emplace_back(matches.gl_pathv[index]);
        }
    }
    ::globfree(&matches);
    return result;
}

bool reportLibrary(std::ostream& output, const char* name)
{
    void* handle = ::dlopen(name, RTLD_LAZY | RTLD_LOCAL);
    output << "  " << std::left << std::setw(18) << name
           << (handle != nullptr ? "available" : "MISSING");
    if (handle == nullptr) output << " (" << ::dlerror() << ')';
    output << '\n';
    if (handle != nullptr) ::dlclose(handle);
    return handle != nullptr;
}

} // namespace

ProbeExit runProbe(std::ostream& output)
{
    bool healthy = true;
    const std::string model = readText("/proc/device-tree/model");
    output << "Board\n  model              "
           << (model.empty() ? "unknown" : model) << '\n';

    output << "Media nodes\n";
    const auto videoNodes = paths("/sys/class/video4linux/video*/name");
    bool mainPath{};
    for (const auto& path : videoNodes) {
        const std::string name = readText(path);
        output << "  " << path << " = " << name << '\n';
        if (name.find("rkisp_mainpath") != std::string::npos) mainPath = true;
    }
    if (!mainPath) {
        output << "  rkisp_mainpath     MISSING\n";
        healthy = false;
    }

    output << "Runtime libraries\n";
    healthy = reportLibrary(output, "librockit.so") && healthy;
    healthy = reportLibrary(output, "librkaiq.so") && healthy;
    healthy = reportLibrary(output, "librga.so") && healthy;

    output << "Devices and data\n";
    const std::array<std::pair<const char*, const char*>, 4> required{{
        {"/dev/rk_dma_heap/rk-dma-heap-cma", "CMA DMA heap"},
        {"/dev/spidev0.0", "SPI display"},
        {"/dev/gpiochip1", "display GPIO"},
        {"/oem/usr/share/iqfiles", "RKAIQ IQ files"}}};
    for (const auto& item : required) {
        const bool found = exists(item.first);
        output << "  " << std::left << std::setw(34) << item.first
               << (found ? "available" : "MISSING")
               << " (" << item.second << ")\n";
        healthy = found && healthy;
    }

    output << "RGA\n";
    const char* rgaCapabilities = querystring(RGA_ALL);
    output << "  capability         "
           << (rgaCapabilities == nullptr
                   ? "query unavailable" : rgaCapabilities)
           << '\n';
    output << "Probe result\n  status             "
           << (healthy ? "READY" : "INCOMPLETE") << '\n';
    return healthy ? ProbeExit::Success : ProbeExit::MissingRequiredCapability;
}

} // namespace camera_display
