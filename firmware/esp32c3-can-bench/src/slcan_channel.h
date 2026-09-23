#pragma once

#include <slcan.h>

#include "twai_port.h"

/// Binds the SDK-free slcan session to the TWAI adapter.
///
/// Receive-only by construction: it exposes no way to submit a frame, so no host input can
/// make this profile transmit.
class TwaiSlcanChannel final : public bench::SlcanChannelPort {
public:
    explicit TwaiSlcanChannel(TwaiPort& port) : port_(port) {}

    bool open(bench::Bitrate bitrate, bool listen_only) override;
    bool close() override;
    /// Counter flags report what happened since the previous read, state flags report now.
    /// Real slcan firmware clears its flags on read; the driver counters only ever grow, so
    /// the difference is taken here or a single early fault would mask every later recovery.
    uint8_t status() override;
    /// True when a frame was produced. Validation and DLC clamping stay in the adapter.
    bool poll(bench::SlcanFrame& frame);

private:
    TwaiPort& port_;
    bool open_ = false;
    uint32_t seen_overruns_ = 0;
    uint32_t seen_arbitration_ = 0;
    uint32_t seen_bus_errors_ = 0;
};
