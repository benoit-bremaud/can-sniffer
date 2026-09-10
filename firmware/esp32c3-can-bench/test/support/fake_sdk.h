#pragma once
#include <Arduino.h>
#include <driver/twai.h>

#ifndef BENCH_NO_ACK
#define BENCH_NO_ACK 0
#endif
/// Mode TwaiPort must request for the profile under build; NO_ACK is diagnostic only.
constexpr twai_mode_t kExpectedTwaiMode = BENCH_NO_ACK ? TWAI_MODE_NO_ACK : TWAI_MODE_NORMAL;

struct FakeSdk {
    esp_err_t install_result = ESP_OK, start_result = ESP_OK, stop_result = ESP_OK;
    esp_err_t uninstall_result = ESP_OK, transmit_result = ESP_OK, status_result = ESP_OK;
    esp_err_t alert_result = ESP_OK;
    esp_err_t receive_result = ESP_OK;
    std::deque<twai_message_t> incoming;
    unsigned receives = 0;
    TickType_t receive_wait = 999;
    int installs = 0, starts = 0, stops = 0, uninstalls = 0, transmits = 0;
    uint32_t now = 0, alerts = 0;
    int pin = -1, level = -1, mode = -1;
    int levels[22] = {};
    int modes[22] = {};
    int outputs[22] = {};  // Last level written per pin, kept apart from input levels.
    bool installed = false;
    twai_state_t state = TWAI_STATE_STOPPED;
    twai_status_info_t counters = {};
    twai_general_config_t general = {};
    twai_timing_config_t timing = {};
    twai_filter_config_t filter = {};
    twai_message_t message = {};
    TickType_t transmit_wait = 999, alert_wait = 999;
};
extern FakeSdk sdk;
void setup();
void loop();
