#pragma once

#include "twai_port.h"

/// Hardware-boundary receive session. Never calls submit(), never starts at construction.
class BenchReceiver {
public:
    explicit BenchReceiver(TwaiPort& port) : port_(port) {}
    void start();
    void stop();
    /// Poll faults and receive at most one frame. True only when last_frame() changed.
    bool service();
    bench::State state() const { return state_; }
    bench::Fault fault() const { return fault_; }
    uint32_t count() const { return count_; }
    bool has_frame() const { return has_frame_; }
    const twai_message_t& last_frame() const { return last_; }
private:
    void fail(bench::Fault fault);
    TwaiPort& port_;
    bench::State state_ = bench::State::Stopped;
    bench::Fault fault_ = bench::Fault::None;
    uint32_t count_ = 0;
    bool has_frame_ = false;
    twai_message_t last_ = {};
};
