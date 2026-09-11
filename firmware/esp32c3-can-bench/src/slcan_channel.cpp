#include "slcan_channel.h"

namespace {

/// Error-passive is a counter threshold rather than a reported state, so derive it from
/// the counters the driver does expose.
constexpr uint32_t kErrorPassiveThreshold = 128;

}  // namespace

bool TwaiSlcanChannel::open(bench::Bitrate bitrate, bool listen_only) {
    port_.configure(listen_only ? TwaiPort::Mode::ListenOnly : TwaiPort::Mode::Normal);
    if (port_.start(bitrate)) {
        // A new session starts from a clean slate rather than inheriting the last one's.
        open_ = true;
        seen_overruns_ = 0;
        seen_arbitration_ = 0;
        seen_bus_errors_ = 0;
        return true;
    }
    // A failed start can still own the driver, so release it: leaving it installed would
    // make the next open fail for a reason unrelated to the bus.
    port_.stop();
    return false;
}

bool TwaiSlcanChannel::close() {
    const bool released = port_.stop();
    if (released) { open_ = false; }
    return released;
}

uint8_t TwaiSlcanChannel::status() {
    // poll() keeps its snapshot across cleanup by design, so a closed channel would
    // otherwise replay the flags of the session before it.
    if (!open_) { return 0; }
    port_.poll();
    const auto& diagnostic = port_.diagnostics();
    if (!diagnostic.status_available) {
        // Nothing readable: report no flags rather than invent them. The host sees a clean
        // byte and the absence of traffic tells it more than a fabricated error would.
        return 0;
    }
    const auto& info = diagnostic.status;
    const uint32_t overruns = info.rx_overrun_count + info.rx_missed_count;
    const uint8_t flags = bench::status_flags(
        overruns > seen_overruns_,
        // A state, not an event: the thresholds describe the controller right now.
        info.rx_error_counter >= kErrorPassiveThreshold ||
            info.tx_error_counter >= kErrorPassiveThreshold,
        info.arb_lost_count > seen_arbitration_,
        info.bus_error_count > seen_bus_errors_,
        info.state == TWAI_STATE_BUS_OFF);
    seen_overruns_ = overruns;
    seen_arbitration_ = info.arb_lost_count;
    seen_bus_errors_ = info.bus_error_count;
    return flags;
}

bool TwaiSlcanChannel::poll(bench::SlcanFrame& frame) {
    twai_message_t message = {};
    if (port_.receive(message) != TwaiPort::ReceiveResult::Frame) { return false; }
    frame.id = message.identifier;
    frame.extended = message.extd != 0;
    frame.rtr = message.rtr != 0;
    frame.dlc = message.data_length_code > 8 ? 8 : message.data_length_code;
    for (unsigned i = 0; i < frame.dlc; ++i) { frame.data[i] = message.data[i]; }
    return true;
}
