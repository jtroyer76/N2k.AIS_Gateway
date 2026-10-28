// Host tests for the phase 2 console's platform-free parts (SPEC 6.1, 6.2): the log ring buffer,
// the bounded JSON writer and the /api/log response. Run with `pio test -e native`.

#include <unity.h>
#include <string.h>
#include <string>

#include "JsonOut.h"
#include "LogJson.h"
#include "LogRing.h"

void setUp() {}
void tearDown() {}

//*****************************************************************************
// LogRing

void test_ring_empty() {
  LogRing<4, 16> r;
  TEST_ASSERT_EQUAL_UINT32(0, r.latest());
  TEST_ASSERT_EQUAL_UINT32(1, r.oldest());
  TEST_ASSERT_NULL(r.find(0));
  TEST_ASSERT_NULL(r.find(1));
}

void test_ring_numbers_lines_from_1_and_keeps_fields() {
  LogRing<4, 16> r;
  r.push(1000, 'I', "one");
  r.push(2000, 'W', "two");
  r.push(3000, 'E', "three");
  TEST_ASSERT_EQUAL_UINT32(1, r.oldest());
  TEST_ASSERT_EQUAL_UINT32(3, r.latest());
  const LogRing<4, 16>::Line *l = r.find(2);
  TEST_ASSERT_NOT_NULL(l);
  TEST_ASSERT_EQUAL_UINT32(2, l->seq);
  TEST_ASSERT_EQUAL_UINT32(2000, l->ms);
  TEST_ASSERT_EQUAL_CHAR('W', l->level);
  TEST_ASSERT_EQUAL_STRING("two", l->text);
  TEST_ASSERT_NULL(r.find(4));
}

void test_ring_drops_oldest_when_full() {
  LogRing<4, 16> r;
  char t[8];
  for (int i = 1; i <= 6; i++) {
    snprintf(t, sizeof(t), "l%d", i);
    r.push(i, 'I', t);
  }
  TEST_ASSERT_EQUAL_UINT32(3, r.oldest());
  TEST_ASSERT_EQUAL_UINT32(6, r.latest());
  TEST_ASSERT_NULL(r.find(2));
  TEST_ASSERT_EQUAL_STRING("l3", r.find(3)->text);
  TEST_ASSERT_EQUAL_STRING("l6", r.find(6)->text);
}

void test_ring_truncates_long_text() {
  LogRing<2, 6> r;
  r.push(0, 'I', "abcdefghij");
  TEST_ASSERT_EQUAL_STRING("abcde", r.find(1)->text);
}

//*****************************************************************************
// JsonOut

void test_json_escapes_strings() {
  char buf[64];
  JsonOut j(buf, sizeof(buf));
  j.str("a\"b\\c\nd\x01");
  TEST_ASSERT_TRUE(j.ok());
  TEST_ASSERT_EQUAL_STRING("\"a\\\"b\\\\c\\nd\\u0001\"", buf);
}

void test_json_numbers_and_raw() {
  char buf[64];
  JsonOut j(buf, sizeof(buf));
  j.raw("{\"a\":");
  j.num((uint32_t)4294967295u);
  j.raw(",\"b\":");
  j.num((int32_t)-51);
  j.raw(",\"c\":");
  j.boolean(true);
  j.raw("}");
  TEST_ASSERT_TRUE(j.ok());
  TEST_ASSERT_EQUAL_STRING("{\"a\":4294967295,\"b\":-51,\"c\":true}", buf);
}

void test_json_overflow_is_flagged_and_stays_terminated() {
  char buf[8];
  JsonOut j(buf, sizeof(buf));
  j.raw("1234567890");
  TEST_ASSERT_FALSE(j.ok());
  TEST_ASSERT_TRUE(strlen(buf) < sizeof(buf));
}

void test_json_rewind_undoes_a_write_that_did_not_fit() {
  char buf[12];
  JsonOut j(buf, sizeof(buf));
  j.raw("[1");
  size_t m = j.mark();
  j.raw(",123456789012");
  TEST_ASSERT_FALSE(j.ok());
  j.rewind(m);
  TEST_ASSERT_TRUE(j.ok());
  j.raw("]");
  TEST_ASSERT_EQUAL_STRING("[1]", buf);
}

