/**
 * expLORA Gateway Lite — SensorCalibration
 *
 * Pure, header-only sensor calibration / correction helpers. Free of
 * Arduino dependencies so the logic can be exercised by host-side
 * unit tests under PlatformIO's `native` env.
 *
 * Three correction families:
 *   - Additive offsets   (temperature, humidity, pressure, ppm, lux)
 *   - Multiplicative     (wind speed, rain amount, rain rate)
 *   - Wind direction     (signed degree offset with modular wrap)
 *
 * The first two are trivial wrappers but live here so the calibration
 * model is described in one place and so the test suite documents the
 * expected behaviour at the system boundary.
 */

#pragma once

#include <cstdint>

namespace SensorCalibration
{

// Additive offset for sensors whose correction is a simple delta in the
// reading's own unit (°C, %RH, hPa, ppm, lux).
inline float applyOffset(float rawValue, float offset)
{
    return rawValue + offset;
}

// Multiplicative correction (a unit-less scaling factor). Used for
// wind speed, rain amount and rain rate where calibration is expressed
// as a percentage adjustment (e.g. 1.10 = +10 %).
inline float applyMultiplier(float rawValue, float multiplier)
{
    return rawValue * multiplier;
}

// Wind direction correction with proper modular arithmetic.
//
// `rawDirection` is the value reported by the sensor (typically 0..359
// or up to 360 for due north). `correctionDegrees` can be positive or
// negative and may exceed one full rotation.
//
// The historical inline implementation in SensorManager used
// `(uint16_t + int) % 360`, which suffers from C++ integer-promotion
// pitfalls: a negative result of `+` stays negative through `%`, then
// silently wraps to a huge value when stored back into a uint16_t.
// This helper does the arithmetic in `long` and normalises negatives
// before casting, so e.g. (10°, -20°) correctly yields 350°.
//
// Existing display convention is preserved: a post-modulo result of 0
// is reported as 360 ("true north" → 360°), keeping the visible range
// at 1..360 instead of 0..359.
inline uint16_t applyWindDirectionCorrection(uint16_t rawDirection, int correctionDegrees)
{
    long combined = static_cast<long>(rawDirection) + static_cast<long>(correctionDegrees);
    long mod = combined % 360;
    if (mod < 0)
    {
        mod += 360;
    }
    if (mod == 0)
    {
        return 360; // preserve historical "north as 360" convention
    }
    return static_cast<uint16_t>(mod);
}

} // namespace SensorCalibration
