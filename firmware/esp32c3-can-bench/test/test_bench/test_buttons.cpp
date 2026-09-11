#include <unity.h>
#include <buttons.h>
#include <fake_sdk.h>
#include "twai_port.h"

using namespace bench;
namespace {
void state(const ButtonTestRunner& runner, ButtonState expected) {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(expected), static_cast<int>(runner.state()));
}
void press(ButtonTestRunner& runner, uint8_t mask, uint32_t now = 0) {
    runner.tick(now, 0); runner.tick(now + 30, 0);
    runner.tick(now + 31, mask); runner.tick(now + 61, mask);
}

void scenario_contracts() {
    Frame f;
    for (auto scenario : {Scenario::Reference125, Scenario::Reference250, Scenario::Varied125}) {
        const auto* def = scenario_definition(scenario);
        TEST_ASSERT_NOT_NULL(def);
        for (uint32_t n = 0; n < def->attempts; ++n) {
            TEST_ASSERT_TRUE(scenario_frame(scenario, n, f));
            if (scenario == Scenario::Varied125) {
                const uint32_t ids[] = {0x1ABCDE, 0x1ABCDF, 0x1ABCE0};
                const uint8_t data[] = {static_cast<uint8_t>(n), static_cast<uint8_t>(n % 3),
                                       0xAA, 0x55, 0, 0xFF, 0x12, 0x34};
                TEST_ASSERT_EQUAL_HEX32(ids[n % 3], f.id);
                TEST_ASSERT_EQUAL_UINT8_ARRAY(data, f.data, 8);
            } else {
                Frame fixed;
                TEST_ASSERT_EQUAL_HEX32(fixed.id, f.id);
                TEST_ASSERT_EQUAL_UINT8_ARRAY(fixed.data, f.data, 8);
            }
        }
        const auto id = f.id;
        TEST_ASSERT_FALSE(scenario_frame(scenario, def->attempts, f));
        TEST_ASSERT_EQUAL_HEX32(id, f.id);
    }
    TEST_ASSERT_FALSE(scenario_frame(Scenario::None, 0, f));
    TEST_ASSERT_NULL(scenario_definition(static_cast<Scenario>(99)));
    TEST_ASSERT_EQUAL_STRING("none", scenario_name(Scenario::None));
    TEST_ASSERT_EQUAL_STRING("reference125", scenario_name(Scenario::Reference125));
    TEST_ASSERT_EQUAL_STRING("reference250", scenario_name(Scenario::Reference250));
    TEST_ASSERT_EQUAL_STRING("varied125", scenario_name(Scenario::Varied125));
    TEST_ASSERT_EQUAL_STRING("unknown", scenario_name(static_cast<Scenario>(99)));
    TEST_ASSERT_EQUAL_STRING("release-wait", button_state_name(ButtonState::ReleaseWait));
    TEST_ASSERT_EQUAL_STRING("ready", button_state_name(ButtonState::Ready));
    TEST_ASSERT_EQUAL_STRING("running", button_state_name(ButtonState::Running));
    TEST_ASSERT_EQUAL_STRING("fault-reset-required", button_state_name(ButtonState::Fault));
    TEST_ASSERT_EQUAL_STRING("unknown", button_state_name(static_cast<ButtonState>(99)));
}

void debounce_boundary_bounce_and_rollover() {
    DebouncedButtons buttons;
    TEST_ASSERT_FALSE(buttons.sample(1, UINT32_MAX - 10));
    TEST_ASSERT_FALSE(buttons.sample(1, 18));
    TEST_ASSERT_TRUE(buttons.sample(1, 19));
    TEST_ASSERT_EQUAL(1, buttons.mask());
    TEST_ASSERT_FALSE(buttons.sample(0, 20));
    TEST_ASSERT_FALSE(buttons.sample(1, 21));
    TEST_ASSERT_FALSE(buttons.sample(1, 50));
    TEST_ASSERT_TRUE(buttons.sample(1, 51));
    buttons.reset(); TEST_ASSERT_FALSE(buttons.sample(1, 99));
}

void held_boot_and_ambiguous_presses_are_silent() {
    TwaiPort port; BenchController c(port); ButtonTestRunner runner(c);
    runner.tick(0, 1); runner.tick(10000, 1);
    state(runner, ButtonState::ReleaseWait); TEST_ASSERT_EQUAL(0, sdk.starts);
    runner.tick(10001, 0); runner.tick(10030, 0); state(runner, ButtonState::ReleaseWait);
    runner.tick(10031, 0); state(runner, ButtonState::Ready);
    runner.tick(10032, 0); state(runner, ButtonState::Ready);
    for (uint8_t mask : {3, 5, 6, 7, 8, 255}) {
        press(runner, mask, 11000 + mask * 100);
        state(runner, ButtonState::ReleaseWait);
        TEST_ASSERT_EQUAL(0, sdk.starts);
    }
}

