// Host tests for the gateway's platform-free decision logic: LED pattern (SPEC 5.5), WiFi
// fallback policy (5.9), rollback validation (5.11) and the raw-sentence ring buffer (5.4/6.1).
// Run with `pio test -e native`.

#include <unity.h>
#include <string>

#include "LedPattern.h"
#include "NetPolicy.h"
#include "RollbackPolicy.h"
#include "RawRing.h"
#include "BusHealth.h"

void setUp() {}
void tearDown() {}

//*****************************************************************************
// LED

static LedInputs healthy(uint32_t now) {
  LedInputs in;
  in.now = now;
  in.lastAisMs = now;
  in.booting = false;
  in.canRunning = true;
  in.hotspotOn = false;
  in.otaActive = false;
  return in;
}

static const uint32_t kMin = 60000;

// The LED module latches `booting` for the first LED_BOOT_MS.
void test_led_boot_blue() {
  LedInputs in = healthy(100);
  in.lastAisMs = 0;
  in.booting = true;
  TEST_ASSERT_EQUAL(LedColor::Blue, ledPattern(in));
  in.booting = false;
  TEST_ASSERT_NOT_EQUAL(LedColor::Blue, ledPattern(in));
}

void test_led_green_flash_after_sentence() {
  LedInputs in = healthy(10000);
  in.lastAisMs = 10000 - 20;
  TEST_ASSERT_EQUAL(LedColor::Green, ledPattern(in));
  in.lastAisMs = 10000 - LED_AIS_FLASH_MS;
  TEST_ASSERT_EQUAL(LedColor::Off, ledPattern(in));
}

void test_led_healthy_idle_is_off() {
  LedInputs in = healthy(10000);
  in.lastAisMs = 10000 - 2000;
  TEST_ASSERT_EQUAL(LedColor::Off, ledPattern(in));
}

void test_led_can_fault_blinks_red_1hz_over_everything_but_ota() {
  LedInputs in = healthy(10000);
  in.canRunning = false;
  in.lastAisMs = 10000 - 10;  // AIS flash would show otherwise
  in.now = 10000; TEST_ASSERT_EQUAL(LedColor::Red, ledPattern(in));
  in.now = 10600; in.lastAisMs = 10590; TEST_ASSERT_EQUAL(LedColor::Off, ledPattern(in));
  in.otaActive = true; TEST_ASSERT_EQUAL(LedColor::White, ledPattern(in));
}

void test_led_no_ais_amber_blip_every_5s() {
  LedInputs in = healthy(0);
  in.lastAisMs = 1000;
  uint32_t cycleStart = 310000;  // a blip-cycle start, 5 min 9 s after the last sentence
  in.now = cycleStart + 10; TEST_ASSERT_EQUAL(LedColor::Amber, ledPattern(in));
  in.now = cycleStart + LED_BLIP_MS + 10; TEST_ASSERT_EQUAL(LedColor::Off, ledPattern(in));
}

void test_led_no_ais_since_boot_counts_after_5_minutes() {
  LedInputs in = healthy(0);
  in.lastAisMs = 0;  // no sentence yet: measured from boot
  in.now = 4 * kMin;  // within 5 min of boot: not yet "no AIS"
  in.now -= in.now % 5000;
  TEST_ASSERT_EQUAL(LedColor::Off, ledPattern(in));
  in.now = 6 * kMin;
  in.now -= in.now % 5000;
  TEST_ASSERT_EQUAL(LedColor::Amber, ledPattern(in));
}

void test_led_hotspot_purple_blip_offset_from_amber() {
  LedInputs in = healthy(0);
  in.hotspotOn = true;
  in.lastAisMs = 0;
  in.now = 10 * kMin;  // no AIS too
  in.now -= in.now % 5000;
  TEST_ASSERT_EQUAL(LedColor::Amber, ledPattern(in));
  in.now += 2500;
  TEST_ASSERT_EQUAL(LedColor::Purple, ledPattern(in));
  in.now += LED_BLIP_MS + 10;
  TEST_ASSERT_EQUAL(LedColor::Off, ledPattern(in));
}

