#pragma once

#include <cstddef>
#include <cstdint>

namespace bench {

constexpr uint32_t kBitrate = 125000;
constexpr int kTxPin = 4;
constexpr int kRxPin = 3;
/// Onboard LED of the ESP32-C3 Super Mini, active low. Strapping pin: drive it after boot only.
constexpr int kLedPin = 8;
constexpr uint32_t kPeriodMs = 1000;
constexpr uint32_t kCompletionMs = 250;
constexpr std::size_t kCommandLimit = 32;
enum class Bitrate : uint32_t { K125 = 125000, K250 = 250000 };

struct Frame {
    uint32_t id = 0x001ABCDE;
    uint8_t data[8] = {1, 2, 3, 4, 5, 6, 7, 8};
};

enum class Command { None, Start, Stop, Status, Help, Invalid };
enum class State { Stopped, Running, Fault };
enum class Fault { None, Initialization, Submission, Transmission, Timeout, BusOff, Driver, Cleanup };

struct Result {
    bool success = false;
    bool failure = false;
    bool bus_off = false;
    bool driver_error = false;
};

/// Nonblocking CAN boundary. Failed start still requires stop() for partial cleanup.
class CanPort {
public:
    virtual ~CanPort() = default;
    /// Initialize/start with no software TX queue. Never retry automatically.
    virtual bool start(Bitrate bitrate = Bitrate::K125) = 0;
    /// Submit one extended eight-byte single-shot frame; true means accepted, not ACKed.
    virtual bool submit(const Frame& frame) = 0;
    /// Consume completion flags; bus_off remains latched even after successful cleanup.
    virtual Result poll() = 0;
    /// Cease activity and clear pending results; false means cleanup is unconfirmed.
    virtual bool stop() = 0;
};

/// Fixed-size LF/CRLF parser. An invalid/overlong line is discarded through LF.
class CommandParser {
public:
    /// None until LF; Invalid for the entire malformed/overlong line, never its suffix.
    Command feed(char byte);
    /// Drop partial input when the USB connection changes.
    void reset();
private:
    char line_[kCommandLimit + 1] = {};
    std::size_t size_ = 0;
    bool invalid_ = false;
    bool cr_ = false;
};

struct Status {
    State state = State::Stopped;
    Fault last_fault = Fault::None;
    uint32_t queued = 0;
    uint32_t succeeded = 0;
    uint32_t failed = 0;
    uint32_t aborted = 0;
    bool pending = false;
    bool bus_off_latched = false;
};

/// Time uses uint32_t milliseconds. Call service() before commands and tick() last.
class BenchController {
public:
    explicit BenchController(CanPort& port) : port_(port) {}
    /// Start only from Stopped, stop idempotently; all other commands leave CAN unchanged.
    void command(Command command, uint32_t now);
    /// Autonomous runner only: after its countdown, schedule the first attempt on this tick.
    /// Already-running/faulted controllers are unchanged; this does not submit by itself.
    void start_immediately(uint32_t now);
    /// Select a supported bitrate only while stopped. Does not start hardware.
    bool configure(Bitrate bitrate);
    Bitrate bitrate() const { return bitrate_; }
    /// Poll faults/completion without ever submitting a frame (including under input load).
    void service(uint32_t now);
    /// Service results, then submit at most one due attempt without catching up missed slots.
    void tick(uint32_t now, const Frame& frame = Frame{});
    /// Stop on USB loss; reconnection must not implicitly call Start.
    void disconnect();
    const Status& status() const { return status_; }
private:
    void stop();
    void fail(Fault fault);
    CanPort& port_;
    Status status_;
    uint32_t last_attempt_ = 0;
    Bitrate bitrate_ = Bitrate::K125;
};

/// One autonomous series per instance/MCU boot; owns no hardware or USB dependencies.
/// Do not issue commands directly to the wrapped controller while this runner owns it.
class BurstRunner {
public:
    static constexpr uint32_t kDelayMs = 10000;
    static constexpr uint32_t kLimit = 10;
    explicit BurstRunner(BenchController& controller, uint32_t boot_ms = 0)
        : controller_(controller), boot_ms_(boot_ms) {}
    /// Consume faults/completions without submitting, including the final attempt.
    void service(uint32_t now);
    /// Arm once at the countdown deadline; submit at most ten attempts via the controller.
    void tick(uint32_t now);
    /// Cancel the countdown/series permanently. Repeated calls may retry failed cleanup.
    void stop(uint32_t now);
    bool finished() const { return finished_; }
private:
    BenchController& controller_;
    uint32_t boot_ms_;
    bool started_ = false;
    bool finished_ = false;
};

/// Stable diagnostic labels, with a defensive fallback for unknown enum values.
const char* state_name(State state);
const char* fault_name(Fault fault);

}  // namespace bench
