#pragma once
#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
constexpr int HIGH = 1;
constexpr int LOW = 0;
constexpr int INPUT_PULLUP = 2;
constexpr int OUTPUT = 1;
struct FakeSerial {
    bool connected = false;
    bool lose_on_read = false;
    bool negative_read = false;
    bool partial_write = false;
    int writable = 1024;
    uint32_t baud = 0, timeout = 0;
    std::size_t rx_size = 0, tx_size = 0;
    std::deque<unsigned char> input;
    std::string output;
    explicit operator bool() const { return connected; }
    std::size_t setRxBufferSize(std::size_t n) { rx_size = n; return n; }
    std::size_t setTxBufferSize(std::size_t n) { tx_size = n; return n; }
    void setTxTimeoutMs(uint32_t n) { timeout = n; }
    void begin(uint32_t n) { baud = n; }
    int availableForWrite() const { return writable; }
    int available() const { return static_cast<int>(input.size()); }
    int read() {
        if (lose_on_read) { connected = false; }
        if (negative_read || input.empty()) { return -1; }
        const auto c = input.front(); input.pop_front(); return c;
    }
    std::size_t write(const uint8_t* data, std::size_t n) {
        if (partial_write && n) { --n; }
        output.append(reinterpret_cast<const char*>(data), n);
        return n;
    }
    void send(const std::string& s) { input.insert(input.end(), s.begin(), s.end()); }
};
extern FakeSerial Serial;
uint32_t millis();
void delay(uint32_t);
void digitalWrite(int, int);
void pinMode(int, int);
int digitalRead(int);
