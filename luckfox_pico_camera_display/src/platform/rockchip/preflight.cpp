#include "platform/rockchip/preflight.hpp"

#include <cstdint>
#include <dlfcn.h>
#include <fstream>
#include <glob.h>
#include <optional>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

namespace camera_display {
namespace {

bool isDirectory(const std::string& path)
{
    struct stat information {};
    return ::stat(path.c_str(), &information) == 0
        && S_ISDIR(information.st_mode);
}

void requireDevice(const std::string& path, std::vector<std::string>& errors)
{
    struct stat information {};
    if (::stat(path.c_str(), &information) != 0) {
        errors.emplace_back("required device is missing: " + path);
        return;
    }
    if (::access(path.c_str(), R_OK | W_OK) != 0) {
        errors.emplace_back("required device is not readable/writable: " + path);
    }
}

std::string resolveVideoEntity(const std::string& wanted)
{
    glob_t matches{};
    std::string result;
    if (::glob("/sys/class/video4linux/video*/name", GLOB_NOSORT,
               nullptr, &matches) == 0) {
        for (std::size_t index = 0; index < matches.gl_pathc; ++index) {
            std::ifstream input(matches.gl_pathv[index]);
            std::string name;
            std::getline(input, name);
            if (name == wanted) {
                const std::string path{matches.gl_pathv[index]};
                const std::size_t nameEnd = path.rfind("/name");
                if (nameEnd == std::string::npos) continue;
                const std::size_t nodeStart = path.rfind('/', nameEnd - 1U);
                if (nodeStart == std::string::npos) continue;
                result = "/dev/" + path.substr(
                    nodeStart + 1U, nameEnd - nodeStart - 1U);
                break;
            }
        }
    }
    ::globfree(&matches);
    return result;
}

struct MediaOwner final {
    std::string pid;
    std::string process;
};

std::optional<MediaOwner> findMediaOwner(const std::string& devicePath)
{
    struct stat deviceInformation {};
    if (::stat(devicePath.c_str(), &deviceInformation) != 0
        || !S_ISCHR(deviceInformation.st_mode)) {
        return std::nullopt;
    }

    glob_t descriptors{};
    std::optional<MediaOwner> owner;
    if (::glob("/proc/[0-9]*/fd/*", GLOB_NOSORT,
               nullptr, &descriptors) == 0) {
        for (std::size_t index = 0; index < descriptors.gl_pathc; ++index) {
            struct stat descriptorInformation {};
            if (::stat(descriptors.gl_pathv[index], &descriptorInformation) != 0
                || !S_ISCHR(descriptorInformation.st_mode)
                || descriptorInformation.st_rdev != deviceInformation.st_rdev) {
                continue;
            }
            const std::string descriptorPath{descriptors.gl_pathv[index]};
            constexpr std::size_t pidStart = sizeof("/proc/") - 1U;
            const std::size_t pidEnd = descriptorPath.find('/', pidStart);
            if (pidEnd == std::string::npos) continue;
            MediaOwner found;
            found.pid = descriptorPath.substr(pidStart, pidEnd - pidStart);
            std::ifstream comm{"/proc/" + found.pid + "/comm"};
            std::getline(comm, found.process);
            if (found.process.empty()) found.process = "unknown";
            owner = std::move(found);
            break;
        }
    }
    ::globfree(&descriptors);
    return owner;
}

void requireSpiTransferSize(
    std::size_t configuredBytes, std::vector<std::string>& errors)
{
    std::ifstream input("/sys/module/spidev/parameters/bufsiz");
    std::uint64_t kernelLimit{};
    if (!(input >> kernelLimit) || kernelLimit == 0) return;
    if (configuredBytes > kernelLimit) {
        errors.emplace_back(
            "display.spi_chunk_bytes exceeds spidev bufsiz ("
            + std::to_string(kernelLimit) + ")");
    }
}

void requireLibrary(const char* name, std::vector<std::string>& errors)
{
    void* handle = ::dlopen(name, RTLD_LAZY | RTLD_LOCAL);
    if (handle == nullptr) {
        const char* detail = ::dlerror();
        errors.emplace_back(
            std::string{"cannot load "} + name + ": "
            + (detail == nullptr ? "unknown error" : detail));
        return;
    }
    ::dlclose(handle);
}

} // namespace

PreflightResult preflight(
    const AppConfig& config, bool requireMedia)
{
    PreflightResult result;
    std::vector<std::string>& errors = result.errors;
    requireDevice(config.display.spi_device, errors);
    requireDevice(config.display.gpio_chip, errors);
    requireSpiTransferSize(config.display.spi_chunk_bytes, errors);
    if (!requireMedia) return result;

    requireDevice("/dev/rk_dma_heap/rk-dma-heap-cma", errors);
    if (!isDirectory(config.camera.iq_directory)) {
        errors.emplace_back(
            "IQ directory is missing: " + config.camera.iq_directory);
    }
    const std::string entity = config.camera.entity_name.empty()
        ? "rkisp_mainpath" : config.camera.entity_name;
    std::string mediaNode;
    if (!entity.empty() && entity.front() == '/') {
        mediaNode = entity;
    }
    else {
        mediaNode = resolveVideoEntity(entity);
    }
    if (mediaNode.empty()) {
        errors.emplace_back("media entity is missing: " + entity);
    }
    else {
        requireDevice(mediaNode, errors);
        const auto owner = findMediaOwner(mediaNode);
        if (owner) {
            result.camera_occupied = true;
            errors.emplace_back(
                "camera is occupied: " + mediaNode + " is held by pid "
                + owner->pid + " (" + owner->process
                + "); use camera-displayctl start");
        }
    }
    requireLibrary("librockit.so", errors);
    requireLibrary("librkaiq.so", errors);
    requireLibrary("librga.so", errors);
    return result;
}

} // namespace camera_display
