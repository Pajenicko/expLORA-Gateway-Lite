/**
 * Native unit tests for SensorHealth.
 *
 * SensorHealth tracks per-sensor packet outcomes in a 24-bucket hourly
 * sliding window. The tricky parts are: bucket rotation on the hour
 * boundary, multi-hour silences zeroing intermediate buckets, the
 * consecutive-rejection streak resetting on success, and the reason
 * string truncation. These tests pin all of that.
 *
 * Run:   pio test -e native
 */

#include <unity.h>
#include <cstring>
#include "../../src/Data/SensorHealth.h"

using SensorHealth::Stats;
using SensorHealth::BUCKET_MS;
using SensorHealth::WINDOW_BUCKETS;
using SensorHealth::REASON_BUF_SIZE;

void setUp(void) {}
void tearDown(void) {}

// ---------------------------------------------------------------------------
// recordSuccess / recordRejection — basic bookkeeping
// ---------------------------------------------------------------------------

static void test_initial_state_is_unknown(void)
{
    Stats s;
    TEST_ASSERT_FALSE(s.hasLastOk);
    TEST_ASSERT_FALSE(s.hasLastReject);
    TEST_ASSERT_EQUAL_UINT16(0, s.consecutiveRejections);
    TEST_ASSERT_EQUAL(SensorHealth::Status::Unknown, SensorHealth::status(s, 1000));
}

static void test_first_success_records_timestamp_and_counter(void)
{
    Stats s;
    SensorHealth::recordSuccess(s, 12345UL);
    TEST_ASSERT_TRUE(s.hasLastOk);
    TEST_ASSERT_EQUAL_UINT32(12345UL, s.lastOkAtMillis);
    TEST_ASSERT_EQUAL_UINT32(1, SensorHealth::okLast24h(s));
    TEST_ASSERT_EQUAL_UINT32(0, SensorHealth::rejectedLast24h(s));
}

static void test_first_rejection_records_reason_and_streak(void)
{
    Stats s;
    SensorHealth::recordRejection(s, 7000UL, "invalid pressure: 1012");
    TEST_ASSERT_TRUE(s.hasLastReject);
    TEST_ASSERT_EQUAL_UINT16(1, s.consecutiveRejections);
    TEST_ASSERT_EQUAL_STRING("invalid pressure: 1012", s.lastRejectReason);
    TEST_ASSERT_EQUAL_UINT32(1, SensorHealth::rejectedLast24h(s));
}

static void test_success_resets_consecutive_streak(void)
{
    Stats s;
    SensorHealth::recordRejection(s, 100, "a");
    SensorHealth::recordRejection(s, 200, "b");
    SensorHealth::recordRejection(s, 300, "c");
    TEST_ASSERT_EQUAL_UINT16(3, s.consecutiveRejections);

    SensorHealth::recordSuccess(s, 400);
    TEST_ASSERT_EQUAL_UINT16(0, s.consecutiveRejections);
}

static void test_status_errors_after_threshold_streak(void)
{
    Stats s;
    // ERROR_STREAK_THRESHOLD is 5 — at 5 rejections we cross over.
    for (int i = 0; i < 5; i++)
    {
        SensorHealth::recordRejection(s, 1000UL + i, "bad");
    }
    TEST_ASSERT_EQUAL(SensorHealth::Status::Errors, SensorHealth::status(s, 1500));
    // One success drops us back to Healthy
    SensorHealth::recordSuccess(s, 2000);
    TEST_ASSERT_EQUAL(SensorHealth::Status::Healthy, SensorHealth::status(s, 2500));
}

static void test_status_stale_when_no_success_in_6h(void)
{
    Stats s;
    SensorHealth::recordSuccess(s, 1000UL);
    // 6h + 1 ms later: should be Stale
    unsigned long staleNow = 1000UL + 6UL * BUCKET_MS + 1;
    TEST_ASSERT_EQUAL(SensorHealth::Status::Stale, SensorHealth::status(s, staleNow));
    // Just under 6h: still Healthy
    unsigned long fineNow = 1000UL + 5UL * BUCKET_MS;
    TEST_ASSERT_EQUAL(SensorHealth::Status::Healthy, SensorHealth::status(s, fineNow));
}

// ---------------------------------------------------------------------------
// 24h sliding window rotation
// ---------------------------------------------------------------------------

static void test_window_advances_on_hour_boundary(void)
{
    Stats s;
    SensorHealth::recordSuccess(s, 1000UL);
    TEST_ASSERT_EQUAL_UINT8(0, s.headIdx);

    // One hour + 1 ms later → headIdx should advance to 1
    SensorHealth::recordSuccess(s, 1000UL + BUCKET_MS + 1);
    TEST_ASSERT_EQUAL_UINT8(1, s.headIdx);

    // okLast24h still 2 (one in bucket 0, one in bucket 1)
    TEST_ASSERT_EQUAL_UINT32(2, SensorHealth::okLast24h(s));
}

static void test_window_does_not_advance_within_hour(void)
{
    Stats s;
    SensorHealth::recordSuccess(s, 1000UL);
    SensorHealth::recordSuccess(s, 1000UL + BUCKET_MS - 1);
    TEST_ASSERT_EQUAL_UINT8(0, s.headIdx);
    TEST_ASSERT_EQUAL_UINT32(2, SensorHealth::okLast24h(s));
}

