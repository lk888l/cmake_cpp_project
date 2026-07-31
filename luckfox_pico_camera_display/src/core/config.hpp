#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace camera_display {

enum class AspectMode : std::uint8_t {
    Letterbox,
    CenterCrop,
    Stretch,
};

struct CameraConfig final {
    std::uint32_t camera_id{0};
    std::uint32_t width{640};
    std::uint32_t height{360};
    std::uint32_t target_fps{30};
    std::uint32_t minimum_fps{25};
    std::uint32_t buffer_count{2};
    std::uint32_t frame_timeout_ms{500};
    std::string iq_directory{"/oem/usr/share/iqfiles"};
    std::string entity_name{};
};

struct DisplayConfig final {
    std::string spi_device{"/dev/spidev0.0"};
    std::uint32_t spi_hz{40'000'000};
    std::uint32_t spi_chunk_bytes{4096};
    std::string gpio_chip{"/dev/gpiochip1"};
    int dc_line{20};
    int reset_line{22};
    int backlight_line{21};
    std::uint16_t width{240};
    std::uint16_t height{240};
    std::uint16_t x_offset{0};
    std::uint16_t y_offset{0};
    std::uint16_t rotation{0};
    bool bgr{true};
    bool invert_colors{true};
};

struct RenderConfig final {
    AspectMode aspect{AspectMode::Letterbox};
    bool osd_enabled{true};
    std::uint32_t osd_period_ms{1000};
};

struct RuntimeConfig final {
    std::uint32_t maximum_restarts{3};
    std::uint32_t shutdown_timeout_s{3};
    std::uint32_t stats_period_ms{1000};
};

struct AppConfig final {
    CameraConfig camera;
    DisplayConfig display;
    RenderConfig render;
    RuntimeConfig runtime;
};

struct ConfigResult final {
    AppConfig config;
    std::vector<std::string> errors;
};

[[nodiscard]] ConfigResult loadConfigFile(const std::string& path);
[[nodiscard]] std::vector<std::string> validateConfig(const AppConfig& config);
[[nodiscard]] const char* toString(AspectMode mode) noexcept;

} // namespace camera_display

