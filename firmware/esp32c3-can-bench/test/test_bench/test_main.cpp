#include <unity.h>
#include <activity.h>
#include <bench.h>
#include <fake_sdk.h>
#include "twai_port.h"

#include <cstring>
#include <string>

using namespace bench;

namespace {
class FakeCan final : public CanPort {
public:
    bool start_ok = true, submit_ok = true, stop_ok = true, bus_off = false;
    int starts = 0, submissions = 0, stops = 0;
    Result next;
    Frame sent;
    Bitrate bitrate = Bitrate::K125;
    bool start(Bitrate selected = Bitrate::K125) override {
        ++starts; bitrate = selected; return start_ok;
    }
    bool submit(const Frame& f) override { ++submissions; sent = f; return submit_ok; }
    Result poll() override {
        Result result = next;
        next = Result{};
        bus_off = bus_off || result.bus_off;
        result.bus_off = bus_off;
        return result;
    }
    bool stop() override { ++stops; next = Result{}; return stop_ok; }
};

void expect_state(const BenchController& c, State state) {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(state), static_cast<int>(c.status().state));
}
void expect_fault(const BenchController& c, Fault fault) {
    expect_state(c, State::Fault);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(fault), static_cast<int>(c.status().last_fault));
}
Command feed(CommandParser& p, const std::string& line) {
    Command result = Command::None;
    for (char c : line) { result = p.feed(c); }
    return result;
}
void expect_command(Command expected, Command actual) {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(expected), static_cast<int>(actual));
}
void pending(BenchController& c) { c.command(Command::Start, 0); c.tick(1000); }

void parser_accepts_commands() {
    CommandParser parser;
    expect_command(Command::Start, feed(parser, "start\n"));
    expect_command(Command::Stop, feed(parser, " \tstop \t\r\n"));
    expect_command(Command::Status, feed(parser, "status\n"));
    expect_command(Command::Help, feed(parser, "help\r\n"));
    expect_command(Command::None, feed(parser, "\n"));
    expect_command(Command::None, feed(parser, " \t\r\n"));
}

void parser_rejects_invalid_and_binary() {
    for (const auto& s : {std::string("START\n"), std::string("start stop\n"),
                         std::string("sta\rrt\n"), std::string("start\r\r\n"),
                         std::string("start\0stop\n", 11), std::string("\xFF\n", 2)}) {
        CommandParser parser;
        expect_command(Command::Invalid, feed(parser, s));
        expect_command(Command::Start, feed(parser, "start\n"));
    }
}

void parser_bounds_and_fragmentation() {
    CommandParser parser;
    expect_command(Command::Start, feed(parser, std::string(27, ' ') + "start\r\n"));
    expect_command(Command::Invalid, feed(parser, std::string(28, ' ') + "start\n"));
    expect_command(Command::Invalid, feed(parser, std::string(10000, 'x') + "start\n"));
    expect_command(Command::None, feed(parser, "sta"));
    expect_command(Command::Start, feed(parser, "rt\n"));
    feed(parser, "sta"); parser.reset();
    expect_command(Command::Invalid, feed(parser, "rt\n"));
    feed(parser, std::string(40, 'x')); parser.reset();
    expect_command(Command::Stop, feed(parser, "stop\n"));
}

void stopped_is_silent() {
    FakeCan port; BenchController c(port);
    for (auto cmd : {Command::None, Command::Stop, Command::Status, Command::Help, Command::Invalid}) {
        c.command(cmd, 1000); c.tick(2000); c.service(3000); c.disconnect();
    }
    expect_state(c, State::Stopped);
    TEST_ASSERT_EQUAL(0, port.starts);
    TEST_ASSERT_EQUAL(0, port.submissions);
    TEST_ASSERT_EQUAL(0, port.stops);
}

void cadence_and_fixed_frame() {
    FakeCan port; BenchController c(port);
    c.command(Command::Start, 50); c.tick(1049);
    TEST_ASSERT_EQUAL(0, port.submissions);
    c.command(Command::Start, 800); c.tick(1050);
    TEST_ASSERT_EQUAL(1, port.starts);
    TEST_ASSERT_EQUAL(1, port.submissions);
    TEST_ASSERT_EQUAL_HEX32(0x001ABCDE, port.sent.id);
    const uint8_t bytes[] = {1,2,3,4,5,6,7,8};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(bytes, port.sent.data, 8);
    TEST_ASSERT_EQUAL(1, c.status().queued);
    TEST_ASSERT_EQUAL(0, c.status().succeeded);
    TEST_ASSERT_TRUE(c.status().pending);
    c.tick(1100); TEST_ASSERT_EQUAL(1, port.submissions);
    port.next.success = true; c.tick(1101);
    TEST_ASSERT_EQUAL(1, c.status().succeeded);
    c.tick(2049); TEST_ASSERT_EQUAL(1, port.submissions);
    c.tick(2050); TEST_ASSERT_EQUAL(2, port.submissions);
}

void late_ticks_do_not_burst() {
    FakeCan port; BenchController c(port);
    c.command(Command::Start, 0); c.tick(9000);
    port.next.success = true; c.tick(9001);
    c.tick(9001); c.tick(9999);
    TEST_ASSERT_EQUAL(1, port.submissions);
    c.tick(10000); TEST_ASSERT_EQUAL(2, port.submissions);
}

void clock_rollover_is_safe() {
    FakeCan port; BenchController c(port);
    c.command(Command::Start, UINT32_MAX - 499);
    c.tick(499); TEST_ASSERT_EQUAL(0, port.submissions);
    c.tick(500); TEST_ASSERT_EQUAL(1, port.submissions);
    port.next.success = true; c.tick(501);
    TEST_ASSERT_EQUAL(1, c.status().succeeded);
}

