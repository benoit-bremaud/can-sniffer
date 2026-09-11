#pragma once
// Narrow SDK double. Production is also compiled against the real pinned ESP-IDF headers.
#include <cstdint>
using esp_err_t = int;
using gpio_num_t = int;
using TickType_t = uint32_t;
constexpr esp_err_t ESP_OK = 0;
constexpr esp_err_t ESP_ERR_TIMEOUT = 1;
constexpr esp_err_t ESP_FAIL = 2;
constexpr uint32_t TWAI_ALERT_TX_SUCCESS = 0x2;
constexpr uint32_t TWAI_ALERT_TX_FAILED = 0x400;
constexpr uint32_t TWAI_ALERT_BUS_OFF = 0x2000;
enum twai_mode_t { TWAI_MODE_NORMAL, TWAI_MODE_NO_ACK, TWAI_MODE_LISTEN_ONLY };
enum twai_state_t { TWAI_STATE_STOPPED, TWAI_STATE_RUNNING, TWAI_STATE_BUS_OFF, TWAI_STATE_RECOVERING };
struct twai_general_config_t {
    gpio_num_t tx_io;
    gpio_num_t rx_io;
    twai_mode_t mode;
    uint32_t tx_queue_len;
    uint32_t rx_queue_len;
    uint32_t alerts_enabled;
};
struct twai_timing_config_t { uint32_t brp, tseg_1, tseg_2, sjw; bool triple_sampling; };
struct twai_filter_config_t { uint32_t acceptance_code, acceptance_mask; bool single_filter; };
struct twai_status_info_t {
    twai_state_t state;
    uint32_t tx_error_counter, rx_error_counter, tx_failed_count;
    uint32_t rx_missed_count, rx_overrun_count, arb_lost_count, bus_error_count;
};
struct twai_message_t {
    uint32_t identifier = 0;
    uint8_t data_length_code = 0;
    uint8_t data[8] = {};
    unsigned extd = 0, ss = 0, rtr = 0, self = 0;
};
#define TWAI_GENERAL_CONFIG_DEFAULT(tx, rx, mode) {tx, rx, mode, 5, 5, 0}
// brp = 80 MHz / (rate * (1 + tseg_1 + tseg_2)), matching the real configs closely enough
// that a test can recover the rate arithmetically and tell one config from another.
#define TWAI_TIMING_CONFIG_10KBITS() {400, 15, 4, 3, false}
#define TWAI_TIMING_CONFIG_20KBITS() {200, 15, 4, 3, false}
#define TWAI_TIMING_CONFIG_50KBITS() {80, 15, 4, 3, false}
#define TWAI_TIMING_CONFIG_100KBITS() {40, 15, 4, 3, false}
#define TWAI_TIMING_CONFIG_125KBITS() {32, 15, 4, 3, false}
#define TWAI_TIMING_CONFIG_250KBITS() {16, 15, 4, 3, false}
#define TWAI_TIMING_CONFIG_500KBITS() {8, 15, 4, 3, false}
#define TWAI_TIMING_CONFIG_800KBITS() {5, 15, 4, 3, false}
#define TWAI_TIMING_CONFIG_1MBITS() {4, 15, 4, 3, false}
#define TWAI_FILTER_CONFIG_ACCEPT_ALL() {0, 0xFFFFFFFF, true}
esp_err_t twai_driver_install(const twai_general_config_t*, const twai_timing_config_t*, const twai_filter_config_t*);
esp_err_t twai_start();
esp_err_t twai_stop();
esp_err_t twai_driver_uninstall();
esp_err_t twai_transmit(const twai_message_t*, TickType_t);
esp_err_t twai_receive(twai_message_t*, TickType_t);
esp_err_t twai_read_alerts(uint32_t*, TickType_t);
esp_err_t twai_get_status_info(twai_status_info_t*);
