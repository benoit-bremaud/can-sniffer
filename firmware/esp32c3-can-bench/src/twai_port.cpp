#include "twai_port.h"

#include <driver/twai.h>
#include <cstring>

#ifndef BENCH_NO_ACK
#define BENCH_NO_ACK 0
#endif

namespace {
// Diagnostic builds only. NO_ACK reports every transmission as successful without any
// acknowledgement, which isolates the controller/transceiver from the rest of the bus.
constexpr twai_mode_t kMode = BENCH_NO_ACK ? TWAI_MODE_NO_ACK : TWAI_MODE_NORMAL;
}  // namespace

bool TwaiPort::start(bench::Bitrate bitrate) {
    if (installed_ || bus_off_) { return false; }
    if (bitrate != bench::Bitrate::K125 && bitrate != bench::Bitrate::K250) { return false; }
    diagnostics_ = Diagnostics{};
    twai_general_config_t general = TWAI_GENERAL_CONFIG_DEFAULT(
        static_cast<gpio_num_t>(bench::kTxPin),
        static_cast<gpio_num_t>(bench::kRxPin), kMode);
    general.tx_queue_len = 0;
    general.rx_queue_len = 1;
    general.alerts_enabled = TWAI_ALERT_TX_SUCCESS | TWAI_ALERT_TX_FAILED | TWAI_ALERT_BUS_OFF;
    twai_timing_config_t timing = TWAI_TIMING_CONFIG_125KBITS();
    if (bitrate == bench::Bitrate::K250) {
        const twai_timing_config_t faster = TWAI_TIMING_CONFIG_250KBITS();
        timing = faster;
    }
    const twai_filter_config_t filter = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    if (twai_driver_install(&general, &timing, &filter) != ESP_OK) { return false; }
    installed_ = true;  // Keep ownership even if start fails so cleanup can retry.
    return twai_start() == ESP_OK;
}

bool TwaiPort::submit(const bench::Frame& frame) {
    if (!installed_ || bus_off_) { return false; }
    twai_message_t message = {};
    message.identifier = frame.id;
    message.extd = 1;
    message.ss = 1;
    message.data_length_code = sizeof(frame.data);
    std::memcpy(message.data, frame.data, sizeof(frame.data));
    return twai_transmit(&message, 0) == ESP_OK;
}

bench::Result TwaiPort::poll() {
    bench::Result result;
    result.bus_off = bus_off_;
    if (!installed_) { return result; }
    uint32_t alerts = 0;
    const esp_err_t error = twai_read_alerts(&alerts, 0);
    diagnostics_ = Diagnostics{};
    diagnostics_.sampled = true;
    diagnostics_.alerts_available = error == ESP_OK || error == ESP_ERR_TIMEOUT;
    diagnostics_.alerts = diagnostics_.alerts_available ? alerts : 0;
    result.driver_error = error != ESP_OK && error != ESP_ERR_TIMEOUT;
    twai_status_info_t info = {};
    diagnostics_.status_available = twai_get_status_info(&info) == ESP_OK;
    if (diagnostics_.status_available) { diagnostics_.status = info; }
    if (!diagnostics_.status_available) {
        result.driver_error = true;
    } else if (info.state == TWAI_STATE_BUS_OFF) {
        bus_off_ = true;
    } else if (info.state != TWAI_STATE_RUNNING) {
        result.driver_error = true;
    }
    bus_off_ = bus_off_ || (alerts & TWAI_ALERT_BUS_OFF);
    result.bus_off = bus_off_;
    result.success = (alerts & TWAI_ALERT_TX_SUCCESS) != 0;
    result.failure = (alerts & TWAI_ALERT_TX_FAILED) != 0;
    return result;
}

TwaiPort::ReceiveResult TwaiPort::receive(twai_message_t& frame) {
    if (!installed_ || bus_off_) { return ReceiveResult::Error; }
    twai_message_t next = {};
    const auto error = twai_receive(&next, 0);
    if (error == ESP_ERR_TIMEOUT) { return ReceiveResult::Empty; }
    if (error != ESP_OK || next.data_length_code > 8 ||
        next.identifier > (next.extd ? 0x1FFFFFFFu : 0x7FFu)) { return ReceiveResult::Error; }
    frame = next;
    return ReceiveResult::Frame;
}

bool TwaiPort::stop() {
    if (!installed_) { return true; }
    twai_status_info_t info = {};
    if (twai_get_status_info(&info) != ESP_OK) {
        // Best effort to cease activity even when status inspection failed.
        if (twai_stop() != ESP_OK) { return false; }
    } else if (info.state == TWAI_STATE_RUNNING) {
        if (twai_stop() != ESP_OK) { return false; }
    } else if (info.state == TWAI_STATE_BUS_OFF) {
        bus_off_ = true;  // Uninstall is allowed here, but restart remains forbidden.
    } else if (info.state != TWAI_STATE_STOPPED) {
        return false;
    }
    if (twai_driver_uninstall() != ESP_OK) { return false; }
    installed_ = false;
    return true;
}
