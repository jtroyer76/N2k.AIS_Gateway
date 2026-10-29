// Host tests for the settings rules (SPEC 7.1, 7.2): where a value comes from at boot, what's
// valid, and the made-up device password. Run with `pio test -e native`.

#include <unity.h>
#include <string.h>

#include "SettingsPolicy.h"

void setUp() {}
void tearDown() {}

void test_saved_wins_over_secrets() {
  TEST_ASSERT_EQUAL(SettingSource::Saved, settingSource(true, true));
  TEST_ASSERT_EQUAL(SettingSource::Saved, settingSource(true, false));
}

void test_secrets_used_when_never_saved() {
  TEST_ASSERT_EQUAL(SettingSource::Secrets, settingSource(false, true));
}

void test_default_when_neither() {
  TEST_ASSERT_EQUAL(SettingSource::Default, settingSource(false, false));
}

void test_baud_only_38400_or_4800() {
  TEST_ASSERT_TRUE(validBaud(38400));
  TEST_ASSERT_TRUE(validBaud(4800));
  TEST_ASSERT_FALSE(validBaud(9600));
  TEST_ASSERT_FALSE(validBaud(0));
  TEST_ASSERT_TRUE(validBaud(AIS_BAUD_DEFAULT));
}

void test_ssid_length() {
  TEST_ASSERT_FALSE(validSsid(""));
  TEST_ASSERT_TRUE(validSsid("a"));
  TEST_ASSERT_TRUE(validSsid("12345678901234567890123456789012"));    // 32
  TEST_ASSERT_FALSE(validSsid("123456789012345678901234567890123"));  // 33
}

void test_wifi_pass_blank_or_wpa2_length() {
  TEST_ASSERT_TRUE(validWifiPass(""));
  TEST_ASSERT_FALSE(validWifiPass("1234567"));
  TEST_ASSERT_TRUE(validWifiPass("12345678"));
  char s[65];
  memset(s, 'x', 64);
  s[64] = '\0';
  TEST_ASSERT_FALSE(validWifiPass(s));  // 64
  s[63] = '\0';
  TEST_ASSERT_TRUE(validWifiPass(s));   // 63
}

void test_device_pass_never_blank() {
  TEST_ASSERT_FALSE(validDevicePass(""));
  TEST_ASSERT_FALSE(validDevicePass("1234567"));
  TEST_ASSERT_TRUE(validDevicePass("12345678"));
}

static uint32_t seq;
static uint32_t counter() { return seq++; }

void test_made_up_password_length_and_alphabet() {
  char pw[DEVICE_PASS_LEN + 1];
  seq = 0;
  makePassword(pw, counter);
  TEST_ASSERT_EQUAL(DEVICE_PASS_LEN, strlen(pw));
  TEST_ASSERT_TRUE(validDevicePass(pw));
  for (int i = 0; i < DEVICE_PASS_LEN; i++) TEST_ASSERT_NOT_NULL(strchr(kPasswordChars, pw[i]));
}

void test_alphabet_has_no_look_alikes() {
  for (const char *c = "0O1li"; *c; c++) TEST_ASSERT_NULL(strchr(kPasswordChars, *c));
}

void test_password_uses_whole_alphabet() {
  // A counter walks every index, so every character must appear.
  const size_t n = sizeof(kPasswordChars) - 1;
  bool seen[sizeof(kPasswordChars)] = {};
  char pw[DEVICE_PASS_LEN + 1];
  seq = 0;
  for (size_t k = 0; k < n; k++) {
    makePassword(pw, counter);
    for (int i = 0; i < DEVICE_PASS_LEN; i++) seen[strchr(kPasswordChars, pw[i]) - kPasswordChars] = true;
  }
  for (size_t i = 0; i < n; i++) TEST_ASSERT_TRUE(seen[i]);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_saved_wins_over_secrets);
  RUN_TEST(test_secrets_used_when_never_saved);
  RUN_TEST(test_default_when_neither);
  RUN_TEST(test_baud_only_38400_or_4800);
  RUN_TEST(test_ssid_length);
  RUN_TEST(test_wifi_pass_blank_or_wpa2_length);
  RUN_TEST(test_device_pass_never_blank);
  RUN_TEST(test_made_up_password_length_and_alphabet);
  RUN_TEST(test_alphabet_has_no_look_alikes);
  RUN_TEST(test_password_uses_whole_alphabet);
  return UNITY_END();
}
