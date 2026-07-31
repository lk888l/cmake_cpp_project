#include "platform/rockchip/dma_buffer.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <utility>

namespace camera_display {

DmaBuffer::~DmaBuffer()
{
    reset();
}

DmaBuffer::DmaBuffer(DmaBuffer&& other) noexcept
    : fd_(other.fd_),
      mapping_(other.mapping_),
      size_(other.size_),
      cpu_read_active_(other.cpu_read_active_)
{
    other.fd_ = -1;
    other.mapping_ = nullptr;
    other.size_ = 0;
    other.cpu_read_active_ = false;
}

DmaBuffer& DmaBuffer::operator=(DmaBuffer&& other) noexcept
{
    if (this == &other) return *this;
    reset();
    fd_ = other.fd_;
    mapping_ = other.mapping_;
    size_ = other.size_;
    cpu_read_active_ = other.cpu_read_active_;
    other.fd_ = -1;
    other.mapping_ = nullptr;
    other.size_ = 0;
    other.cpu_read_active_ = false;
    return *this;
}

bool DmaBuffer::allocate(const std::string& heapPath, std::size_t byteSize,
                         std::string& error)
{
    if (fd_ >= 0 || byteSize == 0) {
        error = "DMA buffer has invalid allocation state or size";
        return false;
    }
    int heapFd{};
    do {
        heapFd = ::open(heapPath.c_str(), O_RDONLY | O_CLOEXEC);
    } while (heapFd < 0 && errno == EINTR);
    if (heapFd < 0) {
        error = "cannot open " + heapPath + ": " + std::strerror(errno);
        return false;
    }

    dma_heap_allocation_data allocation{};
    allocation.len = byteSize;
    allocation.fd_flags = O_RDWR | O_CLOEXEC;
    int result{};
    do {
        result = ::ioctl(heapFd, DMA_HEAP_IOCTL_ALLOC, &allocation);
    } while (result < 0 && errno == EINTR);
    const int savedErrno = errno;
    (void)::close(heapFd);
    if (result < 0) {
        error = "DMA_HEAP_IOCTL_ALLOC failed: ";
        error += std::strerror(savedErrno);
        return false;
    }
    fd_ = static_cast<int>(allocation.fd);
    size_ = byteSize;
    mapping_ = ::mmap(nullptr, size_, PROT_READ | PROT_WRITE,
                      MAP_SHARED, fd_, 0);
    if (mapping_ == MAP_FAILED) {
        mapping_ = nullptr;
        error = "DMA buffer mmap failed: ";
        error += std::strerror(errno);
        reset();
        return false;
    }
    return true;
}

bool DmaBuffer::synchronize(unsigned long flags, std::string& error) noexcept
{
    if (fd_ < 0) {
        error = "DMA buffer is not allocated";
        return false;
    }
    dma_buf_sync sync{};
    sync.flags = flags;
    int result{};
    do {
        result = ::ioctl(fd_, DMA_BUF_IOCTL_SYNC, &sync);
    } while (result < 0 && errno == EINTR);
    if (result < 0) {
        error = "DMA_BUF_IOCTL_SYNC failed: ";
        error += std::strerror(errno);
        return false;
    }
    return true;
}

bool DmaBuffer::beginCpuRead(std::string& error) noexcept
{
    if (cpu_read_active_) return true;
    if (!synchronize(DMA_BUF_SYNC_START | DMA_BUF_SYNC_READ, error)) return false;
    cpu_read_active_ = true;
    return true;
}

bool DmaBuffer::endCpuRead(std::string& error) noexcept
{
    if (!cpu_read_active_) return true;
    if (!synchronize(DMA_BUF_SYNC_END | DMA_BUF_SYNC_READ, error)) return false;
    cpu_read_active_ = false;
    return true;
}

void DmaBuffer::reset() noexcept
{
    if (cpu_read_active_ && fd_ >= 0) {
        std::string ignored;
        (void)endCpuRead(ignored);
    }
    if (mapping_ != nullptr) {
        (void)::munmap(mapping_, size_);
        mapping_ = nullptr;
    }
    if (fd_ >= 0) {
        (void)::close(fd_);
        fd_ = -1;
    }
    size_ = 0;
}

} // namespace camera_display