void test_json_reserve_keeps_room_for_the_tail() {
  char buf[16];
  JsonOut j(buf, sizeof(buf));
  j.setReserve(4);
  j.raw("01234567890");  // 11 chars + 4 reserve + NUL = 16: fits exactly
  TEST_ASSERT_TRUE(j.ok());
  j.raw("x");           // would eat into the reserve
  TEST_ASSERT_FALSE(j.ok());
}

//*****************************************************************************
// /api/log response

typedef LogRing<4, 32> Ring4;

static std::string logJson(const Ring4 &r, uint32_t since, size_t maxLines, size_t cap = 512) {
  char buf[512];
  JsonOut j(buf, cap);
  writeLogJson(j, r, since, maxLines);
  TEST_ASSERT_TRUE(j.ok());
  return buf;
}

static void fill(Ring4 &r, int n) {
  char t[8];
  for (int i = 1; i <= n; i++) {
    snprintf(t, sizeof(t), "l%d", i);
    r.push(i * 1000, 'I', t);
  }
}

void test_log_json_all_lines_since_0() {
  Ring4 r;
  fill(r, 2);
  TEST_ASSERT_EQUAL_STRING(
      "{\"lines\":[[1,1000,\"I\",\"l1\"],[2,2000,\"I\",\"l2\"]],\"next\":2,\"more\":false,\"lost\":0,\"reset\":false}",
      logJson(r, 0, 50).c_str());
}

void test_log_json_nothing_new() {
  Ring4 r;
  fill(r, 2);
  TEST_ASSERT_EQUAL_STRING("{\"lines\":[],\"next\":2,\"more\":false,\"lost\":0,\"reset\":false}",
                           logJson(r, 2, 50).c_str());
}

void test_log_json_empty_ring() {
  Ring4 r;
  TEST_ASSERT_EQUAL_STRING("{\"lines\":[],\"next\":0,\"more\":false,\"lost\":0,\"reset\":false}",
                           logJson(r, 0, 50).c_str());
}

void test_log_json_max_lines_sets_more() {
  Ring4 r;
  fill(r, 3);
  TEST_ASSERT_EQUAL_STRING("{\"lines\":[[2,2000,\"I\",\"l2\"]],\"next\":2,\"more\":true,\"lost\":0,\"reset\":false}",
                           logJson(r, 1, 1).c_str());
}

void test_log_json_reports_lines_lost_to_the_ring() {
  Ring4 r;
  fill(r, 6);  // keeps 3..6
  TEST_ASSERT_EQUAL_STRING(
      "{\"lines\":[[3,3000,\"I\",\"l3\"]],\"next\":3,\"more\":true,\"lost\":2,\"reset\":false}",
      logJson(r, 0, 1).c_str());
}

void test_log_json_since_ahead_of_latest_means_device_restarted() {
  Ring4 r;
  fill(r, 1);
  TEST_ASSERT_EQUAL_STRING("{\"lines\":[[1,1000,\"I\",\"l1\"]],\"next\":1,\"more\":false,\"lost\":0,\"reset\":true}",
                           logJson(r, 99, 50).c_str());
}

void test_log_json_stops_before_a_line_that_does_not_fit() {
  Ring4 r;
  fill(r, 3);
  // Room for the frame and one line only.
  std::string out = logJson(r, 0, 50, 110);
  TEST_ASSERT_EQUAL_STRING("{\"lines\":[[1,1000,\"I\",\"l1\"]],\"next\":1,\"more\":true,\"lost\":0,\"reset\":false}",
                           out.c_str());
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_ring_empty);
  RUN_TEST(test_ring_numbers_lines_from_1_and_keeps_fields);
  RUN_TEST(test_ring_drops_oldest_when_full);
  RUN_TEST(test_ring_truncates_long_text);
  RUN_TEST(test_json_escapes_strings);
  RUN_TEST(test_json_numbers_and_raw);
  RUN_TEST(test_json_overflow_is_flagged_and_stays_terminated);
  RUN_TEST(test_json_rewind_undoes_a_write_that_did_not_fit);
  RUN_TEST(test_json_reserve_keeps_room_for_the_tail);
  RUN_TEST(test_log_json_all_lines_since_0);
  RUN_TEST(test_log_json_nothing_new);
  RUN_TEST(test_log_json_empty_ring);
  RUN_TEST(test_log_json_max_lines_sets_more);
  RUN_TEST(test_log_json_reports_lines_lost_to_the_ring);
  RUN_TEST(test_log_json_since_ahead_of_latest_means_device_restarted);
  RUN_TEST(test_log_json_stops_before_a_line_that_does_not_fit);
  return UNITY_END();
}
