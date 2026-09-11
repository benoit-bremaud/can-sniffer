#include <Arduino.h>
#include <activity.h>
#include <bench.h>
#include <buttons.h>
#include <cstdio>
#include <cstring>

#include "twai_port.h"
#include "bench_receiver.h"

#ifndef BENCH_AUTONOMOUS
#define BENCH_AUTONOMOUS 0
#endif
#ifndef BENCH_BUTTONS
#define BENCH_BUTTONS 0
#endif
#ifndef BENCH_RECEIVER
#define BENCH_RECEIVER 0
#endif
#ifndef BENCH_NO_ACK
#define BENCH_NO_ACK 0
#endif
#if (BENCH_AUTONOMOUS + BENCH_BUTTONS + BENCH_RECEIVER) > 1
#error "Select only one bench profile"
#endif

namespace {
TwaiPort can;
#if BENCH_RECEIVER
BenchReceiver receiver(can);
#else
bench::BenchController controller(can);
#endif
#if BENCH_AUTONOMOUS
bench::BurstRunner burst(controller);
#endif
#if BENCH_BUTTONS
bench::ButtonTestRunner buttons(controller);
#endif
bench::CommandParser parser;
#if !BENCH_RECEIVER
bench::ActivityPulse led;
uint32_t led_succeeded = 0;
#endif
bool connected = false;
uint32_t dropped_logs = 0;
constexpr unsigned kInputBudget = 64;
constexpr unsigned kRxBufferSize = 256;

void log_line(const char* text) {
    const std::size_t size = std::strlen(text);
    // Do not block control processing behind a slow/disconnected USB reader.
    if (!Serial || Serial.availableForWrite() < static_cast<int>(size) ||
        Serial.write(reinterpret_cast<const uint8_t*>(text), size) != size) {
        ++dropped_logs;
    }
}

void diagnostic_lines() {
    const auto& diagnostic = can.diagnostics();
    if (!diagnostic.sampled) { log_line("twai_snapshot=unavailable\n"); return; }
    char line[320];
    if (diagnostic.alerts_available) {
        std::snprintf(line, sizeof(line), "twai_snapshot=last_poll alerts=0x%08lx\n",
                      static_cast<unsigned long>(diagnostic.alerts));
        log_line(line);
    } else { log_line("twai_snapshot=last_poll alerts=unavailable\n"); }
    if (!diagnostic.status_available) { log_line("twai_status=unavailable\n"); return; }
    const auto& info = diagnostic.status;
    std::snprintf(line, sizeof(line),
        "twai_state=%d tx_error_counter=%lu rx_error_counter=%lu tx_failed_count=%lu "
        "rx_missed_count=%lu rx_overrun_count=%lu arb_lost_count=%lu bus_error_count=%lu\n",
        static_cast<int>(info.state), static_cast<unsigned long>(info.tx_error_counter),
        static_cast<unsigned long>(info.rx_error_counter), static_cast<unsigned long>(info.tx_failed_count),
        static_cast<unsigned long>(info.rx_missed_count), static_cast<unsigned long>(info.rx_overrun_count),
        static_cast<unsigned long>(info.arb_lost_count), static_cast<unsigned long>(info.bus_error_count));
    log_line(line);
}

#if BENCH_RECEIVER
void received_line() {
    if (!receiver.has_frame()) { log_line("last_frame=none\n"); return; }
    const auto& frame = receiver.last_frame();
    char payload[25] = {};
    static_assert(sizeof(payload) >= 8 * 3 + 1, "eight bytes as \"%02X \" plus NUL");
    if (!frame.rtr) {
        // Bound here, not only at the SDK boundary: ESP-IDF reports DLC 9-15 verbatim, and
        // sizeof(payload) - i * 3 is unsigned, so i >= 9 would wrap and unbound the write.
        const unsigned length = frame.data_length_code > 8 ? 8u : frame.data_length_code;
        for (unsigned i = 0; i < length; ++i) {
            std::snprintf(payload + i * 3, sizeof(payload) - i * 3, "%02X ", frame.data[i]);
        }
    }
    char line[128];
    std::snprintf(line, sizeof(line), "rx_id=0x%08lx extended=%u rtr=%u dlc=%u data=[%s]\n",
        static_cast<unsigned long>(frame.identifier), static_cast<unsigned>(frame.extd),
        static_cast<unsigned>(frame.rtr), static_cast<unsigned>(frame.data_length_code), payload);
    log_line(line);
}
#endif

void status_line() {
#if BENCH_NO_ACK
    // Marks every sample so a captured log can never be mistaken for an acknowledged run.
    log_line("selftest=no_ack ack_required=0\n");
#endif
#if BENCH_RECEIVER
    char line[192];
    std::snprintf(line, sizeof(line), "receiver=%s bitrate=125000 received=%lu fault=%s logs_dropped=%lu\n",
        receiver.state() == bench::State::Running ? "ready" : bench::state_name(receiver.state()),
        static_cast<unsigned long>(receiver.count()), bench::fault_name(receiver.fault()),
        static_cast<unsigned long>(dropped_logs));
    log_line(line);
    received_line();
#else
    const auto& status = controller.status();
    char line[384];
    std::snprintf(line, sizeof(line),
        "state=%s bitrate=%lu tx=%d rx=%d pending=%u queued=%lu transmitted=%lu "
        "failed=%lu aborted=%lu fault=%s bus_off_latched=%u logs_dropped=%lu\n",
        bench::state_name(status.state), static_cast<unsigned long>(controller.bitrate()),
        bench::kTxPin, bench::kRxPin, static_cast<unsigned>(status.pending),
        static_cast<unsigned long>(status.queued), static_cast<unsigned long>(status.succeeded),
        static_cast<unsigned long>(status.failed), static_cast<unsigned long>(status.aborted),
        bench::fault_name(status.last_fault), static_cast<unsigned>(status.bus_off_latched),
        static_cast<unsigned long>(dropped_logs));
    log_line(line);
#if BENCH_BUTTONS
    const auto counts = buttons.counts();
    std::snprintf(line, sizeof(line),
        "buttons=%s scenario=%s run_queued=%lu run_transmitted=%lu run_failed=%lu run_aborted=%lu\n",
        bench::button_state_name(buttons.state()), bench::scenario_name(buttons.scenario()),
        static_cast<unsigned long>(counts.queued), static_cast<unsigned long>(counts.succeeded),
        static_cast<unsigned long>(counts.failed), static_cast<unsigned long>(counts.aborted));
    log_line(line);
#endif
#endif
    diagnostic_lines();
}

#if !BENCH_RECEIVER
// One call per loop covers every emission path, including the USB-less autonomous one.
void service_led(uint32_t now) {
    const uint32_t succeeded = controller.status().succeeded;
    if (succeeded != led_succeeded) { led_succeeded = succeeded; led.pulse(now); }
    digitalWrite(bench::kLedPin, led.active(now) ? LOW : HIGH);
}
#endif

void help() {
#if BENCH_NO_ACK
    log_line("SELF-TEST IMAGE (TWAI_MODE_NO_ACK): the controller reports every transmission\n"
             "as successful without any acknowledgement. A pass here proves the controller,\n"
             "the transceiver and the bit timing only. It proves nothing about the receiver,\n"
             "the wiring or the bus. Never use this image for an acceptance test.\n");
#endif
#if BENCH_RECEIVER
    log_line("BENCH ONLY: never connect to a charger. RECEIVER image; silent at boot.\n"
             "start enables 125k normal-mode reception (ACK/error signaling, not listen-only).\n"
             "No data-frame transmission. Commands: start | stop | status | help.\n"
             "USB loss keeps reception active; stop or remove power to end it.\n");
#elif BENCH_BUTTONS
    log_line("BENCH ONLY: never connect to a charger. BUTTONS image; silent at boot.\n"
             "Blue GPIO0: 10 at 125k. Red GPIO1: 10 at 250k. White GPIO5: 12 varied at 125k.\n"
             "All 1 Hz. Release before selecting. Fault requires reset.\n"
             "USB optional. Commands: stop | status | help. start disabled.\n");
#elif BENCH_AUTONOMOUS
    log_line("BENCH ONLY: never connect to a charger. AUTONOMOUS image.\n"
             "Boot/reset: wait 10 s, at most 10 frames at 1 Hz, then stop.\n"
             "USB optional. Commands: stop | status | help. start disabled.\n"
             "Frame: 0x001ABCDE [01 02 03 04 05 06 07 08], extended, 125 kbit/s.\n");
#else
    log_line("BENCH ONLY: disconnect every charger. start asserts this prerequisite.\n"
             "Commands: start | stop | status | help (LF/CRLF). No automatic restart.\n"
             "Frame: 0x001ABCDE [01 02 03 04 05 06 07 08], extended, 125 kbit/s, 1 Hz.\n");
#endif
}

void discard_input() {
    // The configured SDK RX queue has this capacity; never use an unbounded drain.
    for (unsigned i = 0; i < kRxBufferSize && Serial.available() > 0; ++i) {
        Serial.read();
    }
    parser.reset();
}

void handle(bench::Command command, uint32_t now) {
    if (command == bench::Command::None) { return; }
    if (command == bench::Command::Invalid) { log_line("error: invalid command\n"); return; }
    if (command == bench::Command::Help) { help(); return; }
#if BENCH_RECEIVER
    (void)now;
    if (command == bench::Command::Start) { receiver.start(); }
    if (command == bench::Command::Stop) { receiver.stop(); }
#elif BENCH_BUTTONS
    if (command == bench::Command::Start) { log_line("error: use a physical button\n"); }
    if (command == bench::Command::Stop) { buttons.stop(now); }
#elif BENCH_AUTONOMOUS
    if (command == bench::Command::Start) {
        log_line("error: autonomous series cannot restart via start; reset required\n");
    }
    if (command == bench::Command::Stop) { burst.stop(now); }
#else
    if (command == bench::Command::Start && controller.status().state == bench::State::Fault) {
        log_line("error: fault latched; stop for cleanup, reset after bus-off\n");
    }
    controller.command(command, now);
#endif
    status_line();
}
}  // namespace

