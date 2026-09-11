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

void parser_classifies_transmit_lines_distinctly_from_invalid_ones() {
    // Classification only. That nothing ever reaches the driver is proven by the session
    // test below and by application_slcan_speaks_protocol_only; a parser cannot prove it.
    SlcanParser parser;
    for (const char* line : {"t1238001122334455667788\r", "T001ABCDE80102030405060708\r",
                             "r1230\r", "R001ABCDE0\r", "x001ABCDE0\r"}) {
        TEST_ASSERT_EQUAL(static_cast<int>(SlcanCommand::Transmit),
                          static_cast<int>(feed_line(parser, line).command));
    }
}

void parser_rejects_malformed_and_overlong_lines() {
    SlcanParser parser;
    // N is the LAWICEL serial-number query, which this adapter does not carry: refusing
    // says so, where a version string would be a wrong answer shaped like a right one.
    for (const char* line : {"Z\r", "CC\r", "OO\r", "LL\r", "FF\r", "VV\r", "N\r"}) {
        TEST_ASSERT_EQUAL(static_cast<int>(SlcanCommand::Invalid),
                          static_cast<int>(feed_line(parser, line).command));
    }
    // A bare terminator is answered rather than ignored: python-can emits one on every
    // set_bitrate, and silence there is the ambiguity this profile exists to remove.
    TEST_ASSERT_EQUAL(static_cast<int>(SlcanCommand::Empty),
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

    SlcanFrame remote_standard;
    remote_standard.id = 0x123;
    remote_standard.rtr = true;
    remote_standard.dlc = 4;
    TEST_ASSERT_EQUAL_STRING("r1234\r", encode(remote_standard).c_str());
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

class FakeChannel final : public SlcanChannelPort {
public:
    bool open_ok = true, close_ok = true;
    uint8_t flags = 0;
    int opens = 0, closes = 0;
    Bitrate bitrate = Bitrate::K125;
    bool listen = false;

    bool open(Bitrate requested, bool listen_only) override {
        ++opens; bitrate = requested; listen = listen_only; return open_ok;
    }
    bool close() override { ++closes; return close_ok; }
    uint8_t status() override { return flags; }
};

std::string drive(SlcanSession& session, const std::string& line) {
    std::string answered;
    for (char byte : line) {
        const SlcanReply reply = session.feed(byte);
        answered.append(reply.text, reply.length);
    }
    return answered;
}

void session_applies_the_bitrate_only_on_a_closed_channel() {
    FakeChannel channel;
    SlcanSession session(channel);

    TEST_ASSERT_EQUAL_STRING("\r", drive(session, "S5\r").c_str());
    TEST_ASSERT_EQUAL_STRING("\r", drive(session, "L\r").c_str());
    TEST_ASSERT_EQUAL(1, channel.opens);
    TEST_ASSERT_EQUAL_UINT32(250000, static_cast<uint32_t>(channel.bitrate));
    TEST_ASSERT_TRUE(channel.listen);
    TEST_ASSERT_TRUE(session.is_open());

    // Retiming a running channel is impossible, so accepting it would report a rate that
    // was never applied. Refused, and the stored rate is untouched.
    TEST_ASSERT_EQUAL_STRING("\a", drive(session, "S4\r").c_str());
    TEST_ASSERT_EQUAL_UINT32(250000, static_cast<uint32_t>(session.bitrate()));
    // Opening an open channel is equally refused rather than silently re-applied.
    TEST_ASSERT_EQUAL_STRING("\a", drive(session, "O\r").c_str());
    TEST_ASSERT_EQUAL(1, channel.opens);

    TEST_ASSERT_EQUAL_STRING("\r", drive(session, "C\r").c_str());
    TEST_ASSERT_FALSE(session.is_open());
    TEST_ASSERT_EQUAL_STRING("\r", drive(session, "S4\r").c_str());
    TEST_ASSERT_EQUAL_STRING("\r", drive(session, "O\r").c_str());
    TEST_ASSERT_FALSE(channel.listen);
    TEST_ASSERT_EQUAL_UINT32(125000, static_cast<uint32_t>(channel.bitrate));
}

void session_reports_every_refusal_instead_of_staying_silent() {
    FakeChannel channel;
    SlcanSession session(channel);

    // A transmit request is refused in every state, open or closed.
    TEST_ASSERT_EQUAL_STRING("\a", drive(session, "T001ABCDE80102030405060708\r").c_str());
    // A refused transmit must not reach the channel at all, not even to open it.
    TEST_ASSERT_EQUAL(0, channel.opens);
    TEST_ASSERT_EQUAL(0, channel.closes);
    drive(session, "L\r");
    TEST_ASSERT_EQUAL_STRING("\a", drive(session, "t1230\r").c_str());
    TEST_ASSERT_EQUAL_STRING("\a", drive(session, "S9\r").c_str());
    TEST_ASSERT_EQUAL_STRING("\a", drive(session, "Z\r").c_str());
    // A bare terminator is answered too. python-can writes one on every set_bitrate, and
    // leaving it unanswered would reintroduce exactly the silence this profile removes.
    TEST_ASSERT_EQUAL_STRING("\r", drive(session, "\r").c_str());

    // A failing driver is reported, and the channel stays closed rather than half-open.
    FakeChannel refusing;
    refusing.open_ok = false;
    SlcanSession other(refusing);
    TEST_ASSERT_EQUAL_STRING("\a", drive(other, "L\r").c_str());
    TEST_ASSERT_FALSE(other.is_open());
    // Closing an already closed channel still reaches the port: an earlier cleanup may have
    // failed with the driver installed, and C is the only command that can retry it.
    TEST_ASSERT_EQUAL_STRING("\r", drive(other, "C\r").c_str());
    TEST_ASSERT_EQUAL(1, refusing.closes);

    // A close the driver refused is reported, and the channel stays open rather than
    // pretending the cleanup happened.
    FakeChannel stuck;
    stuck.close_ok = false;
    SlcanSession held(stuck);
    drive(held, "L\r");
    TEST_ASSERT_EQUAL_STRING("\a", drive(held, "C\r").c_str());
    TEST_ASSERT_TRUE(held.is_open());
    TEST_ASSERT_EQUAL(1, stuck.closes);
}

void session_answers_status_and_version() {
    FakeChannel channel;
    channel.flags = 0xE8;
    SlcanSession session(channel);
    TEST_ASSERT_EQUAL_STRING("FE8\r", drive(session, "F\r").c_str());
    TEST_ASSERT_EQUAL_STRING("V0100\r", drive(session, "V\r").c_str());
    channel.flags = 0x08;
    TEST_ASSERT_EQUAL_STRING("F08\r", drive(session, "F\r").c_str());
}

void session_renders_frames_and_drops_unrepresentable_ones() {
    FakeChannel channel;
    SlcanSession session(channel);
    const SlcanReply line = session.frame(extended_frame());
    TEST_ASSERT_EQUAL_STRING("T001ABCDE80102030405060708\r",
                             std::string(line.text, line.length).c_str());

    SlcanFrame broken = extended_frame();
    broken.dlc = 15;
    TEST_ASSERT_EQUAL(0, session.frame(broken).length);
}

void session_drops_a_partial_line_on_reset() {
    FakeChannel channel;
    SlcanSession session(channel);
    session.feed('S');
    session.reset();
    TEST_ASSERT_EQUAL_STRING("\r", drive(session, "L\r").c_str());
    TEST_ASSERT_EQUAL(1, channel.opens);
}

}  // namespace

void run_slcan_tests() {
    RUN_TEST(parser_accepts_the_commands_the_host_sends);
    RUN_TEST(parser_maps_only_the_bitrates_the_controller_can_produce);
    RUN_TEST(parser_classifies_transmit_lines_distinctly_from_invalid_ones);
    RUN_TEST(parser_rejects_malformed_and_overlong_lines);
    RUN_TEST(encoder_renders_frames_the_host_can_parse);
    RUN_TEST(encoder_refuses_what_it_cannot_represent);
    RUN_TEST(status_byte_follows_the_lawicel_layout);
    RUN_TEST(session_applies_the_bitrate_only_on_a_closed_channel);
    RUN_TEST(session_reports_every_refusal_instead_of_staying_silent);
    RUN_TEST(session_answers_status_and_version);
    RUN_TEST(session_renders_frames_and_drops_unrepresentable_ones);
    RUN_TEST(session_drops_a_partial_line_on_reset);
}
