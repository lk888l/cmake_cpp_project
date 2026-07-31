#include "core/config.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <fstream>
#include <limits>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <unordered_map>

namespace camera_display {
namespace {

std::string trim(std::string value)
{
    const auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

bool parseBoolean(std::string_view text, bool& value)
{
    if (text == "true" || text == "yes" || text == "1" || text == "on") {
        value = true;
        return true;
    }
    if (text == "false" || text == "no" || text == "0" || text == "off") {
        value = false;
        return true;
    }
    return false;
}

template <typename T>
bool parseUnsigned(std::string_view text, T& value)
{
    static_assert(std::is_unsigned<T>::value, "T must be unsigned");
    if (text.empty() || text.front() == '-') return false;
    std::uint64_t parsed{};
    const auto result =
        std::from_chars(text.data(), text.data() + text.size(), parsed, 10);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()
        || parsed > std::numeric_limits<T>::max()) {
        return false;
    }
    value = static_cast<T>(parsed);
    return true;
}

bool parseLine(std::string_view text, int& value)
{
    std::uint32_t parsed{};
    if (!parseUnsigned(text, parsed)
        || parsed > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
        return false;
    }
    value = static_cast<int>(parsed);
    return true;
}

using Setter = bool (*)(AppConfig&, std::string_view);

bool setCameraId(AppConfig& c, std::string_view v) { return parseUnsigned(v, c.camera.camera_id); }
bool setCameraWidth(AppConfig& c, std::string_view v) { return parseUnsigned(v, c.camera.width); }
bool setCameraHeight(AppConfig& c, std::string_view v) { return parseUnsigned(v, c.camera.height); }
bool setTargetFps(AppConfig& c, std::string_view v) { return parseUnsigned(v, c.camera.target_fps); }
bool setMinimumFps(AppConfig& c, std::string_view v) { return parseUnsigned(v, c.camera.minimum_fps); }
bool setBufferCount(AppConfig& c, std::string_view v) { return parseUnsigned(v, c.camera.buffer_count); }
bool setFrameTimeout(AppConfig& c, std::string_view v) { return parseUnsigned(v, c.camera.frame_timeout_ms); }
bool setIqDirectory(AppConfig& c, std::string_view v) { c.camera.iq_directory = v; return !v.empty(); }
bool setEntity(AppConfig& c, std::string_view v) { c.camera.entity_name = v; return true; }
bool setSpiDevice(AppConfig& c, std::string_view v) { c.display.spi_device = v; return !v.empty(); }
bool setSpiHz(AppConfig& c, std::string_view v) { return parseUnsigned(v, c.display.spi_hz); }
bool setSpiChunk(AppConfig& c, std::string_view v) { return parseUnsigned(v, c.display.spi_chunk_bytes); }
bool setGpioChip(AppConfig& c, std::string_view v) { c.display.gpio_chip = v; return !v.empty(); }
bool setDcLine(AppConfig& c, std::string_view v) { return parseLine(v, c.display.dc_line); }
bool setResetLine(AppConfig& c, std::string_view v) { return parseLine(v, c.display.reset_line); }
bool setBacklightLine(AppConfig& c, std::string_view v) { return parseLine(v, c.display.backlight_line); }
bool setDisplayWidth(AppConfig& c, std::string_view v) { return parseUnsigned(v, c.display.width); }
bool setDisplayHeight(AppConfig& c, std::string_view v) { return parseUnsigned(v, c.display.height); }
bool setXOffset(AppConfig& c, std::string_view v) { return parseUnsigned(v, c.display.x_offset); }
bool setYOffset(AppConfig& c, std::string_view v) { return parseUnsigned(v, c.display.y_offset); }
bool setRotation(AppConfig& c, std::string_view v) { return parseUnsigned(v, c.display.rotation); }
bool setBgr(AppConfig& c, std::string_view v) { return parseBoolean(v, c.display.bgr); }
bool setInvert(AppConfig& c, std::string_view v) { return parseBoolean(v, c.display.invert_colors); }
bool setOsd(AppConfig& c, std::string_view v) { return parseBoolean(v, c.render.osd_enabled); }
bool setOsdPeriod(AppConfig& c, std::string_view v) { return parseUnsigned(v, c.render.osd_period_ms); }
bool setMaxRestarts(AppConfig& c, std::string_view v) { return parseUnsigned(v, c.runtime.maximum_restarts); }
bool setShutdownTimeout(AppConfig& c, std::string_view v) { return parseUnsigned(v, c.runtime.shutdown_timeout_s); }
bool setStatsPeriod(AppConfig& c, std::string_view v) { return parseUnsigned(v, c.runtime.stats_period_ms); }

bool setAspect(AppConfig& c, std::string_view value)
{
    if (value == "letterbox") c.render.aspect = AspectMode::Letterbox;
    else if (value == "center-crop") c.render.aspect = AspectMode::CenterCrop;
    else if (value == "stretch") c.render.aspect = AspectMode::Stretch;
    else return false;
    return true;
}

const std::unordered_map<std::string, Setter>& setters()
{
    static const std::unordered_map<std::string, Setter> table = {
        {"camera.id", setCameraId},
        {"camera.width", setCameraWidth},
        {"camera.height", setCameraHeight},
        {"camera.target_fps", setTargetFps},
        {"camera.minimum_fps", setMinimumFps},
        {"camera.buffer_count", setBufferCount},
        {"camera.frame_timeout_ms", setFrameTimeout},
        {"camera.iq_directory", setIqDirectory},
        {"camera.entity_name", setEntity},
        {"display.spi_device", setSpiDevice},
        {"display.spi_hz", setSpiHz},
        {"display.spi_chunk_bytes", setSpiChunk},
        {"display.gpio_chip", setGpioChip},
        {"display.dc_line", setDcLine},
        {"display.reset_line", setResetLine},
        {"display.backlight_line", setBacklightLine},
        {"display.width", setDisplayWidth},
        {"display.height", setDisplayHeight},
        {"display.x_offset", setXOffset},
        {"display.y_offset", setYOffset},
        {"display.rotation", setRotation},
        {"display.bgr", setBgr},
        {"display.invert_colors", setInvert},
        {"render.aspect", setAspect},
        {"render.osd_enabled", setOsd},
        {"render.osd_period_ms", setOsdPeriod},
        {"runtime.maximum_restarts", setMaxRestarts},
        {"runtime.shutdown_timeout_s", setShutdownTimeout},
        {"runtime.stats_period_ms", setStatsPeriod},
    };
    return table;
}

} // namespace

ConfigResult loadConfigFile(const std::string& path)
{
    ConfigResult result;
    std::ifstream input(path);
    if (!input) {
        result.errors.emplace_back("cannot open configuration file: " + path);
        return result;
    }

    std::string section;
    std::string line;
    std::size_t lineNumber{};
    while (std::getline(input, line)) {
        ++lineNumber;
        line = trim(line);
        if (line.empty() || line.front() == '#' || line.front() == ';') continue;

        if (line.front() == '[' && line.back() == ']') {
            section = trim(line.substr(1, line.size() - 2));
            if (section.empty()) {
                result.errors.emplace_back("line " + std::to_string(lineNumber)
                                           + ": empty section");
            }
            continue;
        }

        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            result.errors.emplace_back("line " + std::to_string(lineNumber)
                                       + ": expected key=value");
            continue;
        }
        const std::string key = section + "." + trim(line.substr(0, separator));
        const std::string value = trim(line.substr(separator + 1));
        const auto found = setters().find(key);
        if (found == setters().end()) {
            result.errors.emplace_back("line " + std::to_string(lineNumber)
                                       + ": unknown key " + key);
            continue;
        }
        if (!found->second(result.config, value)) {
            result.errors.emplace_back("line " + std::to_string(lineNumber)
                                       + ": invalid value for " + key);
        }
    }

