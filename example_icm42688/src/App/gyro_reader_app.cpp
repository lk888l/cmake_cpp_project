#include "gyro_reader_app.hpp"

#include <chrono>
#include <cstdio>
#include <thread>

#include "vqf.hpp"

namespace app {
namespace {

constexpr float kGravityMps2 = 9.80665F;
constexpr float kDegToRad = 0.017453292519943295F;

} // namespace

GyroReaderApp::GyroReaderApp(hardware::Icm42688& imu, GyroReaderConfig config)
    : imu_(imu)
    , config_(config)
{
    if (config_.period_ms == 0) {
        config_.period_ms = 10;
    }
}

void GyroReaderApp::run(const std::atomic_bool& running)
{
    const auto period = std::chrono::milliseconds(config_.period_ms);
    const vqf_real_t fusion_ts = static_cast<vqf_real_t>(config_.period_ms) / 1000.0;
    VQF vqf(fusion_ts);

    if (config_.print_header) {
        std::puts("quat_w,quat_x,quat_y,quat_z,roll_deg,pitch_deg,yaw_deg,temp_c,"
                  "accel_x_g,accel_y_g,accel_z_g,gyro_x_dps,gyro_y_dps,gyro_z_dps");
    }

    auto next_wake = std::chrono::steady_clock::now();
    while (running.load()) {
        hardware::Icm42688Sample sample = {};
        const hardware::Icm42688Status status = imu_.readSample(sample);
        if (status == hardware::Icm42688Status::ok) {
            const vqf_real_t gyr[3] = {
                static_cast<vqf_real_t>(sample.gyro_x_dps * kDegToRad),
                static_cast<vqf_real_t>(sample.gyro_y_dps * kDegToRad),
                static_cast<vqf_real_t>(sample.gyro_z_dps * kDegToRad),
            };
            const vqf_real_t acc[3] = {
                static_cast<vqf_real_t>(sample.accel_x_g * kGravityMps2),
                static_cast<vqf_real_t>(sample.accel_y_g * kGravityMps2),
                static_cast<vqf_real_t>(sample.accel_z_g * kGravityMps2),
            };

            vqf.update(gyr, acc);

            vqf_real_t quat[4] = {};
            vqf.getQuat6D(quat);

            hardware::EulerAngles angles = {};
            const hardware::Quaternion orientation{
                static_cast<float>(quat[0]),
                static_cast<float>(quat[1]),
                static_cast<float>(quat[2]),
                static_cast<float>(quat[3]),
            };
            const hardware::Icm42688Status angle_status =
                hardware::quaternionToEulerAngles(orientation, angles);
            if (angle_status != hardware::Icm42688Status::ok) {
                angles = {};
            }

            std::printf("%.6f,%.6f,%.6f,%.6f,%.3f,%.3f,%.3f,%.3f,"
                        "%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n",
                        static_cast<double>(quat[0]),
                        static_cast<double>(quat[1]),
                        static_cast<double>(quat[2]),
                        static_cast<double>(quat[3]),
                        static_cast<double>(angles.roll_deg),
                        static_cast<double>(angles.pitch_deg),
                        static_cast<double>(angles.yaw_deg),
                        static_cast<double>(sample.temperature_c),
                        static_cast<double>(sample.accel_x_g),
                        static_cast<double>(sample.accel_y_g),
                        static_cast<double>(sample.accel_z_g),
                        static_cast<double>(sample.gyro_x_dps),
                        static_cast<double>(sample.gyro_y_dps),
                        static_cast<double>(sample.gyro_z_dps));
            std::fflush(stdout);
        } else {
            std::fprintf(stderr,
                         "failed to read ICM42688 sample: %s\n",
                         hardware::toString(status));
        }

        next_wake += period;
        std::this_thread::sleep_until(next_wake);
    }
}

} // namespace app
