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

/// Build the LAWICEL status byte from controller counters. Bit 3 is error-passive, bit 5
/// arbitration lost, bit 6 bus error, bit 7 bus-off; bits 0 and 1 are the queue overruns.
uint8_t status_flags(bool rx_overrun, bool error_passive, bool arbitration_lost,
                     bool bus_error, bool bus_off);

}  // namespace bench
