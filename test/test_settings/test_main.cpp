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

void test_password_matches() {
  TEST_ASSERT_TRUE(passwordMatches("abcdefgh2345", "abcdefgh2345"));
  TEST_ASSERT_FALSE(passwordMatches("abcdefgh2346", "abcdefgh2345"));
  TEST_ASSERT_FALSE(passwordMatches("abcdefgh234", "abcdefgh2345"));    // prefix
  TEST_ASSERT_FALSE(passwordMatches("abcdefgh23456", "abcdefgh2345"));  // longer
  TEST_ASSERT_FALSE(passwordMatches("", "abcdefgh2345"));
  TEST_ASSERT_FALSE(passwordMatches("ABCDEFGH2345", "abcdefgh2345"));   // case matters
}

//*****************************************************************************
// Saving from the setup page (7.5)

static GatewaySettings saved() {
  GatewaySettings s = {};
  strcpy(s.ssid, "Harbour");
  strcpy(s.wifiPass, "harbour-pass");
  strcpy(s.devicePass, "abcdefgh2345");
  s.aisBaud = 38400;
  return s;
}

static SettingsForm form(const char *ssid, const char *wifiPass, uint32_t baud, const char *newPass,
                         const char *password) {
  SettingsForm f = {ssid, wifiPass, baud, newPass, password};
  return f;
}

void test_form_from_boat_network_needs_password() {
  GatewaySettings out = saved();
  TEST_ASSERT_EQUAL_STRING("wrong device password",
                           applySettingsForm(saved(), form("Harbour", "", 4800, "", "nope"), false, out));
  TEST_ASSERT_EQUAL_STRING("wrong device password",
                           applySettingsForm(saved(), form("Harbour", "", 4800, "", ""), false, out));
  TEST_ASSERT_NULL(applySettingsForm(saved(), form("Harbour", "", 4800, "", "abcdefgh2345"), false, out));
  TEST_ASSERT_EQUAL(4800, out.aisBaud);
}

void test_form_through_hotspot_needs_no_password() {
  GatewaySettings out = saved();
  TEST_ASSERT_NULL(applySettingsForm(saved(), form("Marina", "marina-pass", 38400, "", ""), true, out));
  TEST_ASSERT_EQUAL_STRING("Marina", out.ssid);
  TEST_ASSERT_EQUAL_STRING("marina-pass", out.wifiPass);
}

void test_form_blank_wifi_pass_keeps_saved_for_same_network() {
  GatewaySettings out = saved();
  TEST_ASSERT_NULL(applySettingsForm(saved(), form("Harbour", "", 38400, "", ""), true, out));
  TEST_ASSERT_EQUAL_STRING("harbour-pass", out.wifiPass);
}

void test_form_blank_wifi_pass_on_new_network_is_open() {
  GatewaySettings out = saved();
  TEST_ASSERT_NULL(applySettingsForm(saved(), form("Open Marina", "", 38400, "", ""), true, out));
  TEST_ASSERT_EQUAL_STRING("", out.wifiPass);
}

void test_form_network_name_is_case_sensitive() {
  // "harbour" is a different network: the saved password isn't carried over.
  GatewaySettings out = saved();
  TEST_ASSERT_NULL(applySettingsForm(saved(), form("harbour", "", 38400, "", ""), true, out));
  TEST_ASSERT_EQUAL_STRING("", out.wifiPass);
}

void test_form_new_device_password() {
  GatewaySettings out = saved();
  TEST_ASSERT_NULL(applySettingsForm(saved(), form("Harbour", "", 38400, "new-password", "abcdefgh2345"), false, out));
  TEST_ASSERT_EQUAL_STRING("new-password", out.devicePass);
  out = saved();
  TEST_ASSERT_NULL(applySettingsForm(saved(), form("Harbour", "", 38400, "", "abcdefgh2345"), false, out));
  TEST_ASSERT_EQUAL_STRING("abcdefgh2345", out.devicePass);  // blank keeps it
}

void test_form_rejects_bad_values_and_leaves_out_alone() {
  GatewaySettings out = saved();
  TEST_ASSERT_NOT_NULL(applySettingsForm(saved(), form("", "", 38400, "", ""), true, out));
  TEST_ASSERT_NOT_NULL(applySettingsForm(saved(), form("Marina", "short", 38400, "", ""), true, out));
  TEST_ASSERT_NOT_NULL(applySettingsForm(saved(), form("Harbour", "", 9600, "", ""), true, out));
  TEST_ASSERT_NOT_NULL(applySettingsForm(saved(), form("Harbour", "", 38400, "short", ""), true, out));
  TEST_ASSERT_EQUAL_STRING("Harbour", out.ssid);
  TEST_ASSERT_EQUAL_STRING("harbour-pass", out.wifiPass);
  TEST_ASSERT_EQUAL_STRING("abcdefgh2345", out.devicePass);
  TEST_ASSERT_EQUAL(38400, out.aisBaud);
}

void test_form_first_setup_from_nothing() {
  GatewaySettings blank = {};
  strcpy(blank.devicePass, "abcdefgh2345");  // made up on first boot
  blank.aisBaud = AIS_BAUD_DEFAULT;
  GatewaySettings out = blank;
  TEST_ASSERT_NULL(applySettingsForm(blank, form("Harbour", "harbour-pass", 38400, "", ""), true, out));
  TEST_ASSERT_EQUAL_STRING("Harbour", out.ssid);
  TEST_ASSERT_EQUAL_STRING("abcdefgh2345", out.devicePass);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_form_from_boat_network_needs_password);
  RUN_TEST(test_form_through_hotspot_needs_no_password);
  RUN_TEST(test_form_blank_wifi_pass_keeps_saved_for_same_network);
  RUN_TEST(test_form_blank_wifi_pass_on_new_network_is_open);
  RUN_TEST(test_form_network_name_is_case_sensitive);
  RUN_TEST(test_form_new_device_password);
  RUN_TEST(test_form_rejects_bad_values_and_leaves_out_alone);
  RUN_TEST(test_form_first_setup_from_nothing);
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
  RUN_TEST(test_password_matches);
  return UNITY_END();
}
