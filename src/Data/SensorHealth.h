/**
 * expLORA Gateway Lite — SensorHealth
 *
 * Pure, header-only per-sensor health tracking. Records when each
 * sensor last delivered a valid packet, when it was last rejected and
 * why, the current consecutive-rejection streak, and an hourly 24-bucket
 * sliding window of successes / failures.
 *
 * Designed to be exercised by host-side unit tests under the `native`
 * PlatformIO env — no Arduino types, no logger, no global state. All
 * timestamps are passed in by the caller (`nowMillis`), which means the
 * tests can control "time" deterministically.
 *
 * Wraparound: the project's main loop uses millis() which rolls over
 * every ~49.7 days. Time-delta arithmetic uses the standard signed-cast
 * idiom `(long)(now - start) >= INTERVAL` so it survives the wrap.
 *
 * Memory: ~120 B per sensor with the 24×4-byte bucket arrays. For
 * MAX_SENSORS = 20 that's ~2.4 KB total — negligible against the
 * ESP32-S3's 320 KB SRAM.
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>

namespace SensorHealth
{

constexpr size_t        WINDOW_BUCKETS    = 24;        // hourly buckets → 24 h window
constexpr unsigned long BUCKET_MS         = 3600000UL; // 1 hour in millis
constexpr size_t        REASON_BUF_SIZE   = 64;        // null-terminated reason string

struct Stats
{
    // Last-event timestamps (millis since boot).
    unsigned long lastOkAtMillis     = 0;
    unsigned long lastRejectAtMillis = 0;
    bool          hasLastOk          = false;
    bool          hasLastReject      = false;

    // Human-readable last rejection reason (null-terminated, fixed buffer
    // to avoid heap fragmentation on long-running devices).
    char lastRejectReason[REASON_BUF_SIZE] = {0};

    // Streak counter that resets to zero on every success. A high value
    // is the smoking gun for a broken sensor.
    uint16_t consecutiveRejections = 0;

    // 24-bucket hourly sliding window. headIdx points at the bucket
    // currently accumulating new events.
    uint16_t      okBuckets[WINDOW_BUCKETS]     = {};
    uint16_t      rejectBuckets[WINDOW_BUCKETS] = {};
    uint8_t       headIdx                        = 0;
    unsigned long currentBucketStartedAtMillis   = 0;
    bool          windowInitialised              = false;
};

// Advance the head bucket forward if at least one BUCKET_MS has elapsed
// since the current bucket was opened. Zero out any buckets we skip past
// (this handles long silences correctly: 5 h with no events advances 5
// buckets and zeros each one, so they don't contain stale counts when
// the window wraps around).
inline void advanceBucketsIfNeeded(Stats &s, unsigned long nowMillis)
{
    if (!s.windowInitialised)
    {
        s.headIdx                      = 0;
        s.currentBucketStartedAtMillis = nowMillis;
        s.windowInitialised            = true;
        return;
    }

    // Use signed delta to be safe across millis() wrap.
    long elapsed = static_cast<long>(nowMillis - s.currentBucketStartedAtMillis);
    if (elapsed < static_cast<long>(BUCKET_MS))
    {
        return;
    }

    // How many full hours have passed since the current bucket was opened?
    unsigned long hours = static_cast<unsigned long>(elapsed) / BUCKET_MS;
    if (hours > WINDOW_BUCKETS)
    {
        hours = WINDOW_BUCKETS; // a full silence longer than 24 h wipes everything
    }

    for (unsigned long i = 0; i < hours; i++)
    {
        s.headIdx = static_cast<uint8_t>((s.headIdx + 1) % WINDOW_BUCKETS);
        s.okBuckets[s.headIdx]     = 0;
        s.rejectBuckets[s.headIdx] = 0;
    }
    s.currentBucketStartedAtMillis += hours * BUCKET_MS;
}

// Record a successful packet at `nowMillis`.
inline void recordSuccess(Stats &s, unsigned long nowMillis)
{
    advanceBucketsIfNeeded(s, nowMillis);

    s.lastOkAtMillis        = nowMillis;
    s.hasLastOk             = true;
    s.consecutiveRejections = 0;

    if (s.okBuckets[s.headIdx] < UINT16_MAX)
    {
        s.okBuckets[s.headIdx]++;
    }
}

// Record a rejected packet at `nowMillis`. `reason` is copied into a
// fixed-size buffer (truncated if longer than REASON_BUF_SIZE - 1).
// `reason` may be nullptr — in that case the previous reason is kept.
inline void recordRejection(Stats &s, unsigned long nowMillis, const char *reason)
{
    advanceBucketsIfNeeded(s, nowMillis);

    s.lastRejectAtMillis = nowMillis;
    s.hasLastReject      = true;
    if (s.consecutiveRejections < UINT16_MAX)
    {
        s.consecutiveRejections++;
    }

    if (reason != nullptr)
    {
        std::strncpy(s.lastRejectReason, reason, REASON_BUF_SIZE - 1);
        s.lastRejectReason[REASON_BUF_SIZE - 1] = '\0';
    }

    if (s.rejectBuckets[s.headIdx] < UINT16_MAX)
    {
        s.rejectBuckets[s.headIdx]++;
    }
}

// Sum of all OK packets in the rolling window. Call advanceBucketsIfNeeded
// first if you want the answer reflective of current time.
inline uint32_t okLast24h(const Stats &s)
{
    uint32_t sum = 0;
    for (size_t i = 0; i < WINDOW_BUCKETS; i++) sum += s.okBuckets[i];
    return sum;
}

inline uint32_t rejectedLast24h(const Stats &s)
{
    uint32_t sum = 0;
    for (size_t i = 0; i < WINDOW_BUCKETS; i++) sum += s.rejectBuckets[i];
    return sum;
}

// Coarse status classification for UI badges.
enum class Status
{
    Unknown, // no packets ever recorded
    Healthy, // recent success and not stuck rejecting
    Errors,  // recent rejections (consecutive streak > THRESHOLD)
    Stale,   // last success too long ago
};

constexpr uint16_t      ERROR_STREAK_THRESHOLD = 5;
constexpr unsigned long STALE_AFTER_MS         = 6UL * BUCKET_MS; // 6 h with no success → stale

inline Status status(const Stats &s, unsigned long nowMillis)
{
    if (!s.hasLastOk && !s.hasLastReject)
    {
        return Status::Unknown;
    }
    if (s.consecutiveRejections >= ERROR_STREAK_THRESHOLD)
    {
        return Status::Errors;
    }
    if (s.hasLastOk &&
        static_cast<long>(nowMillis - s.lastOkAtMillis) > static_cast<long>(STALE_AFTER_MS))
    {
        return Status::Stale;
    }
    return Status::Healthy;
}

} // namespace SensorHealth
