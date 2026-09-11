#pragma once
#include <cstdint>

namespace bench {

/// Non-blocking one-shot indicator. Owns no hardware: the caller drives the pin from
/// active(). Time is uint32_t milliseconds and wraps safely, like the rest of the bench.
class ActivityPulse {
public:
    // 250 ms against the 1 Hz frame cadence: a 25 % duty flash, countable by eye. Shorter
    // pulses were measured to be missed by an operator watching the receiver window.
    static constexpr uint32_t kPulseMs = 250;
    /// (Re)arm a full-length pulse; calling during a pulse extends it from now.
    void pulse(uint32_t now);
    /// True while the pulse is still due. A pulse never survives its own duration, and
    /// expiry is latched, so a millis() wrap can never bring a stale pulse back.
    bool active(uint32_t now);
private:
    uint32_t started_ = 0;
    bool armed_ = false;
};

}  // namespace bench
