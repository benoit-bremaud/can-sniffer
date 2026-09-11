#pragma once

#include <cstddef>
#include <cstdint>

#include "bench.h"

namespace bench {

/// Longest line the host may send. Real commands are far shorter; anything longer is
/// rejected through its terminator rather than executed as a truncated prefix.
constexpr std::size_t kSlcanLineLimit = 32;
/// "T" + 8 id + 1 dlc + 16 data + CR + NUL.
constexpr std::size_t kSlcanFrameChars = 28;

/// A received frame, independent of any SDK type so the codec stays natively testable.
struct SlcanFrame {
    uint32_t id = 0;
    bool extended = false;
    bool rtr = false;
    uint8_t dlc = 0;
    uint8_t data[8] = {};
};

/// Host commands this adapter answers. Transmit requests are a distinct outcome rather
/// than simply invalid: refusing them is a safety property worth naming and testing.
enum class SlcanCommand {
    None,       ///< No complete line yet.
    Empty,      ///< A bare terminator, answered rather than ignored.
    Close,      ///< C
    OpenNormal, ///< O — acknowledges on the bus; bench use only.
    OpenListen, ///< L — TWAI_MODE_LISTEN_ONLY; the controller cannot influence the bus.
    SetBitrate, ///< S<n> with a rate this controller can actually produce.
    Status,     ///< F — LAWICEL status byte.
    Version,    ///< V
    Transmit,   ///< t/T/r/R — always refused: this profile has no transmit path.
    Invalid,    ///< Unknown, malformed, or a rate the controller cannot produce.
};

/// Outcome of feeding one byte: the command, plus its argument when it carries one.
struct SlcanRequest {
    SlcanCommand command = SlcanCommand::None;
    Bitrate bitrate = Bitrate::K125;  ///< Only meaningful for SetBitrate.
};

/// Line-based parser. Commands are terminated by CR; LF is tolerated and ignored so a
/// hand-typed terminal session behaves like the host library.
class SlcanParser {
public:
    /// None until a terminator arrives. An overlong line is refused as a whole, never
    /// executed as a prefix.
    SlcanRequest feed(char byte);
    /// Drop a partial line, for use when the link is reset.
    void reset();

private:
    SlcanRequest interpret() const;
    char line_[kSlcanLineLimit + 1] = {};
    std::size_t size_ = 0;
    bool overflowed_ = false;
};

/// Render a received frame as a LAWICEL line, CR included. Returns the length written,
/// or zero when the frame cannot be represented; `out` must hold kSlcanFrameChars bytes.
std::size_t encode_frame(const SlcanFrame& frame, char* out, std::size_t capacity);

/// What the session wants written back to the host. Points into the session's own buffer,
/// so it stays valid only until the next call.
struct SlcanReply {
    const char* text = "";
    std::size_t length = 0;
};

/// The channel a session drives. Kept SDK-free so the session is exercised natively; the
/// TWAI adapter and the test double are its two implementations.
class SlcanChannelPort {
public:
    virtual ~SlcanChannelPort() = default;
    /// Install and start in the requested mode. False must leave the channel closed.
    virtual bool open(Bitrate bitrate, bool listen_only) = 0;
    /// Cease and release. False means cleanup is unconfirmed.
    virtual bool close() = 0;
    /// LAWICEL status byte built from the controller counters.
    virtual uint8_t status() = 0;
};

/// Drives a channel from host commands and renders received frames.
///
/// Every command is answered, with CR on success and BEL on refusal. A silent adapter makes
/// "applied" indistinguishable from "ignored", which is what made the CANable undiagnosable.
class SlcanSession {
public:
    explicit SlcanSession(SlcanChannelPort& port) : port_(port) {}
    /// Feed one received byte; the reply is empty when the line is still incomplete.
    SlcanReply feed(char byte);
    /// Render a frame for the host. Empty when the frame cannot be represented.
    SlcanReply frame(const SlcanFrame& frame);
    /// Drop a partial line, for a link reset.
    void reset();
    bool is_open() const { return open_; }
    Bitrate bitrate() const { return bitrate_; }

private:
    SlcanReply reply(bool accepted);
    SlcanChannelPort& port_;
    SlcanParser parser_;
    char out_[kSlcanFrameChars] = {};
    Bitrate bitrate_ = Bitrate::K125;
    bool open_ = false;
};

/// Build the LAWICEL status byte from controller counters: bit 3 data overrun, bit 5
/// error-passive, bit 6 arbitration lost, bit 7 bus error. Bits 0 and 1, the host queue
/// overruns, are never set, and bus-off is reported on bit 7 like any other bus error —
/// LAWICEL defines no bit of its own for it.
uint8_t status_flags(bool rx_overrun, bool error_passive, bool arbitration_lost,
                     bool bus_error, bool bus_off);

}  // namespace bench