void stop_aborts_and_restart_waits() {
    FakeCan port; BenchController c(port); pending(c);
    c.command(Command::Stop, 1000); c.command(Command::Stop, 1000);
    expect_state(c, State::Stopped);
    TEST_ASSERT_EQUAL(1, c.status().aborted);
    TEST_ASSERT_EQUAL(0, c.status().failed);
    TEST_ASSERT_EQUAL(1, port.stops);
    port.next.success = true; c.tick(2000);
    TEST_ASSERT_EQUAL(0, c.status().succeeded);
    port.next = Result{};
    c.command(Command::Start, 2000); c.tick(2999);
    TEST_ASSERT_EQUAL(1, port.submissions);
    c.tick(3000); TEST_ASSERT_EQUAL(2, port.submissions);
}

void disconnect_stops_and_never_restarts() {
    FakeCan port; BenchController c(port); pending(c);
    c.disconnect(); c.disconnect(); c.tick(99999);
    expect_state(c, State::Stopped);
    TEST_ASSERT_EQUAL(1, c.status().aborted);
    TEST_ASSERT_EQUAL(1, port.submissions);
}

void initialization_failure_requires_cleanup() {
    FakeCan port; port.start_ok = false; BenchController c(port);
    c.command(Command::Start, 0); expect_fault(c, Fault::Initialization);
    TEST_ASSERT_EQUAL(1, port.stops);
    TEST_ASSERT_EQUAL(0, c.status().failed);
    c.command(Command::Start, 0); TEST_ASSERT_EQUAL(1, port.starts);
    c.command(Command::Stop, 0); expect_state(c, State::Stopped);
    port.start_ok = true; c.command(Command::Start, 0); expect_state(c, State::Running);
}

void submission_failure_is_not_queued() {
    FakeCan port; port.submit_ok = false; BenchController c(port); pending(c);
    expect_fault(c, Fault::Submission);
    TEST_ASSERT_EQUAL(0, c.status().queued);
    TEST_ASSERT_EQUAL(1, c.status().failed);
    c.tick(10000); TEST_ASSERT_EQUAL(1, port.submissions);
}

void completion_timeout_and_late_success() {
    FakeCan port; BenchController c(port); pending(c);
    c.tick(1249); expect_state(c, State::Running);
    port.next.success = true; c.tick(1250);
    expect_fault(c, Fault::Timeout);
    TEST_ASSERT_EQUAL(1, c.status().failed);
    port.next.success = true; c.service(1251);
    TEST_ASSERT_EQUAL(1, c.status().failed);
    TEST_ASSERT_EQUAL(0, c.status().succeeded);
}

void fault_wins_over_success() {
    for (int kind = 0; kind < 3; ++kind) {
        FakeCan port; BenchController c(port); pending(c);
        port.next.success = true;
        if (kind == 0) { port.next.failure = true; }
        if (kind == 1) { port.next.driver_error = true; }
        if (kind == 2) { port.next.bus_off = true; }
        c.tick(1001);
        expect_state(c, State::Fault);
        TEST_ASSERT_EQUAL(0, c.status().succeeded);
        TEST_ASSERT_EQUAL(1, c.status().failed);
        TEST_ASSERT_FALSE(c.status().pending);
    }
}

void bus_off_latches_until_reset() {
    FakeCan port; BenchController c(port); pending(c);
    port.next.bus_off = true; c.tick(1001);
    expect_fault(c, Fault::BusOff);
    c.command(Command::Stop, 1002); c.command(Command::Start, 2000);
    expect_state(c, State::Fault);
    TEST_ASSERT_TRUE(c.status().bus_off_latched);
    TEST_ASSERT_EQUAL(1, port.starts);
}

void cleanup_failure_stays_fault() {
    FakeCan port; BenchController c(port); pending(c);
    port.stop_ok = false; c.command(Command::Stop, 1001);
    expect_fault(c, Fault::Cleanup);
    TEST_ASSERT_EQUAL(1, c.status().aborted);
    c.command(Command::Stop, 1002); expect_fault(c, Fault::Cleanup);
    port.stop_ok = true; c.command(Command::Stop, 1003); expect_state(c, State::Stopped);
    c.command(Command::Start, 2000); port.stop_ok = false;
    port.next.driver_error = true; c.tick(2001); expect_fault(c, Fault::Cleanup);
}

void asynchronous_fault_prevents_due_submission() {
    FakeCan port; BenchController c(port); c.command(Command::Start, 0);
    port.next.failure = true; c.tick(1000);
    expect_fault(c, Fault::Transmission);
    TEST_ASSERT_EQUAL(0, port.submissions);
    TEST_ASSERT_EQUAL(0, c.status().failed);
}