void all_series_are_bounded_and_repeatable() {
    TwaiPort port; BenchController c(port); ButtonTestRunner runner(c);
    uint32_t now = 0, total = 0;
    for (uint8_t mask : {1, 2, 4, 1}) {
        press(runner, mask, now); state(runner, ButtonState::Running);
        TEST_ASSERT_EQUAL(mask == 2 ? 16 : 32, sdk.timing.brp);
        const uint32_t limit = mask == 4 ? 12 : 10;
        runner.tick(now + 1060, mask); TEST_ASSERT_EQUAL(total, sdk.transmits);
        for (uint32_t n = 0; n < limit; ++n) {
            const uint32_t due = now + 1061 + n * 1000;
            runner.tick(due, 2); // Busy selections must not change bitrate or enqueue tests.
            TEST_ASSERT_EQUAL(++total, sdk.transmits);
            Frame expected; TEST_ASSERT_TRUE(scenario_frame(runner.scenario(), n, expected));
            TEST_ASSERT_EQUAL_HEX32(expected.id, sdk.message.identifier);
            TEST_ASSERT_EQUAL_UINT8_ARRAY(expected.data, sdk.message.data, 8);
            runner.tick(due, 2); TEST_ASSERT_EQUAL(total, sdk.transmits);
            state(runner, ButtonState::Running);
            sdk.alerts = TWAI_ALERT_TX_SUCCESS; runner.tick(due + 1, 2);
        }
        state(runner, ButtonState::ReleaseWait);
        TEST_ASSERT_FALSE(sdk.installed);
        TEST_ASSERT_EQUAL(limit, runner.counts().queued);
        TEST_ASSERT_EQUAL(limit, runner.counts().succeeded);
        now += 30000;
        runner.tick(now - 1, 2); TEST_ASSERT_EQUAL(total, sdk.transmits);
    }
    TEST_ASSERT_EQUAL(total, c.status().succeeded);
}

void stop_releases_and_faults_require_reset() {
    for (unsigned error = 0; error < 8; ++error) {
        sdk = FakeSdk{};
        TwaiPort port; BenchController c(port); ButtonTestRunner runner(c);
        if (error == 0) { sdk.install_result = ESP_FAIL; }
        if (error == 1) { sdk.start_result = ESP_FAIL; }
        press(runner, 1);
        if (error == 2) { sdk.transmit_result = ESP_FAIL; }
        runner.tick(1061, 0);
        if (error == 3) { sdk.alerts = TWAI_ALERT_TX_SUCCESS | TWAI_ALERT_TX_FAILED; }
        if (error == 4) { sdk.state = TWAI_STATE_BUS_OFF; }
        if (error == 5) { sdk.status_result = ESP_FAIL; }
        if (error == 6) { sdk.alerts = TWAI_ALERT_TX_SUCCESS; }
        if (error == 7) { sdk.stop_result = ESP_FAIL; runner.stop(1062); }
        runner.service(error == 6 ? 1311 : 1062);
        state(runner, ButtonState::Fault);
        const int starts = sdk.starts, transmits = sdk.transmits;
        sdk.stop_result = ESP_OK;
        runner.stop(2000); press(runner, 2, 3000);
        state(runner, ButtonState::Fault);
        TEST_ASSERT_EQUAL(starts, sdk.starts); TEST_ASSERT_EQUAL(transmits, sdk.transmits);
    }
}

void cancel_pending_and_bitrate_guards() {
    TwaiPort port; BenchController c(port); ButtonTestRunner runner(c);
    TEST_ASSERT_FALSE(c.configure(static_cast<Bitrate>(42)));
    TEST_ASSERT_FALSE(port.start(static_cast<Bitrate>(42)));
    TEST_ASSERT_EQUAL(0, sdk.installs);
    press(runner, 2); TEST_ASSERT_FALSE(c.configure(Bitrate::K125));
    runner.tick(1061, 0); runner.stop(1062);
    state(runner, ButtonState::ReleaseWait); TEST_ASSERT_EQUAL(1, runner.counts().aborted);
    runner.stop(1063); press(runner, 1, 2000);
    TEST_ASSERT_EQUAL(0, runner.counts().aborted); TEST_ASSERT_EQUAL(32, sdk.timing.brp);
    runner.stop(2062);
    // Misuse of the ownership contract fails closed rather than reconfiguring a running port.
    c.command(Command::Start, 3000); press(runner, 1, 3000);
    state(runner, ButtonState::Fault); TEST_ASSERT_FALSE(sdk.installed);
}

void runner_rollover_and_late_ticks() {
    TwaiPort port; BenchController c(port); ButtonTestRunner runner(c);
    press(runner, 4, UINT32_MAX - 100);
    runner.tick(959, 0); TEST_ASSERT_EQUAL(0, sdk.transmits);
    runner.tick(960, 0); TEST_ASSERT_EQUAL(1, sdk.transmits);
    sdk.alerts = TWAI_ALERT_TX_SUCCESS; runner.service(961);
    runner.tick(9000, 0); TEST_ASSERT_EQUAL(2, sdk.transmits);
    sdk.alerts = TWAI_ALERT_TX_SUCCESS; runner.service(9001);
    runner.tick(9001, 0); TEST_ASSERT_EQUAL(2, sdk.transmits);
    runner.stop(9002);
}
}  // namespace

void run_button_tests() {
    RUN_TEST(scenario_contracts);
    RUN_TEST(debounce_boundary_bounce_and_rollover);
    RUN_TEST(held_boot_and_ambiguous_presses_are_silent);
    RUN_TEST(all_series_are_bounded_and_repeatable);
    RUN_TEST(stop_releases_and_faults_require_reset);
    RUN_TEST(cancel_pending_and_bitrate_guards);
    RUN_TEST(runner_rollover_and_late_ticks);
}
