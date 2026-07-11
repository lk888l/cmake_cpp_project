#pragma once

#include "bsp_status.hpp"

#include <cstddef>
#include <cstdint>

namespace bsp {

class SpiBus {
public:
    virtual ~SpiBus() = default;
    virtual Status init() = 0;
    virtual Status write(const uint8_t* data, size_t length) = 0;
    virtual void deinit() = 0;
    virtual bool isInitialized() const = 0;
};

} // namespace bsp