void setup() {
    // Recessive TX level before enabling the driver. This is not a power-up interlock.
    digitalWrite(bench::kTxPin, HIGH);
    pinMode(bench::kTxPin, OUTPUT);
#if !BENCH_RECEIVER
    // Active low: HIGH is off. Driven only here, after boot released the strapping pin.
    digitalWrite(bench::kLedPin, HIGH);
    pinMode(bench::kLedPin, OUTPUT);
#endif
#if BENCH_BUTTONS
    pinMode(bench::kBluePin, INPUT_PULLUP);
    pinMode(bench::kRedPin, INPUT_PULLUP);
    pinMode(bench::kWhitePin, INPUT_PULLUP);
#endif
    Serial.setRxBufferSize(kRxBufferSize);
    Serial.setTxBufferSize(1024);
    Serial.begin(115200);
    Serial.setTxTimeoutMs(1);
}

void loop() {
#if BENCH_RECEIVER
    const auto before = receiver.state();
    const bool present = static_cast<bool>(Serial);
    if (present != connected || !present) {
        discard_input();
        connected = present;
        if (present) { help(); status_line(); }
    } else {
        for (unsigned i = 0; i < kInputBudget && Serial.available() > 0; ++i) {
            const int byte = Serial.read();
            if (byte < 0) { break; }
            handle(parser.feed(static_cast<char>(byte)), millis());
        }
    }
    if (receiver.service()) { received_line(); }
    if (before != receiver.state()) { status_line(); }
    delay(1);
#elif BENCH_BUTTONS
    const auto before = controller.status();
    const auto before_buttons = buttons.state();
    buttons.service(millis());
    const bool present = static_cast<bool>(Serial);
    if (present != connected || !present) {
        discard_input();
        connected = present;
        if (present) { help(); status_line(); }
    } else {
        for (unsigned i = 0; i < kInputBudget && Serial.available() > 0; ++i) {
            const int byte = Serial.read();
            if (byte < 0) { break; }
            handle(parser.feed(static_cast<char>(byte)), millis());
        }
    }
    const uint8_t mask = (digitalRead(bench::kBluePin) == LOW ? 1 : 0) |
                         (digitalRead(bench::kRedPin) == LOW ? 2 : 0) |
                         (digitalRead(bench::kWhitePin) == LOW ? 4 : 0);
    buttons.tick(millis(), mask);  // Neither a USB host nor drained logging is required.
    const auto& after = controller.status();
    if (before_buttons != buttons.state() || before.queued != after.queued ||
        before.succeeded != after.succeeded || before.state != after.state) { status_line(); }
    service_led(millis());
    delay(1);
#else
#if BENCH_AUTONOMOUS
    burst.service(millis());
#endif
    const bool present = static_cast<bool>(Serial);
    if (!present || !connected) {
#if BENCH_AUTONOMOUS
        burst.tick(millis());  // Standalone USB power need not enumerate as a serial device.
#else
        controller.disconnect();
#endif
        discard_input();
        if (present) { help(); status_line(); }
        connected = present;
        service_led(millis());
        delay(1);
        return;
    }

    const auto before = controller.status();
    controller.service(millis());
    for (unsigned i = 0; i < kInputBudget && Serial.available() > 0; ++i) {
        const int byte = Serial.read();
        if (byte < 0) { break; }
        handle(parser.feed(static_cast<char>(byte)), millis());
    }
    // Finish buffered commands (especially stop) before allowing another frame.
    if (!Serial) {
#if !BENCH_AUTONOMOUS
        controller.disconnect();
#endif
        discard_input();
        connected = false;
    } else if (Serial.available() == 0) {
#if BENCH_AUTONOMOUS
        burst.tick(millis());
#else
        controller.tick(millis());
#endif
    }
    const auto& after = controller.status();
    if (before.state != after.state || before.queued != after.queued ||
        before.succeeded != after.succeeded || before.failed != after.failed ||
        before.aborted != after.aborted || before.last_fault != after.last_fault) {
        status_line();
    }
    service_led(millis());
    delay(1);
#endif
}
