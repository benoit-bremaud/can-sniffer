#include "activity.h"

namespace bench {

void ActivityPulse::pulse(uint32_t now) {
    started_ = now;
    armed_ = true;
}

bool ActivityPulse::active(uint32_t now) const {
    // Unsigned subtraction is wrap-safe; a stale pulse expires instead of latching on.
    return armed_ && static_cast<uint32_t>(now - started_) < kPulseMs;
}

}  // namespace bench
