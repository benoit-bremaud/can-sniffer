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
    /// Refreshes the driver snapshot, so the answer describes now rather than last time.
    uint8_t status() override;
    /// True when a frame was produced. Validation and DLC clamping stay in the adapter.
    bool poll(bench::SlcanFrame& frame);

private:
    TwaiPort& port_;
    bool open_ = false;
};
