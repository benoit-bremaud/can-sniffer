#include <unity.h>
#include <fake_sdk.h>
#include "bench_receiver.h"

namespace {
twai_message_t known_frame() {
    twai_message_t frame = {};
    frame.identifier = 0x001ABCDE; frame.extd = 1; frame.data_length_code = 8;
    for (unsigned i = 0; i < 8; ++i) { frame.data[i] = i + 1; }
    return frame;
}

void receiver_lifecycle_and_retention() {
    // Arrange: the real hardware-boundary receiver, with only SDK calls doubled.
    TwaiPort port; BenchReceiver receiver(port);
    // Act/assert: boot is silent, one frame per call, repeated start preserves results.
    receiver.stop(); TEST_ASSERT_FALSE(receiver.service());
    TEST_ASSERT_EQUAL(0, sdk.installs);
    receiver.start(); receiver.start();
    TEST_ASSERT_EQUAL(1, sdk.starts);
    TEST_ASSERT_EQUAL(kExpectedTwaiMode, sdk.general.mode);
    TEST_ASSERT_EQUAL(32, sdk.timing.brp);
    TEST_ASSERT_FALSE(receiver.service());
    sdk.incoming.push_back(known_frame()); sdk.incoming.push_back(known_frame());
    TEST_ASSERT_TRUE(receiver.service());
    TEST_ASSERT_EQUAL(1, receiver.count()); TEST_ASSERT_EQUAL(1, sdk.incoming.size());
    TEST_ASSERT_EQUAL(0, sdk.receive_wait);
    TEST_ASSERT_TRUE(receiver.has_frame());
    TEST_ASSERT_EQUAL_HEX32(0x001ABCDE, receiver.last_frame().identifier);
    const auto expected = known_frame();
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected.data, receiver.last_frame().data, 8);
    receiver.start(); TEST_ASSERT_EQUAL(1, receiver.count());
    TEST_ASSERT_TRUE(receiver.service()); TEST_ASSERT_EQUAL(2, receiver.count());
    receiver.stop(); TEST_ASSERT_FALSE(receiver.service());
    TEST_ASSERT_EQUAL(2, receiver.count()); TEST_ASSERT_TRUE(receiver.has_frame());
    receiver.start(); TEST_ASSERT_EQUAL(0, receiver.count());
    TEST_ASSERT_FALSE(receiver.has_frame()); receiver.stop();
    TEST_ASSERT_EQUAL(0, sdk.transmits);
}

void receiver_adapter_validates_frames() {
    TwaiPort port; twai_message_t output = known_frame();
    TEST_ASSERT_EQUAL(TwaiPort::ReceiveResult::Error, port.receive(output));
    TEST_ASSERT_TRUE(port.start());
    for (bool extended : {false, true}) {
        for (bool rtr : {false, true}) {
            for (unsigned dlc : {0u, 8u}) {
                auto input = known_frame(); input.extd = extended; input.rtr = rtr;
                input.identifier = extended ? 0x1FFFFFFF : 0x7FF; input.data_length_code = dlc;
                sdk.incoming.push_back(input);
                TEST_ASSERT_EQUAL(TwaiPort::ReceiveResult::Frame, port.receive(output));
                TEST_ASSERT_EQUAL(dlc, output.data_length_code);
                TEST_ASSERT_EQUAL(rtr, output.rtr);
            }
        }
    }
    output = known_frame();
    for (unsigned invalid : {0u, 1u, 2u}) {
        auto input = known_frame();
        if (invalid == 0) { input.data_length_code = 9; }
        if (invalid == 1) { input.extd = 0; input.identifier = 0x800; }
        if (invalid == 2) { input.identifier = 0x20000000; }
        sdk.incoming.push_back(input);
        TEST_ASSERT_EQUAL(TwaiPort::ReceiveResult::Error, port.receive(output));
        TEST_ASSERT_EQUAL_HEX32(0x001ABCDE, output.identifier);
    }
    sdk.state = TWAI_STATE_BUS_OFF; port.poll();
    TEST_ASSERT_EQUAL(TwaiPort::ReceiveResult::Error, port.receive(output));
    port.stop(); TEST_ASSERT_EQUAL(0, sdk.transmits);
}

void receiver_faults_stop_and_never_send() {
    for (int fault = 0; fault < 7; ++fault) {
        sdk = FakeSdk{};
        TwaiPort port; BenchReceiver receiver(port);
        if (fault == 0) { sdk.install_result = ESP_FAIL; }
        if (fault == 1) { sdk.start_result = ESP_FAIL; }
        receiver.start();
        if (fault == 2) { sdk.receive_result = ESP_FAIL; }
        if (fault == 3) { sdk.status_result = ESP_FAIL; }
        if (fault == 4) { sdk.state = TWAI_STATE_BUS_OFF; }
        if (fault == 5) { sdk.alerts = TWAI_ALERT_TX_FAILED; }
        if (fault == 6) { auto frame = known_frame(); frame.data_length_code = 255; sdk.incoming.push_back(frame); }
        TEST_ASSERT_FALSE(receiver.service());
        TEST_ASSERT_EQUAL(bench::State::Fault, receiver.state());
        TEST_ASSERT_FALSE(sdk.installed);
        const auto starts = sdk.starts; receiver.start(); TEST_ASSERT_EQUAL(starts, sdk.starts);
        receiver.stop();
        TEST_ASSERT_EQUAL(fault == 4 ? bench::State::Fault : bench::State::Stopped, receiver.state());
        TEST_ASSERT_EQUAL(0, receiver.count()); TEST_ASSERT_FALSE(receiver.has_frame());
        TEST_ASSERT_EQUAL(0, sdk.transmits);
    }
}

void receiver_cleanup_failure_and_retry() {
    TwaiPort port; BenchReceiver receiver(port); receiver.start();
    sdk.stop_result = ESP_FAIL; receiver.stop();
    TEST_ASSERT_EQUAL(bench::State::Fault, receiver.state());
    TEST_ASSERT_EQUAL(bench::Fault::Cleanup, receiver.fault());
    sdk.stop_result = ESP_OK; receiver.stop();
    receiver.start(); sdk.receive_result = ESP_FAIL; sdk.uninstall_result = ESP_FAIL;
    receiver.service(); TEST_ASSERT_EQUAL(bench::Fault::Cleanup, receiver.fault());
    sdk.uninstall_result = ESP_OK; receiver.stop();
    TEST_ASSERT_EQUAL(bench::State::Stopped, receiver.state());
    TEST_ASSERT_EQUAL(0, sdk.transmits);
}
}  // namespace

void run_receiver_tests() {
    RUN_TEST(receiver_lifecycle_and_retention);
    RUN_TEST(receiver_adapter_validates_frames);
    RUN_TEST(receiver_faults_stop_and_never_send);
    RUN_TEST(receiver_cleanup_failure_and_retry);
}
