/**
 * Native unit tests for SensorCalibration.
 *
 * Most interesting case: applyWindDirectionCorrection — the previous
 * inline implementation in SensorManager wrapped negative offsets to
 * ~65k° due to (uint16_t + int) % 360 promotion mismatch. These tests
 * pin the corrected behaviour.
 *
 * Run:   pio test -e native
 */

#include <unity.h>
#include "../../src/Data/SensorCalibration.h"

void setUp(void) {}
void tearDown(void) {}

// ---------------------------------------------------------------------------
// applyOffset — additive correction (temp / humidity / pressure / ppm / lux)
// ---------------------------------------------------------------------------

static void test_offset_zero_is_identity(void)
{
    TEST_ASSERT_EQUAL_FLOAT(21.5f, SensorCalibration::applyOffset(21.5f, 0.0f));
}

static void test_offset_positive(void)
{
    TEST_ASSERT_EQUAL_FLOAT(23.0f, SensorCalibration::applyOffset(21.5f, 1.5f));
}

static void test_offset_negative(void)
{
    TEST_ASSERT_EQUAL_FLOAT(20.0f, SensorCalibration::applyOffset(21.5f, -1.5f));
}

static void test_offset_preserves_zero_input(void)
{
    // 0 °C + offset behaves like any other reading; no special-casing.
    TEST_ASSERT_EQUAL_FLOAT(0.5f, SensorCalibration::applyOffset(0.0f, 0.5f));
}

// ---------------------------------------------------------------------------
// applyMultiplier — multiplicative correction (wind speed, rain)
// ---------------------------------------------------------------------------

static void test_multiplier_one_is_identity(void)
{
    TEST_ASSERT_EQUAL_FLOAT(4.2f, SensorCalibration::applyMultiplier(4.2f, 1.0f));
}

static void test_multiplier_scales(void)
{
    // 10 % uplift
    TEST_ASSERT_EQUAL_FLOAT(11.0f, SensorCalibration::applyMultiplier(10.0f, 1.10f));
}

static void test_multiplier_zero_zeros_value(void)
{
    // A multiplier of 0 silences the sensor — caller's responsibility.
    TEST_ASSERT_EQUAL_FLOAT(0.0f, SensorCalibration::applyMultiplier(123.45f, 0.0f));
}

static void test_multiplier_negative_is_passed_through(void)
{
    // Wind/rain shouldn't be negative, but the helper is dumb arithmetic;
    // it doesn't clamp. Test documents that intentionally.
    TEST_ASSERT_EQUAL_FLOAT(-5.0f, SensorCalibration::applyMultiplier(10.0f, -0.5f));
}

// ---------------------------------------------------------------------------
// applyWindDirectionCorrection — modular wrap with proper signed handling
// ---------------------------------------------------------------------------

static void test_wind_zero_correction_is_identity_for_normal_values(void)
{
    TEST_ASSERT_EQUAL_UINT16(180, SensorCalibration::applyWindDirectionCorrection(180, 0));
    TEST_ASSERT_EQUAL_UINT16(45,  SensorCalibration::applyWindDirectionCorrection(45,  0));
    TEST_ASSERT_EQUAL_UINT16(359, SensorCalibration::applyWindDirectionCorrection(359, 0));
}

static void test_wind_north_convention_returns_360(void)
{
    // True north (0°) is reported as 360° to preserve historical UI behaviour.
    TEST_ASSERT_EQUAL_UINT16(360, SensorCalibration::applyWindDirectionCorrection(0, 0));
    // 360 raw also stays 360 (360 % 360 == 0 → mapped back to 360).
    TEST_ASSERT_EQUAL_UINT16(360, SensorCalibration::applyWindDirectionCorrection(360, 0));
}

static void test_wind_positive_correction_no_wrap(void)
{
    TEST_ASSERT_EQUAL_UINT16(190, SensorCalibration::applyWindDirectionCorrection(180, 10));
}

static void test_wind_positive_correction_wraps_through_north(void)
{
    // 350° + 20° = 370° → 10°
    TEST_ASSERT_EQUAL_UINT16(10, SensorCalibration::applyWindDirectionCorrection(350, 20));
}

static void test_wind_positive_correction_lands_on_north(void)
{
    // 350° + 10° = 360° → mapped to 360 by north convention.
    TEST_ASSERT_EQUAL_UINT16(360, SensorCalibration::applyWindDirectionCorrection(350, 10));
}

static void test_wind_negative_correction_wraps_through_north(void)
{
    // 10° - 20° = -10° → 350°. This is the bug-fix case: the old inline
    // implementation produced 65526 here due to signed→unsigned promotion.
    TEST_ASSERT_EQUAL_UINT16(350, SensorCalibration::applyWindDirectionCorrection(10, -20));
}

static void test_wind_negative_correction_no_wrap(void)
{
    TEST_ASSERT_EQUAL_UINT16(170, SensorCalibration::applyWindDirectionCorrection(180, -10));
}

static void test_wind_large_positive_correction(void)
{
    // 0° + 720° = two full rotations → 0° → 360° (north convention)
    TEST_ASSERT_EQUAL_UINT16(360, SensorCalibration::applyWindDirectionCorrection(0, 720));
    // 180° + 720° → 180°
    TEST_ASSERT_EQUAL_UINT16(180, SensorCalibration::applyWindDirectionCorrection(180, 720));
}

static void test_wind_large_negative_correction(void)
{
    // 180° - 720° = -540° → -540 % 360 = -180 → +360 → 180°
    TEST_ASSERT_EQUAL_UINT16(180, SensorCalibration::applyWindDirectionCorrection(180, -720));
    // 90° - 720° → 90°
    TEST_ASSERT_EQUAL_UINT16(90, SensorCalibration::applyWindDirectionCorrection(90, -720));
}

static void test_wind_out_of_range_raw_is_normalised(void)
{
    // Sensor delivering out-of-spec value (400°) gets brought back into range.
    TEST_ASSERT_EQUAL_UINT16(40, SensorCalibration::applyWindDirectionCorrection(400, 0));
}

// ---------------------------------------------------------------------------

int main(int, char **)
{
    UNITY_BEGIN();

    RUN_TEST(test_offset_zero_is_identity);
    RUN_TEST(test_offset_positive);
    RUN_TEST(test_offset_negative);
    RUN_TEST(test_offset_preserves_zero_input);

    RUN_TEST(test_multiplier_one_is_identity);
    RUN_TEST(test_multiplier_scales);
    RUN_TEST(test_multiplier_zero_zeros_value);
    RUN_TEST(test_multiplier_negative_is_passed_through);

    RUN_TEST(test_wind_zero_correction_is_identity_for_normal_values);
    RUN_TEST(test_wind_north_convention_returns_360);
    RUN_TEST(test_wind_positive_correction_no_wrap);
    RUN_TEST(test_wind_positive_correction_wraps_through_north);
    RUN_TEST(test_wind_positive_correction_lands_on_north);
    RUN_TEST(test_wind_negative_correction_wraps_through_north);
    RUN_TEST(test_wind_negative_correction_no_wrap);
    RUN_TEST(test_wind_large_positive_correction);
    RUN_TEST(test_wind_large_negative_correction);
    RUN_TEST(test_wind_out_of_range_raw_is_normalised);

    return UNITY_END();
}