static void test_multi_hour_silence_zeroes_skipped_buckets(void)
{
    Stats s;
    // Fill bucket 0 with 5 successes
    for (int i = 0; i < 5; i++) SensorHealth::recordSuccess(s, 1000UL + i);
    TEST_ASSERT_EQUAL_UINT32(5, SensorHealth::okLast24h(s));

    // Now 3 h of silence, then one new success
    SensorHealth::recordSuccess(s, 1000UL + 3UL * BUCKET_MS + 10);
    // Bucket 0 (5), buckets 1 & 2 (zeroed during skip), bucket 3 (1 new) → 6
    TEST_ASSERT_EQUAL_UINT8(3, s.headIdx);
    TEST_ASSERT_EQUAL_UINT32(6, SensorHealth::okLast24h(s));
    TEST_ASSERT_EQUAL_UINT16(0, s.okBuckets[1]);
    TEST_ASSERT_EQUAL_UINT16(0, s.okBuckets[2]);
}

static void test_full_24h_silence_wipes_window_completely(void)
{
    Stats s;
    // Pre-populate every bucket with 1 by recording across 24 hours.
    for (size_t i = 0; i < WINDOW_BUCKETS; i++)
    {
        SensorHealth::recordSuccess(s, 1000UL + i * BUCKET_MS);
    }
    TEST_ASSERT_EQUAL_UINT32(WINDOW_BUCKETS, SensorHealth::okLast24h(s));

    // 24 hours + 1 ms of silence, then one new event. All old buckets must
    // be zeroed; only the new event remains.
    unsigned long now = 1000UL + 24UL * BUCKET_MS + 24UL * BUCKET_MS + 1;
    SensorHealth::recordSuccess(s, now);
    TEST_ASSERT_EQUAL_UINT32(1, SensorHealth::okLast24h(s));
}

static void test_window_wraps_around_correctly(void)
{
    Stats s;
    // 25 hours of one success per hour. The first one (in bucket 0) should
    // be evicted when the window wraps; we should end with exactly 24
    // successes.
    for (size_t i = 0; i < 25; i++)
    {
        SensorHealth::recordSuccess(s, 1000UL + i * BUCKET_MS);
    }
    TEST_ASSERT_EQUAL_UINT32(24, SensorHealth::okLast24h(s));
}

static void test_ok_and_reject_buckets_are_independent(void)
{
    Stats s;
    SensorHealth::recordSuccess(s, 1000UL);
    SensorHealth::recordRejection(s, 1100UL, "a");
    SensorHealth::recordRejection(s, 1200UL, "b");
    TEST_ASSERT_EQUAL_UINT32(1, SensorHealth::okLast24h(s));
    TEST_ASSERT_EQUAL_UINT32(2, SensorHealth::rejectedLast24h(s));
}

// ---------------------------------------------------------------------------
// Edge cases
// ---------------------------------------------------------------------------

static void test_long_reason_is_truncated_with_null_terminator(void)
{
    Stats s;
    char veryLong[200];
    std::memset(veryLong, 'X', sizeof(veryLong));
    veryLong[sizeof(veryLong) - 1] = '\0';

    SensorHealth::recordRejection(s, 1000UL, veryLong);
    // Must be truncated to REASON_BUF_SIZE - 1 chars + terminator.
    TEST_ASSERT_EQUAL_size_t(REASON_BUF_SIZE - 1, std::strlen(s.lastRejectReason));
    TEST_ASSERT_EQUAL_CHAR('\0', s.lastRejectReason[REASON_BUF_SIZE - 1]);
}

static void test_null_reason_keeps_previous_reason(void)
{
    Stats s;
    SensorHealth::recordRejection(s, 1000UL, "first reason");
    SensorHealth::recordRejection(s, 1100UL, nullptr);
    TEST_ASSERT_EQUAL_STRING("first reason", s.lastRejectReason);
    TEST_ASSERT_EQUAL_UINT16(2, s.consecutiveRejections);
}

static void test_advance_only_call_rotates_without_event(void)
{
    Stats s;
    SensorHealth::recordSuccess(s, 1000UL);
    // Simulate tickSensorHealth being called from main loop after silence
    SensorHealth::advanceBucketsIfNeeded(s, 1000UL + 2UL * BUCKET_MS + 1);
    TEST_ASSERT_EQUAL_UINT8(2, s.headIdx);
    // Earlier success still counts
    TEST_ASSERT_EQUAL_UINT32(1, SensorHealth::okLast24h(s));
}

// ---------------------------------------------------------------------------

int main(int, char **)
{
    UNITY_BEGIN();

    RUN_TEST(test_initial_state_is_unknown);
    RUN_TEST(test_first_success_records_timestamp_and_counter);
    RUN_TEST(test_first_rejection_records_reason_and_streak);
    RUN_TEST(test_success_resets_consecutive_streak);
    RUN_TEST(test_status_errors_after_threshold_streak);
    RUN_TEST(test_status_stale_when_no_success_in_6h);

    RUN_TEST(test_window_advances_on_hour_boundary);
    RUN_TEST(test_window_does_not_advance_within_hour);
    RUN_TEST(test_multi_hour_silence_zeroes_skipped_buckets);
    RUN_TEST(test_full_24h_silence_wipes_window_completely);
    RUN_TEST(test_window_wraps_around_correctly);
    RUN_TEST(test_ok_and_reject_buckets_are_independent);

    RUN_TEST(test_long_reason_is_truncated_with_null_terminator);
    RUN_TEST(test_null_reason_keeps_previous_reason);
    RUN_TEST(test_advance_only_call_rotates_without_event);

    return UNITY_END();
}
