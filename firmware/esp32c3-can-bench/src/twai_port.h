#pragma once

#include <bench.h>
#include <driver/twai.h>

/// SDK boundary; no automatic recovery and no controller start in its constructor.
class TwaiPort final : public bench::CanPort {
public:
    /// Last poll, retained across cleanup; unavailable before polling or after a new start.
    struct Diagnostics {
        bool sampled = false;
        bool alerts_available = false;
        bool status_available = false;
        uint32_t alerts = 0;
        twai_status_info_t status = {};
    };
    const Diagnostics& diagnostics() const { return diagnostics_; }
    enum class ReceiveResult { Empty, Frame, Error };
    /// Nonblocking Classical CAN read; output changes only for a validated frame.
    ReceiveResult receive(twai_message_t& frame);
    bool start(bench::Bitrate bitrate = bench::Bitrate::K125) override;
    bool submit(const bench::Frame& frame) override;
    bench::Result poll() override;
    bool stop() override;
private:
    Diagnostics diagnostics_;
    bool installed_ = false;
    bool bus_off_ = false;
};