void test_led_ais_timing_survives_millis_wraparound() {
  LedInputs in = healthy(0);
  in.lastAisMs = 0xFFFFFFF0;
  in.now = 0x10;  // 32 ms later, across the wrap
  TEST_ASSERT_EQUAL(LedColor::Green, ledPattern(in));
}

//*****************************************************************************
// WiFi fallback hotspot

void test_net_tries_boat_network_at_boot() {
  NetPolicy p;
  NetActions a = p.step(0, false, 0);
  TEST_ASSERT_TRUE(a.stationEnabled);
  TEST_ASSERT_FALSE(a.startHotspot);
}

void test_net_hotspot_after_5_minutes_without_network() {
  NetPolicy p;
  p.step(0, false, 0);
  TEST_ASSERT_FALSE(p.step(5 * kMin - 1, false, 0).startHotspot);
  NetActions a = p.step(5 * kMin, false, 0);
  TEST_ASSERT_TRUE(a.startHotspot);
  TEST_ASSERT_TRUE(p.hotspotOn());
  TEST_ASSERT_FALSE(p.step(5 * kMin + 1, false, 0).startHotspot);  // only once
}

void test_net_hotspot_5_minutes_after_losing_network() {
  NetPolicy p;
  p.step(0, true, 0);
  p.step(10 * kMin, true, 0);
  TEST_ASSERT_FALSE(p.step(10 * kMin + 1, false, 0).startHotspot);
  TEST_ASSERT_TRUE(p.step(15 * kMin, false, 0).startHotspot);
}

void test_net_no_station_search_while_hotspot_client_connected() {
  NetPolicy p;
  p.step(0, false, 0);
  p.step(5 * kMin, false, 0);
  TEST_ASSERT_FALSE(p.step(5 * kMin + 10, false, 1).stationEnabled);
  TEST_ASSERT_TRUE(p.step(5 * kMin + 20, false, 0).stationEnabled);
}

void test_net_hotspot_stops_when_network_returns() {
  NetPolicy p;
  p.step(0, false, 0);
  p.step(5 * kMin, false, 0);
  NetActions a = p.step(6 * kMin, true, 0);
  TEST_ASSERT_TRUE(a.stopHotspot);
  TEST_ASSERT_FALSE(p.hotspotOn());
  TEST_ASSERT_FALSE(p.step(6 * kMin + 1, true, 0).stopHotspot);  // only once
}

void test_net_survives_millis_wraparound() {
  NetPolicy p;
  p.step(0xFFFFFFF0, true, 0);
  TEST_ASSERT_FALSE(p.step(0x10, false, 0).startHotspot);
  TEST_ASSERT_TRUE(p.step(0xFFFFFFF0 + 5 * kMin, false, 0).startHotspot);
}

//*****************************************************************************
// Rollback validation

void test_rollback_nothing_when_image_not_pending() {
  RollbackPolicy p(false);
  TEST_ASSERT_EQUAL(RollbackAction::None, p.step(61000, true));
  TEST_ASSERT_EQUAL(RollbackAction::None, p.step(10 * kMin, false));
}

void test_rollback_marks_valid_when_healthy_after_60s() {
  RollbackPolicy p(true);
  TEST_ASSERT_EQUAL(RollbackAction::None, p.step(59999, true));
  TEST_ASSERT_EQUAL(RollbackAction::MarkValid, p.step(60000, true));
  TEST_ASSERT_EQUAL(RollbackAction::None, p.step(60001, true));  // only once
  TEST_ASSERT_EQUAL(RollbackAction::None, p.step(10 * kMin, false));
}

void test_rollback_waits_while_unhealthy() {
  RollbackPolicy p(true);
  TEST_ASSERT_EQUAL(RollbackAction::None, p.step(2 * kMin, false));
  TEST_ASSERT_EQUAL(RollbackAction::MarkValid, p.step(3 * kMin, true));
}

void test_rollback_restarts_after_5_minutes_unhealthy() {
  RollbackPolicy p(true);
  TEST_ASSERT_EQUAL(RollbackAction::None, p.step(5 * kMin - 1, false));
  TEST_ASSERT_EQUAL(RollbackAction::Restart, p.step(5 * kMin, false));
}

//*****************************************************************************
// Raw sentence ring

