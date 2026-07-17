#pragma once

#include "bsp/bsp_status.hpp"

namespace bsp {

class OutputPin {
public:
    virtual ~OutputPin() = default;
    virtual Status init(bool initial_high) = 0;
    virtual Status set(bool high) = 0;
    virtual void deinit() = 0;
    [[nodiscard]] virtual bool isInitialized() const = 0;
};

class NullOutputPin final : public OutputPin {
public:
    Status init(bool) override { return Status::ok; }
    Status set(bool) override { return Status::ok; }
    void deinit() override {}
    [[nodiscard]] bool isInitialized() const override { return true; }
};

} // namespace bsp
