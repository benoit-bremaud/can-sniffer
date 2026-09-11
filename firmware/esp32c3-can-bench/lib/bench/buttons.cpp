#include "buttons.h"

namespace bench {
bool DebouncedButtons::sample(uint8_t raw, uint32_t now) {
    if (!initialized_ || raw != raw_) {
        initialized_ = true;
        raw_ = raw;
        changed_ = now;
        return false;
    }
    return static_cast<uint32_t>(now - changed_) >= kDebounceMs;
}

RunCounts ButtonTestRunner::counts() const {
    const auto& status = controller_.status();
    RunCounts result;
    result.queued = status.queued - baseline_.queued;
    result.succeeded = status.succeeded - baseline_.succeeded;
    result.failed = status.failed - baseline_.failed;
    result.aborted = status.aborted - baseline_.aborted;
    return result;
}

void ButtonTestRunner::finish(uint32_t now) {
    const bool fault = state_ == ButtonState::Fault ||
                       controller_.status().state == State::Fault;
    controller_.command(Command::Stop, now);
    state_ = fault || controller_.status().state == State::Fault
        ? ButtonState::Fault : ButtonState::ReleaseWait;
    buttons_.reset();
}

void ButtonTestRunner::stop(uint32_t now) { finish(now); }

void ButtonTestRunner::service(uint32_t now) {
    controller_.service(now);
    if (state_ != ButtonState::Running) { return; }
    if (controller_.status().state != State::Running) {
        // Preserve the original failure diagnostic; cleanup already ran in the controller.
        state_ = ButtonState::Fault;
        buttons_.reset();
        return;
    }
    const auto* definition = scenario_definition(scenario_);
    if (counts().queued >= definition->attempts && !controller_.status().pending) {
        finish(now);
    }
}

void ButtonTestRunner::tick(uint32_t now, uint8_t raw_mask) {
    service(now);
    if (state_ == ButtonState::Fault) { return; }
    const bool stable = buttons_.sample(raw_mask, now);
    if (state_ == ButtonState::ReleaseWait) {
        if (stable && raw_mask == 0) { state_ = ButtonState::Ready; }
        return;
    }
    if (state_ == ButtonState::Ready) {
        if (!stable || raw_mask == 0) { return; }
        Scenario selected = Scenario::None;
        switch (raw_mask) {
        case 1: selected = Scenario::Reference125; break;
        case 2: selected = Scenario::Reference250; break;
        case 4: selected = Scenario::Varied125; break;
        default: buttons_.reset(); state_ = ButtonState::ReleaseWait; return;
        }
        baseline_ = controller_.status();
        scenario_ = selected;
        if (!controller_.configure(scenario_definition(selected)->bitrate)) {
            state_ = ButtonState::Fault;
            finish(now);
            return;
        }
        state_ = ButtonState::Running;
        controller_.command(Command::Start, now);
    }
    // Busy input is sampled but never queued. finish() resets the release gate.
    Frame frame;
    if (scenario_frame(scenario_, counts().queued, frame)) { controller_.tick(now, frame); }
    service(now);
}

const char* button_state_name(ButtonState state) {
    switch (state) {
    case ButtonState::ReleaseWait: return "release-wait";
    case ButtonState::Ready: return "ready";
    case ButtonState::Running: return "running";
    case ButtonState::Fault: return "fault-reset-required";
    }
    return "unknown";
}
}  // namespace bench