void activity_pulse_expires_rearms_and_survives_rollover() {
    ActivityPulse pulse;
    TEST_ASSERT_FALSE(pulse.active(0));           // Idle until armed.
    pulse.pulse(1000);
    TEST_ASSERT_TRUE(pulse.active(1000));
    TEST_ASSERT_TRUE(pulse.active(1000 + ActivityPulse::kPulseMs - 1));
    TEST_ASSERT_FALSE(pulse.active(1000 + ActivityPulse::kPulseMs));
    TEST_ASSERT_FALSE(pulse.active(500000));      // Expired, never latched on.
    pulse.pulse(2000);                            // Re-arming during a pulse extends it.
    pulse.pulse(2050);
    TEST_ASSERT_TRUE(pulse.active(2050 + ActivityPulse::kPulseMs - 1));
    TEST_ASSERT_FALSE(pulse.active(2050 + ActivityPulse::kPulseMs));
    const uint32_t late = UINT32_MAX - 10;        // Wrap-safe across the millis() rollover.
    pulse.pulse(late);
    TEST_ASSERT_TRUE(pulse.active(late));
    TEST_ASSERT_TRUE(pulse.active(static_cast<uint32_t>(late + ActivityPulse::kPulseMs - 1)));
    TEST_ASSERT_FALSE(pulse.active(static_cast<uint32_t>(late + ActivityPulse::kPulseMs)));
    // Expiry is latched: coming back around to the original window must stay dark, or the
    // board would flash once every 49.7 days with nothing transmitted.
    TEST_ASSERT_FALSE(pulse.active(late));
    TEST_ASSERT_FALSE(pulse.active(static_cast<uint32_t>(late + 1)));
}

void names_cover_status_values() {
    for (auto state : {State::Stopped, State::Running, State::Fault}) {
        TEST_ASSERT_NOT_EQUAL(0, std::strlen(state_name(state)));
    }
    for (auto fault : {Fault::None, Fault::Initialization, Fault::Submission, Fault::Transmission,
                       Fault::Timeout, Fault::BusOff, Fault::Driver, Fault::Cleanup}) {
        TEST_ASSERT_NOT_EQUAL(0, std::strlen(fault_name(fault)));
    }
    TEST_ASSERT_EQUAL_STRING("unknown", state_name(static_cast<State>(100)));
    TEST_ASSERT_EQUAL_STRING("unknown", fault_name(static_cast<Fault>(100)));
}

void autonomous_countdown_and_exact_limit() {
    FakeCan port; BenchController c(port); BurstRunner burst(c);
    burst.tick(0); burst.tick(9999);
    TEST_ASSERT_EQUAL(0, port.starts);
    for (uint32_t n = 0; n < 10; ++n) {
        const uint32_t now = 10000 + n * 1000;
        burst.tick(now);
        TEST_ASSERT_EQUAL(n + 1, port.submissions);
        TEST_ASSERT_TRUE(c.status().pending);
        TEST_ASSERT_FALSE(burst.finished());
        TEST_ASSERT_EQUAL(0, port.stops);
        port.next.success = true; burst.service(now + 1);
        TEST_ASSERT_EQUAL(n + 1, c.status().succeeded);
        burst.tick(now + 999);
        TEST_ASSERT_EQUAL(n + 1, port.submissions);
    }
    TEST_ASSERT_TRUE(burst.finished()); expect_state(c, State::Stopped);
    TEST_ASSERT_EQUAL(1, port.starts); TEST_ASSERT_EQUAL(1, port.stops);
    TEST_ASSERT_EQUAL(0, c.status().aborted);
    burst.tick(1000000); TEST_ASSERT_EQUAL(10, port.submissions);
}

void autonomous_cancel_and_immediate_start_guards() {
    FakeCan port; BenchController c(port); BurstRunner burst(c);
    burst.stop(9999); burst.stop(10000); burst.tick(20000);
    TEST_ASSERT_TRUE(burst.finished()); TEST_ASSERT_EQUAL(0, port.starts);
    FakeCan second; BenchController other(second); BurstRunner running(other);
    running.tick(10000); running.stop(10001); running.tick(20000);
    TEST_ASSERT_EQUAL(1, other.status().aborted);
    TEST_ASSERT_EQUAL(1, second.submissions);
    other.start_immediately(21000); other.start_immediately(21999);
    other.tick(21000); TEST_ASSERT_EQUAL(2, second.submissions);
    second.next.failure = true; other.service(21001);
    other.start_immediately(22000); expect_state(other, State::Fault);
}

void autonomous_rollover_and_late_ticks() {
    FakeCan port; BenchController c(port); BurstRunner burst(c, UINT32_MAX - 4999);
    burst.tick(4999); TEST_ASSERT_EQUAL(0, port.starts);
    burst.tick(5000); TEST_ASSERT_EQUAL(1, port.submissions);
    port.next.success = true; burst.tick(5001);
    burst.tick(90000); TEST_ASSERT_EQUAL(2, port.submissions);
    port.next.success = true; burst.tick(90001);
    burst.tick(90999); TEST_ASSERT_EQUAL(2, port.submissions);
    burst.tick(91000); TEST_ASSERT_EQUAL(3, port.submissions);
}

void autonomous_errors_never_rearm() {
    for (int kind = 0; kind < 6; ++kind) {
        FakeCan port; BenchController c(port); BurstRunner burst(c);
        if (kind == 0) { port.start_ok = false; }
        if (kind == 1) { port.submit_ok = false; }
        burst.tick(10000);
        if (kind == 2) { port.next.failure = true; }
        if (kind == 3) { port.next.bus_off = true; }
        if (kind == 4) { port.next.driver_error = true; }
        burst.tick(kind == 5 ? 10250 : 10001);
        expect_state(c, State::Fault); TEST_ASSERT_TRUE(burst.finished());
        const int attempts = port.submissions;
        burst.stop(20000); burst.tick(30000);
        TEST_ASSERT_EQUAL(1, port.starts); TEST_ASSERT_EQUAL(attempts, port.submissions);
    }
}

