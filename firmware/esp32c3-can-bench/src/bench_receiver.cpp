#include "bench_receiver.h"

void BenchReceiver::start() {
    if (state_ != bench::State::Stopped) { return; }
    count_ = 0; has_frame_ = false; last_ = {}; fault_ = bench::Fault::None;
    if (!port_.start()) { fail(bench::Fault::Initialization); return; }
    state_ = bench::State::Running;
}

void BenchReceiver::fail(bench::Fault fault) {
    state_ = bench::State::Fault;
    fault_ = port_.stop() ? fault : bench::Fault::Cleanup;
}

void BenchReceiver::stop() {
    if (state_ == bench::State::Stopped) { return; }
    if (!port_.stop()) { state_ = bench::State::Fault; fault_ = bench::Fault::Cleanup; return; }
    if (port_.poll().bus_off) { state_ = bench::State::Fault; fault_ = bench::Fault::BusOff; return; }
    state_ = bench::State::Stopped;
}

bool BenchReceiver::service() {
    if (state_ != bench::State::Running) { return false; }
    const auto result = port_.poll();
    if (result.bus_off) { fail(bench::Fault::BusOff); return false; }
    if (result.driver_error || result.failure) { fail(bench::Fault::Driver); return false; }
    const auto received = port_.receive(last_);
    if (received == TwaiPort::ReceiveResult::Error) { fail(bench::Fault::Driver); return false; }
    if (received == TwaiPort::ReceiveResult::Empty) { return false; }
    has_frame_ = true;
    if (count_ != UINT32_MAX) { ++count_; }
    return true;
}
