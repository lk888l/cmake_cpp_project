#include "platform/rockchip/media_pipeline.hpp"

#include <im2d.h>
#include <rk_aiq_user_api2_sysctl.h>
#include <rk_mpi_mb.h>
#include <rk_mpi_sys.h>
#include <rk_mpi_vi.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <fstream>
#include <glob.h>
#include <sstream>
#include <utility>

namespace camera_display {
namespace {

// Rockit's ISP path mapping on RV1103 is fixed: mainpath=0, selfpath=1,
// bypasspath=2. The application owns mainpath after rkipc has stopped.
constexpr int kViChannel = 0;
constexpr int kRgaSuccess = IM_STATUS_SUCCESS;
constexpr const char* kDmaHeap = "/dev/rk_dma_heap/rk-dma-heap-cma";

std::string hexadecimal(int value)
{
    std::ostringstream stream;
    stream << "0x" << std::hex << value;
    return stream.str();
}

std::string readLine(const std::string& path)
{
    std::ifstream input(path);
    std::string line;
    std::getline(input, line);
    return line;
}

std::string resolveMainPath(const std::string& configured)
{
    if (!configured.empty() && configured.front() == '/') return configured;
    const std::string wanted =
        configured.empty() ? "rkisp_mainpath" : configured;
    glob_t matches{};
    std::string result;
    if (::glob("/sys/class/video4linux/video*/name", GLOB_NOSORT,
               nullptr, &matches) == 0) {
        for (std::size_t index = 0; index < matches.gl_pathc; ++index) {
            const std::string path{matches.gl_pathv[index]};
            if (readLine(path) != wanted) continue;
            const std::size_t nameEnd = path.rfind("/name");
            const std::size_t nodeStart = path.rfind('/', nameEnd - 1U);
            if (nameEnd != std::string::npos
                && nodeStart != std::string::npos) {
                result = "/dev/" + path.substr(
                    nodeStart + 1U, nameEnd - nodeStart - 1U);
                break;
            }
        }
    }
    ::globfree(&matches);
    return result;
}

} // namespace

struct RockchipMediaPipeline::Impl final {
    rk_aiq_sys_ctx_t* aiq_context{};
    bool system_started{};
    bool vi_device_enabled_by_us{};
    bool vi_channel_enabled{};
    bool rga_started{};
    bool frame_outstanding{};
    VIDEO_FRAME_INFO_S current_frame{};
    std::array<int, 2> source_fds{{-1, -1}};
    std::array<rga_buffer_handle_t, 2> source_handles{};
};

RockchipMediaPipeline::RockchipMediaPipeline(CameraConfig config,
                                             std::uint16_t outputWidth,
                                             std::uint16_t outputHeight)
    : config_(std::move(config)),
      output_width_(outputWidth),
      output_height_(outputHeight),
      impl_(new Impl)
{
}

RockchipMediaPipeline::~RockchipMediaPipeline()
{
    stop();
}

void RockchipMediaPipeline::setError(MediaStage stage, std::string message)
{
    std::lock_guard<std::mutex> lock(error_mutex_);
    failed_stage_.store(stage);
    last_error_ = std::move(message);
}

std::string RockchipMediaPipeline::lastError() const
{
    std::lock_guard<std::mutex> lock(error_mutex_);
    return last_error_;
}

bool RockchipMediaPipeline::startRkaiq()
{
    rk_aiq_static_info_t information{};
    const int cameraId = static_cast<int>(config_.camera_id);
    int result = rk_aiq_uapi2_sysctl_enumStaticMetasByPhyId(
        cameraId, &information);
    if (result != 0) {
        setError(MediaStage::Rkaiq,
                 "cannot enumerate sensor metadata: " + hexadecimal(result));
        return false;
    }
    if (information.sensor_info.sensor_name[0] == '\0') {
        setError(MediaStage::Rkaiq, "RKAIQ returned an empty sensor name");
        return false;
    }
    result = rk_aiq_uapi2_sysctl_preInit_devBufCnt(
        information.sensor_info.sensor_name, "rkraw_rx",
        static_cast<int>(config_.buffer_count));
    if (result != 0) {
        setError(MediaStage::Rkaiq,
                 "RKAIQ buffer pre-initialization failed: "
                     + hexadecimal(result));
        return false;
    }
    impl_->aiq_context = rk_aiq_uapi2_sysctl_init(
        information.sensor_info.sensor_name,
        config_.iq_directory.c_str(), nullptr, nullptr);
    if (impl_->aiq_context == nullptr) {
        setError(MediaStage::Rkaiq,
                 "RKAIQ initialization failed (camera may be occupied)");
        return false;
    }
    result = rk_aiq_uapi2_sysctl_prepare(
        impl_->aiq_context, 0, 0, RK_AIQ_WORKING_MODE_NORMAL);
    if (result != 0) {
        setError(MediaStage::Rkaiq,
                 "RKAIQ prepare failed: " + hexadecimal(result));
        return false;
    }
    result = rk_aiq_uapi2_sysctl_start(impl_->aiq_context);
    if (result != 0) {
        setError(MediaStage::Rkaiq,
                 "RKAIQ start failed: " + hexadecimal(result));
        return false;
    }
    return true;
}

bool RockchipMediaPipeline::startVideoInput()
{
    int result = RK_MPI_SYS_Init();
    if (result != RK_SUCCESS) {
        setError(MediaStage::System,
                 "RK_MPI_SYS_Init failed: " + hexadecimal(result));
        return false;
    }
    impl_->system_started = true;

    const VI_DEV device = static_cast<VI_DEV>(config_.camera_id);
    const VI_PIPE pipe = static_cast<VI_PIPE>(config_.camera_id);
    VI_DEV_ATTR_S deviceAttributes{};
    result = RK_MPI_VI_GetDevAttr(device, &deviceAttributes);
    if (result == RK_ERR_VI_NOT_CONFIG) {
        result = RK_MPI_VI_SetDevAttr(device, &deviceAttributes);
        if (result != RK_SUCCESS) {
            setError(MediaStage::VideoInput,
                     "RK_MPI_VI_SetDevAttr failed: " + hexadecimal(result));
            return false;
        }
    }
    else if (result != RK_SUCCESS) {
        setError(MediaStage::VideoInput,
                 "RK_MPI_VI_GetDevAttr failed: " + hexadecimal(result));
        return false;
    }

    result = RK_MPI_VI_GetDevIsEnable(device);
    if (result != RK_SUCCESS) {
        result = RK_MPI_VI_EnableDev(device);
        if (result != RK_SUCCESS) {
            setError(MediaStage::VideoInput,
                     "RK_MPI_VI_EnableDev failed: " + hexadecimal(result));
            return false;
        }
        impl_->vi_device_enabled_by_us = true;
        VI_DEV_BIND_PIPE_S binding{};
        binding.u32Num = 1;
        binding.PipeId[0] = pipe;
        result = RK_MPI_VI_SetDevBindPipe(device, &binding);
        if (result != RK_SUCCESS) {
            setError(MediaStage::VideoInput,
                     "RK_MPI_VI_SetDevBindPipe failed: "
                         + hexadecimal(result));
            return false;
        }
    }

    VI_CHN_ATTR_S attributes{};
    attributes.stIspOpt.u32BufCount = config_.buffer_count;
    attributes.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;
    const std::string mainPath = resolveMainPath(config_.entity_name);
    if (mainPath.empty()) {
        setError(MediaStage::VideoInput,
                 "cannot resolve a video node named rkisp_mainpath");
        return false;
    }
    std::strncpy(attributes.stIspOpt.aEntityName, mainPath.c_str(),
                 sizeof(attributes.stIspOpt.aEntityName) - 1U);
    attributes.stSize.u32Width = config_.width;
    attributes.stSize.u32Height = config_.height;
    attributes.u32Depth = 2;
    attributes.enPixelFormat = RK_FMT_YUV420SP;
    attributes.enCompressMode = COMPRESS_MODE_NONE;
    attributes.stFrameRate.s32SrcFrameRate =
        static_cast<RK_S32>(config_.target_fps);
    attributes.stFrameRate.s32DstFrameRate =
        static_cast<RK_S32>(config_.target_fps);

    result = RK_MPI_VI_SetChnAttr(pipe, kViChannel, &attributes);
    if (result != RK_SUCCESS) {
        setError(MediaStage::VideoInput,
                 "RK_MPI_VI_SetChnAttr failed: " + hexadecimal(result));
        return false;
    }
    result = RK_MPI_VI_EnableChn(pipe, kViChannel);
    if (result != RK_SUCCESS) {
        setError(MediaStage::VideoInput,
                 "RK_MPI_VI_EnableChn failed: " + hexadecimal(result)
                     + " (stop rkipc first)");
        return false;
    }
    impl_->vi_channel_enabled = true;
    return true;
}

bool RockchipMediaPipeline::startRga()
{
    const std::size_t outputBytes =
        static_cast<std::size_t>(output_width_) * output_height_ * 2U;
    for (std::size_t index = 0; index < outputs_.size(); ++index) {
        std::string error;
        if (!outputs_[index].allocate(kDmaHeap, outputBytes, error)) {
            setError(MediaStage::Rga, error);
            return false;
        }
        const rga_buffer_handle_t handle =
            importbuffer_fd(outputs_[index].fd(),
                            static_cast<int>(outputs_[index].size()));
        if (handle <= 0) {
            setError(MediaStage::Rga,
                     "RGA could not import output DMA buffer "
                         + std::to_string(index));
            return false;
        }
        rga_output_handles_[index] =
            static_cast<std::uintptr_t>(handle);
    }

    // Warm/import both VI DMABUFs before worker threads begin. This keeps all
    // RGA mapping allocation out of the steady-state path.
    std::array<VIDEO_FRAME_INFO_S, 2> warmFrames{};
    std::size_t acquired{};
    const VI_PIPE pipe = static_cast<VI_PIPE>(config_.camera_id);
    for (; acquired < warmFrames.size(); ++acquired) {
        const int result = RK_MPI_VI_GetChnFrame(
            pipe, kViChannel, &warmFrames[acquired], 1000);
        if (result != RK_SUCCESS) {
            for (std::size_t releaseIndex = 0;
                 releaseIndex < acquired; ++releaseIndex) {
                (void)RK_MPI_VI_ReleaseChnFrame(
                    pipe, kViChannel, &warmFrames[releaseIndex]);
            }
            setError(MediaStage::Rga,
                     "cannot acquire VI buffers for RGA warm-up: "
                         + hexadecimal(result));
            return false;
        }
    }
    for (std::size_t index = 0; index < warmFrames.size(); ++index) {
        const VIDEO_FRAME_S& frame = warmFrames[index].stVFrame;
        const int fd = RK_MPI_MB_Handle2Fd(frame.pMbBlk);
        const int byteSize = static_cast<int>(
            frame.u32VirWidth * frame.u32VirHeight * 3U / 2U);
        impl_->source_fds[index] = fd;
        impl_->source_handles[index] = importbuffer_fd(fd, byteSize);
        if (impl_->source_handles[index] <= 0) {
            for (const auto& warmFrame : warmFrames) {
                VIDEO_FRAME_INFO_S copy = warmFrame;
                (void)RK_MPI_VI_ReleaseChnFrame(
                    pipe, kViChannel, &copy);
            }
            setError(MediaStage::Rga,
                     "RGA could not import a VI DMA buffer");
            return false;
        }
    }
    for (auto& warmFrame : warmFrames) {
        (void)RK_MPI_VI_ReleaseChnFrame(pipe, kViChannel, &warmFrame);
    }
    if (impl_->source_fds[0] == impl_->source_fds[1]) {
        setError(MediaStage::Rga,
                 "VI warm-up returned the same DMA buffer twice");
        return false;
    }
    impl_->rga_started = true;
    return true;
}

MediaStatus RockchipMediaPipeline::start()
{
    if (impl_->system_started || impl_->aiq_context != nullptr) {
        return MediaStatus::InvalidState;
    }
    {
        std::lock_guard<std::mutex> lock(error_mutex_);
        failed_stage_.store(MediaStage::None);
        last_error_.clear();
    }
    if (!startRkaiq() || !startVideoInput() || !startRga()) {
        stop();
        if (lastError().find("occupied") != std::string::npos) {
            return MediaStatus::Busy;
        }
        return MediaStatus::IoError;
    }
    return MediaStatus::Ok;
}

MediaStatus RockchipMediaPipeline::acquire(
    CapturedFrame& frame, std::chrono::milliseconds timeout)
{
    if (!impl_->vi_channel_enabled || impl_->frame_outstanding) {
        return MediaStatus::InvalidState;
    }
    const auto count = timeout.count();
    const int timeoutMs = static_cast<int>(
        std::max<std::int64_t>(0, std::min<std::int64_t>(count, INT32_MAX)));
    const VI_PIPE pipe = static_cast<VI_PIPE>(config_.camera_id);
    const int result = RK_MPI_VI_GetChnFrame(
        pipe, kViChannel, &impl_->current_frame, timeoutMs);
    if (result != RK_SUCCESS) return MediaStatus::Timeout;

    impl_->frame_outstanding = true;
    const VIDEO_FRAME_S& native = impl_->current_frame.stVFrame;
    frame.dma_fd = RK_MPI_MB_Handle2Fd(native.pMbBlk);
    frame.virtual_address = RK_MPI_MB_Handle2VirAddr(native.pMbBlk);
    frame.width = native.u32Width;
    frame.height = native.u32Height;
    frame.horizontal_stride = native.u32VirWidth;
    frame.vertical_stride = native.u32VirHeight;
    frame.sequence = native.u32TimeRef;
    frame.sensor_timestamp_us = native.u64PTS;
    frame.acquired_at = std::chrono::steady_clock::now();
    frame.native_handle = 1;
    return MediaStatus::Ok;
}

void RockchipMediaPipeline::release(CapturedFrame& frame) noexcept
{
    if (!impl_->frame_outstanding || frame.native_handle == 0) return;
    const VI_PIPE pipe = static_cast<VI_PIPE>(config_.camera_id);
    (void)RK_MPI_VI_ReleaseChnFrame(
        pipe, kViChannel, &impl_->current_frame);
    impl_->frame_outstanding = false;
    frame = {};
}

MediaStatus RockchipMediaPipeline::convert(
    const CapturedFrame& source, const Rect& sourceCrop,
    std::size_t outputSlot)
{
    if (!impl_->rga_started || outputSlot >= outputs_.size()
        || source.dma_fd < 0 || sourceCrop.width == 0
        || sourceCrop.height == 0
        || static_cast<std::uint32_t>(sourceCrop.x)
                + sourceCrop.width > source.width
        || static_cast<std::uint32_t>(sourceCrop.y)
                + sourceCrop.height > source.height) {
        return MediaStatus::InvalidArgument;
    }
    std::size_t sourceIndex = impl_->source_fds.size();
    for (std::size_t index = 0; index < impl_->source_fds.size(); ++index) {
        if (impl_->source_fds[index] == source.dma_fd) {
            sourceIndex = index;
            break;
        }
    }
    if (sourceIndex == impl_->source_fds.size()) {
        setError(MediaStage::Rga,
                 "VI returned an unregistered DMA buffer (buffer count changed)");
        return MediaStatus::InvalidState;
    }

    const auto sourceHandle = impl_->source_handles[sourceIndex];
    const auto outputHandle = static_cast<rga_buffer_handle_t>(
        rga_output_handles_[outputSlot]);
    rga_buffer_t sourceBuffer = wrapbuffer_handle(
        sourceHandle, static_cast<int>(source.width),
        static_cast<int>(source.height), RK_FORMAT_YCbCr_420_SP,
        static_cast<int>(source.horizontal_stride),
        static_cast<int>(source.vertical_stride));
    rga_buffer_t outputBuffer = wrapbuffer_handle(
        outputHandle, output_width_, output_height_, RK_FORMAT_RGB_565,
        output_width_, output_height_);
    rga_buffer_t emptyBuffer{};
    const im_rect sourceRectangle{
        static_cast<int>(sourceCrop.x), static_cast<int>(sourceCrop.y),
        static_cast<int>(sourceCrop.width), static_cast<int>(sourceCrop.height)};
    const im_rect outputRectangle{
        0, 0, static_cast<int>(output_width_),
        static_cast<int>(output_height_)};
    const im_rect emptyRectangle{};
    const IM_STATUS result = improcess(
        sourceBuffer, outputBuffer, emptyBuffer,
        sourceRectangle, outputRectangle, emptyRectangle,
        -1, nullptr, nullptr, IM_SYNC);
    if (result != kRgaSuccess) {
        setError(MediaStage::Rga,
                 std::string{"RGA improcess failed: "} + imStrError(result));
        return MediaStatus::IoError;
    }
    return MediaStatus::Ok;
}

ConvertedFrame RockchipMediaPipeline::output(std::size_t slot) noexcept
{
    if (slot >= outputs_.size()) return {};
    return {
        outputs_[slot].fd(),
        static_cast<std::uint16_t*>(outputs_[slot].data()),
        output_width_,
        output_height_,
        outputs_[slot].size()};
}

bool RockchipMediaPipeline::beginCpuRead(std::size_t slot)
{
    if (slot >= outputs_.size()) return false;
    std::string error;
    if (outputs_[slot].beginCpuRead(error)) return true;
    setError(MediaStage::Rga, std::move(error));
    return false;
}

bool RockchipMediaPipeline::endCpuRead(std::size_t slot) noexcept
{
    if (slot >= outputs_.size()) return false;
    std::string error;
    const bool success = outputs_[slot].endCpuRead(error);
    if (!success) setError(MediaStage::Rga, std::move(error));
    return success;
}

void RockchipMediaPipeline::stopVideoInput() noexcept
{
    if (impl_->frame_outstanding) {
        CapturedFrame frame;
        frame.native_handle = 1;
        release(frame);
    }
    const VI_PIPE pipe = static_cast<VI_PIPE>(config_.camera_id);
    const VI_DEV device = static_cast<VI_DEV>(config_.camera_id);
    if (impl_->vi_channel_enabled) {
        (void)RK_MPI_VI_DisableChn(pipe, kViChannel);
        impl_->vi_channel_enabled = false;
    }
    if (impl_->vi_device_enabled_by_us) {
        (void)RK_MPI_VI_DisableDev(device);
        impl_->vi_device_enabled_by_us = false;
    }
    if (impl_->system_started) {
        (void)RK_MPI_SYS_Exit();
        impl_->system_started = false;
    }
}

void RockchipMediaPipeline::stopRkaiq() noexcept
{
    if (impl_->aiq_context == nullptr) return;
    (void)rk_aiq_uapi2_sysctl_stop(impl_->aiq_context, false);
    rk_aiq_uapi2_sysctl_deinit(impl_->aiq_context);
    impl_->aiq_context = nullptr;
}

void RockchipMediaPipeline::stop() noexcept
{
    for (auto& handle : impl_->source_handles) {
        if (handle > 0) {
            (void)releasebuffer_handle(handle);
            handle = 0;
        }
    }
    impl_->source_fds.fill(-1);
    for (std::size_t index = 0; index < rga_output_handles_.size(); ++index) {
        if (rga_output_handles_[index] != 0) {
            (void)releasebuffer_handle(static_cast<rga_buffer_handle_t>(
                rga_output_handles_[index]));
            rga_output_handles_[index] = 0;
        }
        outputs_[index].reset();
    }
    impl_->rga_started = false;
    stopVideoInput();
    stopRkaiq();
}

std::string RockchipMediaPipeline::description() const
{
    std::ostringstream stream;
    stream << "RKAIQ/VI NV12 " << config_.width << 'x' << config_.height
           << '@' << config_.target_fps << " -> RGA RGB565 "
           << output_width_ << 'x' << output_height_;
    return stream.str();
}

} // namespace camera_display