void autonomous_final_completion_and_cleanup_failures() {
    for (bool timeout : {false, true}) {
        FakeCan port; BenchController c(port); BurstRunner burst(c);
        for (uint32_t n = 0; n < 9; ++n) {
            burst.tick(10000 + n * 1000);
            port.next.success = true; burst.service(10001 + n * 1000);
        }
        burst.tick(19000); burst.tick(19249);
        TEST_ASSERT_FALSE(burst.finished()); TEST_ASSERT_EQUAL(0, port.stops);
        port.next.success = true;
        if (!timeout) { port.stop_ok = false; }
        burst.tick(timeout ? 19250 : 19249);
        expect_fault(c, timeout ? Fault::Timeout : Fault::Cleanup);
        TEST_ASSERT_TRUE(burst.finished()); TEST_ASSERT_EQUAL(10, port.submissions);
        TEST_ASSERT_EQUAL(timeout ? 9 : 10, c.status().succeeded);
        port.stop_ok = true; burst.stop(20000); burst.tick(30000);
        TEST_ASSERT_EQUAL(1, port.starts); TEST_ASSERT_EQUAL(10, port.submissions);
    }
}

void adapter_uses_real_contract_fields() {
    TwaiPort port;
    TEST_ASSERT_FALSE(port.submit(Frame{}));
    TEST_ASSERT_TRUE(port.stop());
    TEST_ASSERT_TRUE(port.start());
    TEST_ASSERT_FALSE(port.start());
    TEST_ASSERT_EQUAL(4, sdk.general.tx_io); TEST_ASSERT_EQUAL(3, sdk.general.rx_io);
    // The diagnostic image must really ask the SDK to skip acknowledgement checking.
    TEST_ASSERT_EQUAL(kExpectedTwaiMode, sdk.general.mode);
    TEST_ASSERT_EQUAL(0, sdk.general.tx_queue_len);
    TEST_ASSERT_EQUAL(125000, 80000000 / (sdk.timing.brp * (1 + sdk.timing.tseg_1 + sdk.timing.tseg_2)));
    TEST_ASSERT_EQUAL(TWAI_ALERT_TX_SUCCESS | TWAI_ALERT_TX_FAILED | TWAI_ALERT_BUS_OFF,
                      sdk.general.alerts_enabled);
    TEST_ASSERT_TRUE(port.submit(Frame{}));
    TEST_ASSERT_EQUAL(0, sdk.transmit_wait);
    TEST_ASSERT_EQUAL(1, sdk.message.extd); TEST_ASSERT_EQUAL(1, sdk.message.ss);
    TEST_ASSERT_EQUAL(0, sdk.message.rtr); TEST_ASSERT_EQUAL(0, sdk.message.self);
    TEST_ASSERT_EQUAL(8, sdk.message.data_length_code);
    TEST_ASSERT_EQUAL_HEX32(0x001ABCDE, sdk.message.identifier);
    const Frame frame;
    TEST_ASSERT_EQUAL_UINT8_ARRAY(frame.data, sdk.message.data, 8);
    sdk.transmit_result = ESP_FAIL; TEST_ASSERT_FALSE(port.submit(frame));
    TEST_ASSERT_TRUE(port.stop()); TEST_ASSERT_FALSE(sdk.installed);
}

void adapter_install_and_start_errors() {
    TwaiPort port;
    sdk.install_result = ESP_FAIL; TEST_ASSERT_FALSE(port.start());
    TEST_ASSERT_EQUAL(0, sdk.starts); TEST_ASSERT_TRUE(port.stop());
    sdk.install_result = ESP_OK; sdk.start_result = ESP_FAIL;
    TEST_ASSERT_FALSE(port.start()); TEST_ASSERT_TRUE(port.stop());
    TEST_ASSERT_EQUAL(1, sdk.uninstalls);
}

void adapter_polls_alerts_and_status() {
    TwaiPort port; TEST_ASSERT_TRUE(port.start());
    sdk.alert_result = ESP_ERR_TIMEOUT;
    TEST_ASSERT_FALSE(port.poll().driver_error);
    TEST_ASSERT_EQUAL(0, sdk.alert_wait);
    sdk.alert_result = ESP_FAIL; TEST_ASSERT_TRUE(port.poll().driver_error);
    sdk.alert_result = ESP_OK; sdk.alerts = TWAI_ALERT_TX_SUCCESS | TWAI_ALERT_TX_FAILED;
    const auto result = port.poll(); TEST_ASSERT_TRUE(result.success); TEST_ASSERT_TRUE(result.failure);
    sdk.status_result = ESP_FAIL; TEST_ASSERT_TRUE(port.poll().driver_error);
    sdk.status_result = ESP_OK; sdk.state = TWAI_STATE_STOPPED;
    TEST_ASSERT_TRUE(port.poll().driver_error);
    sdk.state = TWAI_STATE_RUNNING; sdk.alerts = TWAI_ALERT_BUS_OFF;
    TEST_ASSERT_TRUE(port.poll().bus_off);
    TEST_ASSERT_TRUE(port.stop()); TEST_ASSERT_TRUE(port.poll().bus_off);
    TEST_ASSERT_FALSE(port.start()); TEST_ASSERT_FALSE(port.submit(Frame{}));
}

void adapter_bus_off_between_poll_and_stop() {
    TwaiPort port; BenchController c(port); c.command(Command::Start, 0);
    sdk.state = TWAI_STATE_BUS_OFF;
    c.command(Command::Stop, 1);
    TEST_ASSERT_EQUAL(0, sdk.stops);
    TEST_ASSERT_EQUAL(1, sdk.uninstalls);
    TEST_ASSERT_TRUE(c.status().bus_off_latched);
    expect_fault(c, Fault::BusOff);
}

