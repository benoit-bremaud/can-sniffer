#include "bench.h"

#include <cstring>

namespace bench {
namespace {
bool space(char c) { return c == ' ' || c == '\t'; }

Command parse(char* line) {
    while (space(*line)) { ++line; }
    std::size_t length = std::strlen(line);
    while (length && space(line[length - 1])) { line[--length] = '\0'; }
    if (!length) { return Command::None; }
    if (std::strcmp(line, "start") == 0) { return Command::Start; }
    if (std::strcmp(line, "stop") == 0) { return Command::Stop; }
    if (std::strcmp(line, "status") == 0) { return Command::Status; }
    if (std::strcmp(line, "help") == 0) { return Command::Help; }
    return Command::Invalid;
}
}  // namespace

Command CommandParser::feed(char byte) {
    if (byte == '\n') {
        line_[size_] = '\0';
        const Command result = invalid_ ? Command::Invalid : parse(line_);
        reset();
        return result;
    }
    if (invalid_) { return Command::None; }
    if (cr_) { invalid_ = true; return Command::None; }
    if (byte == '\r') { cr_ = true; return Command::None; }
    if (size_ == kCommandLimit || (byte != '\t' && (byte < ' ' || byte > '~'))) {
        invalid_ = true;
        return Command::None;
    }
    line_[size_++] = byte;
    return Command::None;
}

void CommandParser::reset() {
    size_ = 0;
    invalid_ = false;
    cr_ = false;
}

void BenchController::command(Command command, uint32_t now) {
    if (command == Command::Stop) { stop(); return; }
    if (command != Command::Start || status_.state != State::Stopped) { return; }
    if (!port_.start(bitrate_)) { fail(Fault::Initialization); return; }
    status_.state = State::Running;
    last_attempt_ = now;
}

void BenchController::fail(Fault fault) {
    if (status_.pending) { ++status_.failed; }
    status_.pending = false;
    status_.state = State::Fault;
    status_.last_fault = fault;
    if (fault == Fault::BusOff) { status_.bus_off_latched = true; }
    if (!port_.stop()) { status_.last_fault = Fault::Cleanup; }
    status_.bus_off_latched = status_.bus_off_latched || port_.poll().bus_off;
}

void BenchController::start_immediately(uint32_t now) {
    if (status_.state != State::Stopped) { return; }
    command(Command::Start, now);
    // The runner already waited its countdown. Unsigned subtraction is wrap-safe.
    if (status_.state == State::Running) { last_attempt_ = now - kPeriodMs; }
}

void BenchController::stop() {
    if (status_.state == State::Stopped) { return; }
    if (status_.pending) { ++status_.aborted; }
    status_.pending = false;
    const bool clean = port_.stop();
    // Preserve a bus-off that occurred between the last poll and cleanup.
    status_.bus_off_latched = status_.bus_off_latched || port_.poll().bus_off;
    if (!clean) { status_.last_fault = Fault::Cleanup; }
    else if (status_.bus_off_latched) { status_.last_fault = Fault::BusOff; }
    status_.state = clean && !status_.bus_off_latched ? State::Stopped : State::Fault;
}

void BenchController::disconnect() { stop(); }

void BenchController::service(uint32_t now) {
    if (status_.state != State::Running) { return; }
    const Result result = port_.poll();
    // Never accept success when any fault is present in the same poll.
    if (result.bus_off) { fail(Fault::BusOff); return; }
    if (result.driver_error) { fail(Fault::Driver); return; }
    if (result.failure) { fail(Fault::Transmission); return; }
    if (!status_.pending) { return; }
    // At the deadline, an untimestamped completion cannot prove timely success.
    if (static_cast<uint32_t>(now - last_attempt_) >= kCompletionMs) {
        fail(Fault::Timeout);
        return;
    }
    if (result.success) {
        ++status_.succeeded;
        status_.pending = false;
    }
}

bool BenchController::configure(Bitrate bitrate) {
    if (status_.state != State::Stopped ||
        (bitrate != Bitrate::K125 && bitrate != Bitrate::K250)) { return false; }
    bitrate_ = bitrate;
    return true;
}

void BenchController::tick(uint32_t now, const Frame& frame) {
    service(now);
    if (status_.state != State::Running || status_.pending ||
        static_cast<uint32_t>(now - last_attempt_) < kPeriodMs) { return; }
    last_attempt_ = now;  // Late ticks do not replay missed slots.
    if (!port_.submit(frame)) {
        ++status_.failed;
        fail(Fault::Submission);
        return;
    }
    ++status_.queued;
    status_.pending = true;
}

void BurstRunner::service(uint32_t now) {
    controller_.service(now);
    if (!started_ || finished_) { return; }
    const auto& status = controller_.status();
    if (status.state != State::Running) { finished_ = true; return; }
    // Do not abort the tenth frame just because the driver accepted its submission.
    if (status.queued >= kLimit && !status.pending) { stop(now); }
}

void BurstRunner::tick(uint32_t now) {
    service(now);
    if (finished_) { return; }
    if (!started_) {
        if (static_cast<uint32_t>(now - boot_ms_) < kDelayMs) { return; }
        started_ = true;  // Consume activation before touching hardware, even on failure.
        controller_.start_immediately(now);
    }
    if (controller_.status().queued < kLimit) { controller_.tick(now); }
    service(now);
}

void BurstRunner::stop(uint32_t now) {
    finished_ = true;
    controller_.command(Command::Stop, now);
}

const char* state_name(State state) {
    switch (state) {
    case State::Stopped: return "stopped";
    case State::Running: return "running";
    case State::Fault: return "fault";
    }
    return "unknown";
}

const char* fault_name(Fault fault) {
    switch (fault) {
    case Fault::None: return "none";
    case Fault::Initialization: return "initialization";
    case Fault::Submission: return "submission";
    case Fault::Transmission: return "transmission (ACK/wiring/timing/etc.)";
    case Fault::Timeout: return "completion-timeout";
    case Fault::BusOff: return "bus-off (reset required)";
    case Fault::Driver: return "driver-status";
    case Fault::Cleanup: return "cleanup (controller state unknown)";
    }
    return "unknown";
}
}  // namespace bench
