#include <fake_sdk.h>

FakeSdk sdk;
FakeSerial Serial;
uint32_t millis() { return sdk.now; }
void delay(uint32_t) {}
void digitalWrite(int pin, int level) { sdk.pin = pin; sdk.level = level; sdk.outputs[pin] = level; }
void pinMode(int pin, int mode) {
    sdk.pin = pin; sdk.mode = mode; sdk.modes[pin] = mode;
    if (mode == INPUT_PULLUP) { sdk.levels[pin] = HIGH; }
}
int digitalRead(int pin) { return sdk.levels[pin]; }

esp_err_t twai_driver_install(const twai_general_config_t* g, const twai_timing_config_t* t,
                            const twai_filter_config_t* f) {
    ++sdk.installs; sdk.general = *g; sdk.timing = *t; sdk.filter = *f;
    if (sdk.install_result == ESP_OK) { sdk.installed = true; sdk.state = TWAI_STATE_STOPPED; }
    return sdk.install_result;
}
esp_err_t twai_start() {
    ++sdk.starts;
    if (sdk.start_result == ESP_OK) { sdk.state = TWAI_STATE_RUNNING; sdk.incoming.clear(); }
    return sdk.start_result;
}
esp_err_t twai_stop() {
    ++sdk.stops;
    if (sdk.stop_result == ESP_OK) { sdk.state = TWAI_STATE_STOPPED; }
    return sdk.stop_result;
}
esp_err_t twai_driver_uninstall() {
    ++sdk.uninstalls;
    if (sdk.uninstall_result == ESP_OK) { sdk.installed = false; sdk.alerts = 0; }
    return sdk.uninstall_result;
}
esp_err_t twai_transmit(const twai_message_t* message, TickType_t wait) {
    ++sdk.transmits; sdk.message = *message; sdk.transmit_wait = wait;
    return sdk.transmit_result;
}
esp_err_t twai_receive(twai_message_t* message, TickType_t wait) {
    ++sdk.receives; sdk.receive_wait = wait;
    if (!sdk.installed) { return ESP_FAIL; }
    if (sdk.receive_result != ESP_OK) { return sdk.receive_result; }
    if (sdk.incoming.empty()) { return ESP_ERR_TIMEOUT; }
    *message = sdk.incoming.front(); sdk.incoming.pop_front();
    return ESP_OK;
}
esp_err_t twai_read_alerts(uint32_t* alerts, TickType_t wait) {
    sdk.alert_wait = wait; *alerts = sdk.alerts; sdk.alerts = 0;
    return sdk.alert_result;
}
esp_err_t twai_get_status_info(twai_status_info_t* info) {
    *info = sdk.counters;
    info->state = sdk.state; return sdk.status_result;
}
