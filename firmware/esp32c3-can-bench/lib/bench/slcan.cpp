#include "slcan.h"

namespace bench {
namespace {

constexpr char kCr = '\r';
constexpr char kLf = '\n';

char hex_char(uint8_t nibble) {
    return static_cast<char>(nibble < 10 ? '0' + nibble : 'A' + (nibble - 10));
}

/// Map a LAWICEL speed digit onto a rate the controller can actually produce.
///
/// Two codes are deliberately absent. 9 is 83.3 kbit/s, which the SDK does not offer. 7 is
/// worse than missing: LAWICEL and the SDK call it 800 kbit/s while python-can sends it for
/// 750, so honouring it would mean guessing which host is talking. An ambiguous code is
/// refused rather than resolved by assumption.
bool speed_code(char digit, Bitrate& bitrate) {
    switch (digit) {
    case '0': bitrate = Bitrate::K10; return true;
    case '1': bitrate = Bitrate::K20; return true;
    case '2': bitrate = Bitrate::K50; return true;
    case '3': bitrate = Bitrate::K100; return true;
    case '4': bitrate = Bitrate::K125; return true;
    case '5': bitrate = Bitrate::K250; return true;
    case '6': bitrate = Bitrate::K500; return true;
    case '8': bitrate = Bitrate::M1; return true;
    default: return false;
    }
}

/// C++11 forbids aggregate-initialising a struct that carries member initialisers, so
/// build requests here rather than dropping the defaults from the header.
SlcanRequest make(SlcanCommand command, Bitrate bitrate = Bitrate::K125) {
    SlcanRequest request;
    request.command = command;
    request.bitrate = bitrate;
    return request;
}

}  // namespace

void SlcanParser::reset() {
    size_ = 0;
    overflowed_ = false;
    line_[0] = '\0';
}

SlcanRequest SlcanParser::feed(char byte) {
    if (byte == kLf) { return make(SlcanCommand::None); }
    if (byte != kCr) {
        if (size_ < kSlcanLineLimit) {
            line_[size_++] = byte;
        } else {
            overflowed_ = true;
        }
        return make(SlcanCommand::None);
    }
    line_[size_] = '\0';
    const SlcanRequest request = overflowed_ ? make(SlcanCommand::Invalid) : interpret();
    reset();
    return request;
}

SlcanRequest SlcanParser::interpret() const {
    if (size_ == 0) { return make(SlcanCommand::None); }
    switch (line_[0]) {
    case 'C':
        return make(size_ == 1 ? SlcanCommand::Close : SlcanCommand::Invalid);
    case 'O':
        return make(size_ == 1 ? SlcanCommand::OpenNormal : SlcanCommand::Invalid);
    case 'L':
        return make(size_ == 1 ? SlcanCommand::OpenListen : SlcanCommand::Invalid);
    case 'F':
        return make(size_ == 1 ? SlcanCommand::Status : SlcanCommand::Invalid);
    case 'V':
    case 'N':
        return make(size_ == 1 ? SlcanCommand::Version : SlcanCommand::Invalid);
    case 't':
    case 'T':
    case 'r':
    case 'R':
    case 'x':
        // Named rather than merged into Invalid: refusing to transmit is the property that
        // makes this profile safe to point at a bus, so it is reported and tested as such.
        return make(SlcanCommand::Transmit);
    case 'S': {
        Bitrate bitrate = Bitrate::K125;
        if (size_ != 2 || !speed_code(line_[1], bitrate)) {
            return make(SlcanCommand::Invalid);
        }
        return make(SlcanCommand::SetBitrate, bitrate);
    }
    default:
        return make(SlcanCommand::Invalid);
    }
}

std::size_t encode_frame(const SlcanFrame& frame, char* out, std::size_t capacity) {
    if (out == nullptr || capacity < kSlcanFrameChars) { return 0; }
    if (frame.dlc > 8) { return 0; }
    const uint32_t limit = frame.extended ? 0x1FFFFFFFu : 0x7FFu;
    if (frame.id > limit) { return 0; }

    std::size_t written = 0;
    if (frame.rtr) {
        out[written++] = frame.extended ? 'R' : 'r';
    } else {
        out[written++] = frame.extended ? 'T' : 't';
    }
    const unsigned digits = frame.extended ? 8u : 3u;
    for (unsigned i = 0; i < digits; ++i) {
        const unsigned shift = (digits - 1u - i) * 4u;
        out[written++] = hex_char(static_cast<uint8_t>((frame.id >> shift) & 0xFu));
    }
    out[written++] = hex_char(frame.dlc);
    if (!frame.rtr) {
        for (unsigned i = 0; i < frame.dlc; ++i) {
            out[written++] = hex_char(static_cast<uint8_t>(frame.data[i] >> 4));
            out[written++] = hex_char(static_cast<uint8_t>(frame.data[i] & 0x0Fu));
        }
    }
    out[written++] = kCr;
    out[written] = '\0';
    return written;
}

uint8_t status_flags(bool rx_overrun, bool error_passive, bool arbitration_lost,
                     bool bus_error, bool bus_off) {
    uint8_t flags = 0;
    if (rx_overrun) { flags |= 0x08; }
    if (error_passive) { flags |= 0x20; }
    if (arbitration_lost) { flags |= 0x40; }
    if (bus_error) { flags |= 0x80; }
    if (bus_off) { flags |= 0x80; }
    return flags;
}

}  // namespace bench
