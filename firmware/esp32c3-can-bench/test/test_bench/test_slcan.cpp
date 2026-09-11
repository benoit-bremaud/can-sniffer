#include <unity.h>
#include <slcan.h>

#include <cstring>
#include <string>

using namespace bench;

namespace {

/// What the line produced, not what its last byte returned: a trailing LF is inert, so
/// reading only the final result would hide the command the terminator already emitted.
SlcanRequest feed_line(SlcanParser& parser, const std::string& line) {
    SlcanRequest produced;
    for (char byte : line) {
        const SlcanRequest request = parser.feed(byte);
        if (request.command != SlcanCommand::None) { produced = request; }
    }
    return produced;
}

std::string encode(const SlcanFrame& frame) {
    char out[kSlcanFrameChars] = {};
    const std::size_t written = encode_frame(frame, out, sizeof(out));
    return written == 0 ? std::string() : std::string(out, written);
}

SlcanFrame extended_frame() {
    SlcanFrame frame;
    frame.id = 0x001ABCDE;
    frame.extended = true;
    frame.dlc = 8;
    for (unsigned i = 0; i < 8; ++i) { frame.data[i] = static_cast<uint8_t>(i + 1); }
    return frame;
}

void parser_accepts_the_commands_the_host_sends() {
    // python-can writes exactly these: C, S<n>, then O or L.
    SlcanParser parser;
    TEST_ASSERT_EQUAL(static_cast<int>(SlcanCommand::Close),
                      static_cast<int>(feed_line(parser, "C\r").command));
    TEST_ASSERT_EQUAL(static_cast<int>(SlcanCommand::OpenNormal),
                      static_cast<int>(feed_line(parser, "O\r").command));
    TEST_ASSERT_EQUAL(static_cast<int>(SlcanCommand::OpenListen),
                      static_cast<int>(feed_line(parser, "L\r").command));
    TEST_ASSERT_EQUAL(static_cast<int>(SlcanCommand::Status),
                      static_cast<int>(feed_line(parser, "F\r").command));
    TEST_ASSERT_EQUAL(static_cast<int>(SlcanCommand::Version),
                      static_cast<int>(feed_line(parser, "V\r").command));
    // LF is tolerated so a hand-typed session behaves like the library.
    TEST_ASSERT_EQUAL(static_cast<int>(SlcanCommand::Close),
                      static_cast<int>(feed_line(parser, "C\r\n").command));
}

void parser_maps_only_the_bitrates_the_controller_can_produce() {
    SlcanParser parser;
    const struct { const char* line; uint32_t hertz; } supported[] = {
        {"S0\r", 10000},  {"S1\r", 20000},  {"S2\r", 50000},
        {"S3\r", 100000}, {"S4\r", 125000}, {"S5\r", 250000},
        {"S6\r", 500000}, {"S8\r", 1000000},
    };
    for (const auto& entry : supported) {
        const SlcanRequest request = feed_line(parser, entry.line);
        TEST_ASSERT_EQUAL(static_cast<int>(SlcanCommand::SetBitrate),
                          static_cast<int>(request.command));
        TEST_ASSERT_EQUAL_UINT32(entry.hertz, static_cast<uint32_t>(request.bitrate));
    }
    // S9 is 83.3k, which the SDK does not offer. S7 is worse than missing: LAWICEL and the
    // SDK call it 800k while python-can sends it for 750k, so honouring it would mean
    // guessing which host is talking. An ambiguous code is refused, never resolved.
    for (const char* line : {"S7\r", "S9\r", "S\r", "SA\r", "S44\r"}) {
        TEST_ASSERT_EQUAL(static_cast<int>(SlcanCommand::Invalid),
                          static_cast<int>(feed_line(parser, line).command));
    }
}

void parser_always_refuses_to_transmit() {
    // The safety property of this profile, so it is named and asserted rather than folded
    // into the generic invalid case.
    SlcanParser parser;
    for (const char* line : {"t1238001122334455667788\r", "T001ABCDE80102030405060708\r",
                             "r1230\r", "R001ABCDE0\r", "x001ABCDE0\r"}) {
        TEST_ASSERT_EQUAL(static_cast<int>(SlcanCommand::Transmit),
                          static_cast<int>(feed_line(parser, line).command));
    }
}

void parser_rejects_malformed_and_overlong_lines() {
    SlcanParser parser;
    for (const char* line : {"Z\r", "CC\r", "OO\r", "LL\r", "FF\r", "VV\r"}) {
        TEST_ASSERT_EQUAL(static_cast<int>(SlcanCommand::Invalid),
                          static_cast<int>(feed_line(parser, line).command));
    }
    // An empty line is silence, not an error: the host sends bare terminators.
    TEST_ASSERT_EQUAL(static_cast<int>(SlcanCommand::None),
                      static_cast<int>(feed_line(parser, "\r").command));
    // Overlong input is refused as a whole; its tail must never execute as a command.
    const SlcanRequest request = feed_line(parser, std::string(kSlcanLineLimit + 4, 'C') + "\r");
    TEST_ASSERT_EQUAL(static_cast<int>(SlcanCommand::Invalid), static_cast<int>(request.command));
    TEST_ASSERT_EQUAL(static_cast<int>(SlcanCommand::Close),
                      static_cast<int>(feed_line(parser, "C\r").command));
    // A partial line is dropped by reset rather than joined to the next one.
    parser.feed('C');
    parser.reset();
    TEST_ASSERT_EQUAL(static_cast<int>(SlcanCommand::OpenListen),
                      static_cast<int>(feed_line(parser, "L\r").command));
}

void encoder_renders_frames_the_host_can_parse() {
    // Exactly the layout python-can's _recv_internal expects: T, 8 id digits, dlc, data.
    TEST_ASSERT_EQUAL_STRING("T001ABCDE80102030405060708\r", encode(extended_frame()).c_str());

    SlcanFrame standard;
    standard.id = 0x123;
    standard.dlc = 2;
    standard.data[0] = 0xAB;
    standard.data[1] = 0x00;
    TEST_ASSERT_EQUAL_STRING("t1232AB00\r", encode(standard).c_str());

    SlcanFrame empty;
    empty.id = 0x7FF;
    TEST_ASSERT_EQUAL_STRING("t7FF0\r", encode(empty).c_str());

    SlcanFrame remote;
    remote.id = 0x001ABCDE;
    remote.extended = true;
    remote.rtr = true;
    remote.dlc = 8;
    remote.data[0] = 0xFF;  // Ignored: a remote frame carries no payload.
    TEST_ASSERT_EQUAL_STRING("R001ABCDE8\r", encode(remote).c_str());
}

void encoder_refuses_what_it_cannot_represent() {
    SlcanFrame frame = extended_frame();
    frame.dlc = 9;  // ESP-IDF reports DLC 9-15 verbatim; never write past the buffer.
    TEST_ASSERT_EQUAL(0, encode(frame).size());

    SlcanFrame wide;
    wide.id = 0x800;  // Too wide for an eleven-bit identifier.
    TEST_ASSERT_EQUAL(0, encode(wide).size());

    SlcanFrame huge;
    huge.id = 0x20000000;
    huge.extended = true;
    TEST_ASSERT_EQUAL(0, encode(huge).size());

    char small[4] = {};
    TEST_ASSERT_EQUAL(0, encode_frame(extended_frame(), small, sizeof(small)));
    TEST_ASSERT_EQUAL(0, encode_frame(extended_frame(), nullptr, kSlcanFrameChars));
}

void status_byte_follows_the_lawicel_layout() {
    TEST_ASSERT_EQUAL_HEX8(0x00, status_flags(false, false, false, false, false));
    TEST_ASSERT_EQUAL_HEX8(0x08, status_flags(true, false, false, false, false));
    TEST_ASSERT_EQUAL_HEX8(0x20, status_flags(false, true, false, false, false));
    TEST_ASSERT_EQUAL_HEX8(0x40, status_flags(false, false, true, false, false));
    TEST_ASSERT_EQUAL_HEX8(0x80, status_flags(false, false, false, true, false));
    TEST_ASSERT_EQUAL_HEX8(0x80, status_flags(false, false, false, false, true));
    TEST_ASSERT_EQUAL_HEX8(0xE8, status_flags(true, true, true, true, true));
}

}  // namespace

void run_slcan_tests() {
    RUN_TEST(parser_accepts_the_commands_the_host_sends);
    RUN_TEST(parser_maps_only_the_bitrates_the_controller_can_produce);
    RUN_TEST(parser_always_refuses_to_transmit);
    RUN_TEST(parser_rejects_malformed_and_overlong_lines);
    RUN_TEST(encoder_renders_frames_the_host_can_parse);
    RUN_TEST(encoder_refuses_what_it_cannot_represent);
    RUN_TEST(status_byte_follows_the_lawicel_layout);
}