void adapter_status_detects_bus_off() {
    TwaiPort port; TEST_ASSERT_TRUE(port.start()); sdk.state = TWAI_STATE_BUS_OFF;
    TEST_ASSERT_TRUE(port.poll().bus_off); TEST_ASSERT_TRUE(port.stop());
}

void adapter_cleanup_errors_and_retry() {
    TwaiPort port; TEST_ASSERT_TRUE(port.start());
    sdk.stop_result = ESP_FAIL; TEST_ASSERT_FALSE(port.stop());
    TEST_ASSERT_TRUE(sdk.installed);
    sdk.stop_result = ESP_OK; sdk.uninstall_result = ESP_FAIL;
    TEST_ASSERT_FALSE(port.stop()); TEST_ASSERT_FALSE(port.start());
    sdk.uninstall_result = ESP_OK; TEST_ASSERT_TRUE(port.stop());
    TEST_ASSERT_FALSE(sdk.installed);
    TEST_ASSERT_TRUE(port.start()); sdk.state = TWAI_STATE_RECOVERING;
    TEST_ASSERT_FALSE(port.stop()); sdk.state = TWAI_STATE_STOPPED;
    TEST_ASSERT_TRUE(port.stop());
}

void adapter_cleanup_attempts_stop_without_status() {
    TwaiPort port; TEST_ASSERT_TRUE(port.start()); sdk.status_result = ESP_FAIL;
    sdk.stop_result = ESP_FAIL; TEST_ASSERT_FALSE(port.stop());
    sdk.stop_result = ESP_OK; TEST_ASSERT_TRUE(port.stop());
    TEST_ASSERT_EQUAL(2, sdk.stops);
}

void diagnostics_survive_fault_cleanup_and_reset_on_new_start() {
    TwaiPort port; BenchController c(port);
    TEST_ASSERT_FALSE(port.diagnostics().sampled);
    pending(c);
    sdk.counters = {TWAI_STATE_RUNNING, 8, 2, 1, 3, 4, 5, 6};
    sdk.alerts = TWAI_ALERT_TX_FAILED;
    c.service(1001);
    expect_fault(c, Fault::Transmission);
    TEST_ASSERT_FALSE(sdk.installed);
    const auto snapshot = port.diagnostics();
    TEST_ASSERT_TRUE(snapshot.sampled);
    TEST_ASSERT_TRUE(snapshot.alerts_available);
    TEST_ASSERT_TRUE(snapshot.status_available);
    TEST_ASSERT_EQUAL_HEX32(TWAI_ALERT_TX_FAILED, snapshot.alerts);
    TEST_ASSERT_EQUAL(TWAI_STATE_RUNNING, snapshot.status.state);
    TEST_ASSERT_EQUAL(8, snapshot.status.tx_error_counter);
    TEST_ASSERT_EQUAL(2, snapshot.status.rx_error_counter);
    TEST_ASSERT_EQUAL(1, snapshot.status.tx_failed_count);
    TEST_ASSERT_EQUAL(3, snapshot.status.rx_missed_count);
    TEST_ASSERT_EQUAL(4, snapshot.status.rx_overrun_count);
    TEST_ASSERT_EQUAL(5, snapshot.status.arb_lost_count);
    TEST_ASSERT_EQUAL(6, snapshot.status.bus_error_count);
    sdk.counters = {}; port.poll(); port.stop();
    TEST_ASSERT_EQUAL(8, port.diagnostics().status.tx_error_counter);
    TEST_ASSERT_FALSE(port.start(static_cast<Bitrate>(42)));
    TEST_ASSERT_TRUE(port.diagnostics().sampled);
    sdk.install_result = ESP_FAIL;
    TEST_ASSERT_FALSE(port.start());
    TEST_ASSERT_FALSE(port.diagnostics().sampled);
}

void diagnostics_mark_failed_reads_unavailable() {
    TwaiPort port; TEST_ASSERT_TRUE(port.start());
    sdk.counters.tx_error_counter = 123;
    sdk.alerts = TWAI_ALERT_TX_SUCCESS;
    port.poll();
    TEST_ASSERT_FALSE(port.start());
    TEST_ASSERT_EQUAL(123, port.diagnostics().status.tx_error_counter);
    sdk.status_result = ESP_FAIL; sdk.alert_result = ESP_FAIL;
    TEST_ASSERT_TRUE(port.poll().driver_error);
    TEST_ASSERT_FALSE(port.diagnostics().status_available);
    TEST_ASSERT_FALSE(port.diagnostics().alerts_available);
    TEST_ASSERT_EQUAL(0, port.diagnostics().status.tx_error_counter);
    port.stop();
    TEST_ASSERT_FALSE(port.diagnostics().status_available);
    sdk.status_result = ESP_OK; sdk.alert_result = ESP_ERR_TIMEOUT;
    TEST_ASSERT_TRUE(port.start()); port.poll();
    TEST_ASSERT_TRUE(port.diagnostics().status_available);
    TEST_ASSERT_TRUE(port.diagnostics().alerts_available);
    TEST_ASSERT_EQUAL(0, port.diagnostics().alerts);
    TEST_ASSERT_TRUE(port.stop());
}

void usb_command(const std::string& command) { Serial.send(command); loop(); }
void output_contains(const char* needle) {
    TEST_ASSERT_NOT_EQUAL(std::string::npos, Serial.output.find(needle));
}

