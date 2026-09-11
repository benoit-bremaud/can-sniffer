#include "activity.h"

namespace bench {

void ActivityPulse::pulse(uint32_t now) {
    started_ = now;
    armed_ = true;
}

bool ActivityPulse::active(uint32_t now) {
    // The delta is wrap-safe, but armed_ must be cleared on expiry: left set, the window
    // re-opens every 2^32 ms and flashes ~49.7 days later with no transmission at all.
    if (armed_ && static_cast<uint32_t>(now - started_) >= kPulseMs) { armed_ = false; }
    return armed_;
}

}  // namespace bench
