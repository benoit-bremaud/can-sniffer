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
    /// What to do with a DLC above eight. ISO 11898-1 makes 9-15 legal, each meaning eight
    /// data bytes, and ESP-IDF reports the raw value: a generator treats it as a fault worth
    /// stopping for, a sniffer must show the frame rather than hide it.
    enum class DlcPolicy { Reject, Clamp };
    /// Nonblocking Classical CAN read; output changes only for a validated frame.
    ReceiveResult receive(twai_message_t& frame, DlcPolicy policy = DlcPolicy::Reject);
    /// Whether the controller may influence the bus at all. Listen-only is enforced by the
    /// peripheral: the SDK defines it as no transmissions and no acknowledgments.
    enum class Mode { Normal, ListenOnly };
    /// Select the mode for the next start. Ignored while installed; the channel is closed
    /// and reopened to change it, which is what the slcan protocol already requires.
    void configure(Mode mode) { mode_ = mode; }
    bool start(bench::Bitrate bitrate = bench::Bitrate::K125) override;
    bool submit(const bench::Frame& frame) override;
    bench::Result poll() override;
    bool stop() override;
private:
    Diagnostics diagnostics_;
    bool installed_ = false;
    bool bus_off_ = false;
    Mode mode_ = Mode::Normal;
};