#if BENCH_RECEIVER
void application_receiver_never_transmits() {
    // Arrange/act: run the actual setup/loop, including USB reconnect and bounded input.
    setup(); loop(); sdk.now = 100000; loop();
    TEST_ASSERT_EQUAL(0, sdk.starts);
    Serial.connected = true; Serial.send("start\n"); loop();
    TEST_ASSERT_EQUAL(0, sdk.starts); output_contains("RECEIVER image");
    usb_command("help\nstatus\nBAD\n\n"); output_contains("invalid command");
    usb_command("start\n"); output_contains("receiver=ready");
    twai_message_t frame = {}; frame.identifier = 0x001ABCDE; frame.extd = 1;
    frame.data_length_code = 8;
    for (unsigned i = 0; i < 8; ++i) { frame.data[i] = i + 1; }
    sdk.incoming.push_back(frame); loop();
    output_contains("rx_id=0x001abcde extended=1 rtr=0 dlc=8 data=[01 02 03 04 05 06 07 08 ]");
    Serial.connected = false; sdk.incoming.push_back(frame); loop();
    TEST_ASSERT_TRUE(sdk.installed);
    Serial.output.clear(); Serial.connected = true; loop(); usb_command("status\n");
    output_contains("received=2"); output_contains("rx_id=0x001abcde");
    frame.rtr = 1; sdk.incoming.push_back(frame); loop(); output_contains("rtr=1 dlc=8 data=[]");
    frame.rtr = 0; frame.extd = 0; frame.identifier = 0; frame.data_length_code = 0;
    sdk.incoming.push_back(frame); loop(); output_contains("extended=0 rtr=0 dlc=0 data=[]");
    Serial.writable = 0; usb_command("status\n"); Serial.writable = 1024;
    Serial.partial_write = true; usb_command("help\n"); Serial.partial_write = false;
    Serial.negative_read = true; usb_command("help\n"); Serial.negative_read = false; loop();
    Serial.lose_on_read = true; usb_command("help\n"); Serial.lose_on_read = false; loop();
    Serial.connected = true; loop();
    usb_command(std::string(64, '\n') + "stop\n"); loop();
    TEST_ASSERT_FALSE(sdk.installed);
    Serial.output.clear(); usb_command("status\n"); output_contains("received=4");
    usb_command("start\n"); sdk.status_result = ESP_FAIL; sdk.alert_result = ESP_FAIL; loop();
    output_contains("receiver=fault"); output_contains("twai_status=unavailable");
    output_contains("alerts=unavailable");
    usb_command("start\nstop\n");
    // Assert: no command, elapsed time, malformed input or USB state sent a data frame.
    TEST_ASSERT_EQUAL(0, sdk.transmits);
}
#elif BENCH_BUTTONS
void application_buttons_without_usb() {
    sdk.counters = {TWAI_STATE_RUNNING, UINT32_MAX, UINT32_MAX, UINT32_MAX,
                    UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX};
    setup(); loop(); TEST_ASSERT_EQUAL(0, sdk.installs);
    TEST_ASSERT_EQUAL(INPUT_PULLUP, sdk.modes[0]);
    TEST_ASSERT_EQUAL(INPUT_PULLUP, sdk.modes[1]);
    TEST_ASSERT_EQUAL(INPUT_PULLUP, sdk.modes[5]);
    sdk.now = 30000; loop(); TEST_ASSERT_EQUAL(0, sdk.transmits);
    sdk.levels[0] = LOW; sdk.now = 30001; loop();
    sdk.now = 30031; loop(); TEST_ASSERT_EQUAL(1, sdk.starts);
    sdk.now = 31030; loop(); TEST_ASSERT_EQUAL(0, sdk.transmits);
    sdk.now = 31031; loop(); TEST_ASSERT_EQUAL(1, sdk.transmits);
    TEST_ASSERT_EQUAL(OUTPUT, sdk.modes[kLedPin]);
    TEST_ASSERT_EQUAL(HIGH, sdk.outputs[kLedPin]);  // Active low: dark while nothing is sent.
    sdk.alerts = TWAI_ALERT_TX_SUCCESS; sdk.now++; loop();
    TEST_ASSERT_EQUAL(LOW, sdk.outputs[kLedPin]);   // Flash armed by the acknowledged frame.
    // Sample twice inside one pulse, a stutter half-period apart. The self-test image must
    // break the flash up; every other image must hold it steady. Without this, the NO_ACK
    // build would blink exactly like a genuine acknowledged run on a disconnected bus.
    const int first_sample = sdk.outputs[kLedPin];
    sdk.now += 30; loop();
#if BENCH_NO_ACK
    TEST_ASSERT_NOT_EQUAL(first_sample, sdk.outputs[kLedPin]);
#else
    TEST_ASSERT_EQUAL(first_sample, sdk.outputs[kLedPin]);
#endif
    Serial.connected = true; loop(); output_contains("BUTTONS image");
    usb_command("start\nhelp\nstatus\nBAD\n\n"); output_contains("use a physical button");
    output_contains("invalid command"); output_contains("scenario=reference125");
#if BENCH_NO_ACK
    output_contains("SELF-TEST IMAGE");                  // Banner, once per connection.
    output_contains("selftest=no_ack ack_required=0\n");  // Marker, on every status sample.
#endif
    sdk.now = 32031; usb_command("stop\n"); TEST_ASSERT_EQUAL(1, sdk.transmits);
    TEST_ASSERT_EQUAL(HIGH, sdk.outputs[kLedPin]);  // Pulse expired; it never latches on.
    TEST_ASSERT_FALSE(sdk.installed);
    sdk.levels[0] = HIGH; sdk.now++; loop(); sdk.now += 30; loop();
    sdk.levels[1] = LOW; sdk.now++; loop(); sdk.now += 30; loop();
    TEST_ASSERT_EQUAL(16, sdk.timing.brp);
    usb_command("status\n"); output_contains("bitrate=250000");
    Serial.connected = false; loop(); TEST_ASSERT_TRUE(sdk.installed);
    Serial.connected = true; loop();
    Serial.negative_read = true; usb_command("help\n"); Serial.negative_read = false; loop();
    Serial.writable = 0; usb_command("status\n"); Serial.writable = 1024;
    Serial.partial_write = true; usb_command("help\n"); Serial.partial_write = false;
    Serial.lose_on_read = true; usb_command("status\n"); Serial.lose_on_read = false;
    loop(); Serial.connected = true; loop();
    sdk.now += 1000; loop(); sdk.alerts = TWAI_ALERT_TX_FAILED; sdk.now++; loop();
    output_contains("fault-reset-required");
    Serial.output.clear();
    Serial.connected = false; loop(); Serial.connected = true; loop();
    usb_command("status\n");
    output_contains("alerts=0x00000400");
    output_contains("bus_error_count=4294967295\n");
    usb_command("stop\nstart\nstatus\n");
    sdk.levels[1] = HIGH; sdk.levels[5] = LOW; sdk.now += 1000; loop();
    TEST_ASSERT_EQUAL(2, sdk.starts);
}
#elif !BENCH_AUTONOMOUS
void application_usb_lifecycle_and_faults() {
    // Exercise the actual setup()/loop() and adapters; only the SDK is replaced.
    setup(); TEST_ASSERT_EQUAL(0, sdk.installs); TEST_ASSERT_EQUAL(0, sdk.transmits);
    // Per-pin, so the assertion does not depend on which pin was written last.
    TEST_ASSERT_EQUAL(OUTPUT, sdk.modes[kTxPin]);
    TEST_ASSERT_EQUAL(HIGH, sdk.outputs[kTxPin]);   // Recessive before enabling the driver.
    TEST_ASSERT_EQUAL(HIGH, sdk.outputs[kLedPin]);  // Indicator dark at boot.
    TEST_ASSERT_EQUAL(115200, Serial.baud); TEST_ASSERT_EQUAL(1, Serial.timeout);
    Serial.send("start\n"); loop(); TEST_ASSERT_EQUAL(0, sdk.installs);
    Serial.connected = true; Serial.send("start\n"); loop();
    TEST_ASSERT_EQUAL(0, sdk.installs); output_contains("BENCH ONLY");
    usb_command("help\nstatus\nBAD\n\n"); output_contains("invalid command");
    usb_command("start\n"); TEST_ASSERT_EQUAL(1, sdk.starts);
    sdk.now = 999; loop(); TEST_ASSERT_EQUAL(0, sdk.transmits);
    sdk.now = 1000; loop(); TEST_ASSERT_EQUAL(1, sdk.transmits); output_contains("queued=1");
    TEST_ASSERT_EQUAL(HIGH, sdk.outputs[kLedPin]);  // Queued is not acknowledged.
    sdk.alerts = TWAI_ALERT_TX_SUCCESS; sdk.now = 1001; loop(); output_contains("transmitted=1");
    TEST_ASSERT_EQUAL(LOW, sdk.outputs[kLedPin]);   // Flash tracks the acknowledgement.
    sdk.now = 1001 + ActivityPulse::kPulseMs; loop();
    TEST_ASSERT_EQUAL(HIGH, sdk.outputs[kLedPin]);
    // An input flood must not hide a buffered stop behind another scheduled frame.
    sdk.now = 2000; Serial.send(std::string(64, '\n') + "stop\n");
    loop(); TEST_ASSERT_EQUAL(1, sdk.transmits);
    loop(); TEST_ASSERT_FALSE(sdk.installed);
    usb_command("start\n"); sdk.now = 3000; loop();
    sdk.alerts = TWAI_ALERT_TX_FAILED; sdk.now = 3001; loop(); output_contains("state=fault");
    usb_command("start\n"); output_contains("fault latched");
    usb_command("stop\nstart\n");
    Serial.connected = false; Serial.send("start\n"); loop(); TEST_ASSERT_FALSE(sdk.installed);
    Serial.connected = true; loop(); sdk.now = 5000; loop(); TEST_ASSERT_FALSE(sdk.installed);
    usb_command("sta"); Serial.connected = false; loop(); Serial.connected = true; loop();
    usb_command("rt\n"); TEST_ASSERT_FALSE(sdk.installed);
    // SDK read/write errors and backpressure do not hang or schedule transmission.
    Serial.negative_read = true; usb_command("help\n"); Serial.negative_read = false;
    loop(); Serial.writable = 0; usb_command("status\n"); Serial.writable = 1024;
    Serial.partial_write = true; usb_command("help\n"); Serial.partial_write = false;
    usb_command("status\n"); output_contains("logs_dropped=");
    usb_command("start\n"); Serial.lose_on_read = true; usb_command("help\n");
    TEST_ASSERT_FALSE(sdk.installed); Serial.lose_on_read = false;
    Serial.connected = true; loop(); usb_command("status\n");
    output_contains("state=stopped");
}
#else
void application_autonomous_without_usb() {
    sdk.counters = {TWAI_STATE_RUNNING, UINT32_MAX, UINT32_MAX, UINT32_MAX,
                    UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX};
    setup(); loop(); TEST_ASSERT_EQUAL(0, sdk.installs);
    sdk.now = 9999; loop(); TEST_ASSERT_EQUAL(0, sdk.installs);
    sdk.now = 10000; loop(); TEST_ASSERT_EQUAL(1, sdk.transmits);
    TEST_ASSERT_FALSE(Serial.connected);
    // The USB-less path is this profile's only LED update site, and the LED is its only
    // output: assert it here or a deleted service_led() call would go unnoticed.
    TEST_ASSERT_EQUAL(OUTPUT, sdk.modes[kLedPin]);
    TEST_ASSERT_EQUAL(HIGH, sdk.outputs[kLedPin]);
    sdk.alerts = TWAI_ALERT_TX_SUCCESS; sdk.now = 10001; loop();
    TEST_ASSERT_EQUAL(LOW, sdk.outputs[kLedPin]);
    sdk.now = 10001 + ActivityPulse::kPulseMs; loop();
    TEST_ASSERT_EQUAL(HIGH, sdk.outputs[kLedPin]);  // Expires; never latches on.
    Serial.connected = true; loop(); output_contains("AUTONOMOUS image");
    usb_command("start\nhelp\nstatus\nBAD\n\n"); output_contains("cannot restart");
    output_contains("invalid command");
    for (uint32_t n = 1; n < 10; ++n) {
        // Repeated host connection changes do not re-arm or cancel the active series.
        Serial.connected = (n % 2 == 0);
        sdk.now = 10000 + n * 1000; loop();
        TEST_ASSERT_EQUAL(n + 1, sdk.transmits);
        TEST_ASSERT_TRUE(sdk.installed);
        sdk.alerts = TWAI_ALERT_TX_SUCCESS; sdk.now += 1; loop();
    }
    TEST_ASSERT_FALSE(sdk.installed); TEST_ASSERT_EQUAL(1, sdk.uninstalls);
    Serial.connected = true; loop(); usb_command("status\n");
    output_contains("transmitted=10"); output_contains("state=stopped");
    output_contains("bus_error_count=4294967295\n");
    sdk.now = 1000000; usb_command("start\nstop\nstart\n");
    TEST_ASSERT_EQUAL(10, sdk.transmits); TEST_ASSERT_EQUAL(1, sdk.starts);
    Serial.negative_read = true; usb_command("help\n"); Serial.negative_read = false; loop();
    Serial.writable = 0; usb_command("status\n"); Serial.writable = 1024;
    Serial.partial_write = true; usb_command("help\n"); Serial.partial_write = false;
    usb_command("status\n"); output_contains("logs_dropped=");
    Serial.lose_on_read = true; usb_command("help\n"); Serial.lose_on_read = false;
    Serial.connected = true; loop(); TEST_ASSERT_EQUAL(10, sdk.transmits);
    Serial.send(std::string(64, '\n') + "stop\n"); loop(); loop();
    TEST_ASSERT_EQUAL(10, sdk.transmits);
}
#endif
}  // namespace