void test_ring_keeps_last_n_in_order() {
  RawRing<3, 16> r;
  TEST_ASSERT_EQUAL(0, r.size());
  r.push("a"); r.push("b"); r.push("c"); r.push("d");
  TEST_ASSERT_EQUAL(3, r.size());
  TEST_ASSERT_EQUAL_STRING("b", r.at(0));  // oldest
  TEST_ASSERT_EQUAL_STRING("d", r.at(2));  // newest
}

void test_ring_truncates_long_lines() {
  RawRing<2, 8> r;
  r.push("0123456789");
  TEST_ASSERT_EQUAL_STRING("0123456", r.at(0));
}

//*****************************************************************************
// Bus health (rollback validation input)

static BusHealthInputs goodBus() {
  BusHealthInputs b;
  b.libraryOpen = true;
  b.source = 32;
  b.canRunning = true;
  b.txErrorCounter = 0;
  b.rxErrorCounter = 0;
  b.framesFromOthers = 25;
  return b;
}

void test_bus_healthy_on_working_bus() {
  TEST_ASSERT_TRUE(busHealthy(goodBus()));
}

// Seen on the desk 2026-10-27: CAN Unit with nothing attached -> TEC 0, REC 135, error passive.
void test_bus_unhealthy_floating_bus() {
  BusHealthInputs b = goodBus();
  b.rxErrorCounter = 135;
  b.framesFromOthers = 0;
  TEST_ASSERT_FALSE(busHealthy(b));
}

void test_bus_unhealthy_without_traffic_from_others() {
  BusHealthInputs b = goodBus();
  b.framesFromOthers = 0;
  TEST_ASSERT_FALSE(busHealthy(b));
}

void test_bus_unhealthy_when_frames_not_acknowledged() {
  BusHealthInputs b = goodBus();
  b.txErrorCounter = 128;
  TEST_ASSERT_FALSE(busHealthy(b));
}

void test_bus_unhealthy_without_address() {
  BusHealthInputs b = goodBus();
  b.source = 254;
  TEST_ASSERT_FALSE(busHealthy(b));
}

void test_bus_unhealthy_when_not_running_or_closed() {
  BusHealthInputs b = goodBus();
  b.canRunning = false;
  TEST_ASSERT_FALSE(busHealthy(b));
  b = goodBus();
  b.libraryOpen = false;
  TEST_ASSERT_FALSE(busHealthy(b));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_led_boot_blue);
  RUN_TEST(test_led_green_flash_after_sentence);
  RUN_TEST(test_led_healthy_idle_is_off);
  RUN_TEST(test_led_can_fault_blinks_red_1hz_over_everything_but_ota);
  RUN_TEST(test_led_no_ais_amber_blip_every_5s);
  RUN_TEST(test_led_no_ais_since_boot_counts_after_5_minutes);
  RUN_TEST(test_led_hotspot_purple_blip_offset_from_amber);
  RUN_TEST(test_led_ais_timing_survives_millis_wraparound);
  RUN_TEST(test_net_tries_boat_network_at_boot);
  RUN_TEST(test_net_hotspot_after_5_minutes_without_network);
  RUN_TEST(test_net_hotspot_5_minutes_after_losing_network);
  RUN_TEST(test_net_no_station_search_while_hotspot_client_connected);
  RUN_TEST(test_net_hotspot_stops_when_network_returns);
  RUN_TEST(test_net_survives_millis_wraparound);
  RUN_TEST(test_rollback_nothing_when_image_not_pending);
  RUN_TEST(test_rollback_marks_valid_when_healthy_after_60s);
  RUN_TEST(test_rollback_waits_while_unhealthy);
  RUN_TEST(test_rollback_restarts_after_5_minutes_unhealthy);
  RUN_TEST(test_ring_keeps_last_n_in_order);
  RUN_TEST(test_ring_truncates_long_lines);
  RUN_TEST(test_bus_healthy_on_working_bus);
  RUN_TEST(test_bus_unhealthy_floating_bus);
  RUN_TEST(test_bus_unhealthy_without_traffic_from_others);
  RUN_TEST(test_bus_unhealthy_when_frames_not_acknowledged);
  RUN_TEST(test_bus_unhealthy_without_address);
  RUN_TEST(test_bus_unhealthy_when_not_running_or_closed);
  return UNITY_END();
}
