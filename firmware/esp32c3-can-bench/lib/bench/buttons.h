#pragma once
#include "scenarios.h"

namespace bench {
constexpr int kBluePin = 0;
constexpr int kRedPin = 1;
constexpr int kWhitePin = 5;

/// Whole-mask debounce. Bits 0/1/2 are blue/red/white; other bits are invalid.
class DebouncedButtons {
public:
    static constexpr uint32_t kDebounceMs = 30;
    /// True only after the same raw mask has been observed for at least 30 ms.
    bool sample(uint8_t raw, uint32_t now);
    uint8_t mask() const { return raw_; }
    void reset() { initialized_ = false; }
private:
    bool initialized_ = false;
    uint8_t raw_ = 0;
    uint32_t changed_ = 0;
};

enum class ButtonState { ReleaseWait, Ready, Running, Fault };
struct RunCounts {
    uint32_t queued = 0, succeeded = 0, failed = 0, aborted = 0;
};

/// Owns controller commands in the buttons profile. Fault cannot be cleared by stop().
class ButtonTestRunner {
public:
    explicit ButtonTestRunner(BenchController& controller) : controller_(controller) {}
    /// Consume completions/faults without allowing an emission.
    void service(uint32_t now);
    /// Sample input, accept at most one selection, then schedule at most one attempt.
    void tick(uint32_t now, uint8_t raw_mask);
    /// Cancel and require fresh release; fault remains latched even after cleanup succeeds.
    void stop(uint32_t now);
    ButtonState state() const { return state_; }
    Scenario scenario() const { return scenario_; }
    RunCounts counts() const;
private:
    void finish(uint32_t now);
    BenchController& controller_;
    DebouncedButtons buttons_;
    ButtonState state_ = ButtonState::ReleaseWait;
    Scenario scenario_ = Scenario::None;
    Status baseline_;
};
const char* button_state_name(ButtonState state);
}  // namespace bench