void setUp() { sdk = FakeSdk{}; Serial = FakeSerial{}; }
void tearDown() {}

extern "C" void __gcov_dump();
void run_button_tests();
void run_receiver_tests();

int main() {
    UNITY_BEGIN();
    RUN_TEST(parser_accepts_commands);
    RUN_TEST(parser_rejects_invalid_and_binary);
    RUN_TEST(parser_bounds_and_fragmentation);
    RUN_TEST(stopped_is_silent);
    RUN_TEST(cadence_and_fixed_frame);
    RUN_TEST(late_ticks_do_not_burst);
    RUN_TEST(clock_rollover_is_safe);
    RUN_TEST(stop_aborts_and_restart_waits);
    RUN_TEST(disconnect_stops_and_never_restarts);
    RUN_TEST(initialization_failure_requires_cleanup);
    RUN_TEST(submission_failure_is_not_queued);
    RUN_TEST(completion_timeout_and_late_success);
    RUN_TEST(fault_wins_over_success);
    RUN_TEST(bus_off_latches_until_reset);
    RUN_TEST(cleanup_failure_stays_fault);
    RUN_TEST(asynchronous_fault_prevents_due_submission);
    RUN_TEST(activity_pulse_expires_rearms_and_survives_rollover);
    RUN_TEST(names_cover_status_values);
    RUN_TEST(autonomous_countdown_and_exact_limit);
    RUN_TEST(autonomous_cancel_and_immediate_start_guards);
    RUN_TEST(autonomous_rollover_and_late_ticks);
    RUN_TEST(autonomous_errors_never_rearm);
    RUN_TEST(autonomous_final_completion_and_cleanup_failures);
    RUN_TEST(adapter_uses_real_contract_fields);
    RUN_TEST(adapter_install_and_start_errors);
    RUN_TEST(adapter_polls_alerts_and_status);
    RUN_TEST(adapter_bus_off_between_poll_and_stop);
    RUN_TEST(adapter_status_detects_bus_off);
    RUN_TEST(adapter_cleanup_errors_and_retry);
    RUN_TEST(adapter_cleanup_attempts_stop_without_status);
    RUN_TEST(diagnostics_survive_fault_cleanup_and_reset_on_new_start);
    RUN_TEST(diagnostics_mark_failed_reads_unavailable);
    run_button_tests();
    run_receiver_tests();
#if BENCH_RECEIVER
    RUN_TEST(application_receiver_never_transmits);
#elif BENCH_BUTTONS
    RUN_TEST(application_buttons_without_usb);
#elif BENCH_AUTONOMOUS
    RUN_TEST(application_autonomous_without_usb);
#else
    RUN_TEST(application_usb_lifecycle_and_faults);
#endif
    // PlatformIO may terminate the process after the Unity summary, before exit handlers.
    __gcov_dump();
    return UNITY_END();
}