    auto validation = validateConfig(result.config);
    result.errors.insert(result.errors.end(), validation.begin(), validation.end());
    return result;
}

std::vector<std::string> validateConfig(const AppConfig& config)
{
    std::vector<std::string> errors;
    if (config.camera.width < 64 || config.camera.height < 64
        || (config.camera.width & 1U) != 0 || (config.camera.height & 1U) != 0) {
        errors.emplace_back("camera dimensions must be even and at least 64x64");
    }
    if (config.camera.target_fps < 1 || config.camera.target_fps > 30) {
        errors.emplace_back("camera.target_fps must be in [1, 30]");
    }
    if (config.camera.minimum_fps < 1
        || config.camera.minimum_fps > config.camera.target_fps) {
        errors.emplace_back("camera.minimum_fps must be in [1, target_fps]");
    }
    if (config.camera.buffer_count != 2) {
        errors.emplace_back("camera.buffer_count must be exactly 2 on RV1103");
    }
    if (config.camera.frame_timeout_ms < 100 || config.camera.frame_timeout_ms > 5000) {
        errors.emplace_back("camera.frame_timeout_ms must be in [100, 5000]");
    }
    if (config.camera.iq_directory.empty()) {
        errors.emplace_back("camera.iq_directory must not be empty");
    }
    if (config.display.spi_device.empty() || config.display.gpio_chip.empty()) {
        errors.emplace_back("display device paths must not be empty");
    }
    if (config.display.spi_hz < 1'000'000 || config.display.spi_hz > 80'000'000) {
        errors.emplace_back("display.spi_hz must be in [1000000, 80000000]");
    }
    if (config.display.spi_chunk_bytes < 256
        || config.display.spi_chunk_bytes > 1U * 1024U * 1024U) {
        errors.emplace_back("display.spi_chunk_bytes must be in [256, 1048576]");
    }
    if (config.display.dc_line < 0 || config.display.reset_line < 0
        || config.display.backlight_line < 0) {
        errors.emplace_back("display GPIO lines must be non-negative");
    }
    if (config.display.dc_line == config.display.reset_line
        || config.display.dc_line == config.display.backlight_line
        || config.display.reset_line == config.display.backlight_line) {
        errors.emplace_back("display GPIO lines must be unique");
    }
    if (config.display.width != 240 || config.display.height != 240) {
        errors.emplace_back("this release supports only a 240x240 ST7789 panel");
    }
    if (config.display.rotation != 0 && config.display.rotation != 90
        && config.display.rotation != 180 && config.display.rotation != 270) {
        errors.emplace_back("display.rotation must be 0, 90, 180, or 270");
    }
    if (config.render.osd_period_ms < 250 || config.render.osd_period_ms > 10'000) {
        errors.emplace_back("render.osd_period_ms must be in [250, 10000]");
    }
    if (config.render.osd_period_ms < config.runtime.stats_period_ms) {
        errors.emplace_back(
            "render.osd_period_ms must be at least runtime.stats_period_ms");
    }
    if (config.runtime.maximum_restarts > 10) {
        errors.emplace_back("runtime.maximum_restarts must not exceed 10");
    }
    if (config.runtime.shutdown_timeout_s < 1
        || config.runtime.shutdown_timeout_s > 30) {
        errors.emplace_back("runtime.shutdown_timeout_s must be in [1, 30]");
    }
    if (config.runtime.stats_period_ms < 250
        || config.runtime.stats_period_ms > 60'000) {
        errors.emplace_back("runtime.stats_period_ms must be in [250, 60000]");
    }
    return errors;
}

const char* toString(AspectMode mode) noexcept
{
    switch (mode) {
    case AspectMode::Letterbox: return "letterbox";
    case AspectMode::CenterCrop: return "center-crop";
    case AspectMode::Stretch: return "stretch";
    }
    return "unknown";
}

} // namespace camera_display
