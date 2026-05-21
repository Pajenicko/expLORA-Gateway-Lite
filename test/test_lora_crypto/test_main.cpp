/**
 * Native unit tests for LoRaCrypto.
 *
 * Builds with PlatformIO's `native` env (host toolchain), no flashing needed.
 * Run:   pio test -e native
 */

#include <unity.h>
#include <cstring>
#include "../../src/Protocol/LoRaCrypto.h"

void setUp(void) {}
void tearDown(void) {}

// ---------------------------------------------------------------------------
// checksum()
// ---------------------------------------------------------------------------

static void test_checksum_empty_buffer_is_zero(void)
{
    TEST_ASSERT_EQUAL_UINT8(0, LoRaCrypto::checksum(nullptr, 0));
}

static void test_checksum_single_byte_returns_that_byte(void)
{
    const uint8_t buf[] = {0x42};
    TEST_ASSERT_EQUAL_UINT8(0x42, LoRaCrypto::checksum(buf, 1));
}

static void test_checksum_known_xor_fold(void)
{
    const uint8_t buf[] = {0xAA, 0x55, 0xFF, 0x00, 0x12};
    // 0xAA ^ 0x55 = 0xFF; ^ 0xFF = 0x00; ^ 0x00 = 0x00; ^ 0x12 = 0x12
    TEST_ASSERT_EQUAL_UINT8(0x12, LoRaCrypto::checksum(buf, 5));
}

static void test_checksum_xor_self_is_zero(void)
{
    // XOR is its own inverse: x ^ x == 0
    const uint8_t buf[] = {0xDE, 0xAD, 0xDE, 0xAD};
    TEST_ASSERT_EQUAL_UINT8(0, LoRaCrypto::checksum(buf, 4));
}

// ---------------------------------------------------------------------------
// validateChecksum()
// ---------------------------------------------------------------------------

static void test_validate_rejects_too_short(void)
{
    const uint8_t buf[] = {0x00};
    TEST_ASSERT_FALSE(LoRaCrypto::validateChecksum(buf, 0));
    TEST_ASSERT_FALSE(LoRaCrypto::validateChecksum(buf, 1));
}

static void test_validate_accepts_valid_packet(void)
{
    // payload [0x10, 0x20, 0x30] -> XOR = 0x00
    const uint8_t valid[] = {0x10, 0x20, 0x30, 0x00};
    TEST_ASSERT_TRUE(LoRaCrypto::validateChecksum(valid, 4));
}

static void test_validate_rejects_corrupted_payload(void)
{
    // One byte flipped from the valid packet above.
    const uint8_t corrupted[] = {0x10, 0x21, 0x30, 0x00};
    TEST_ASSERT_FALSE(LoRaCrypto::validateChecksum(corrupted, 4));
}

static void test_validate_rejects_wrong_checksum(void)
{
    // Correct payload, but trailing checksum byte is wrong.
    const uint8_t bad_cs[] = {0x10, 0x20, 0x30, 0xFF};
    TEST_ASSERT_FALSE(LoRaCrypto::validateChecksum(bad_cs, 4));
}

// ---------------------------------------------------------------------------
// decrypt() — known-answer tests
//
// Fixture derived by running the encrypt-equivalent of the algorithm by hand:
//   prev = 0
//   for i in 0..len:
//     enc[i] = plain[i] ^ key_bytes[i & 3] ^ (prev >> 1)
//     prev = enc[i]
// With key = 0x04030201 (LE bytes: 0x01,0x02,0x03,0x04) and
// plain = [0xAA,0xBB,0xCC,0xDD,0xEE], the wire bytes are
// [0xAB,0xEC,0xB9,0x85,0xAD]. Decrypt must invert that.
// ---------------------------------------------------------------------------

static void test_decrypt_known_answer(void)
{
    uint8_t buf[] = {0xAB, 0xEC, 0xB9, 0x85, 0xAD};
    const uint8_t expected[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
    LoRaCrypto::decrypt(buf, sizeof(buf), 0x04030201);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, buf, sizeof(buf));
}

static void test_decrypt_empty_buffer_is_noop(void)
{
    uint8_t buf[1] = {0xCC}; // canary
    LoRaCrypto::decrypt(buf, 0, 0xDEADBEEF);
    TEST_ASSERT_EQUAL_UINT8(0xCC, buf[0]);
}

static void test_decrypt_wrong_key_does_not_recover_plaintext(void)
{
    uint8_t buf[] = {0xAB, 0xEC, 0xB9, 0x85, 0xAD};
    const uint8_t correct_plain[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
    LoRaCrypto::decrypt(buf, sizeof(buf), 0x04030200); // off-by-one key
    // Output must differ from the true plaintext
    TEST_ASSERT_NOT_EQUAL(0, std::memcmp(buf, correct_plain, sizeof(buf)));
}

static void test_decrypt_first_byte_only_uses_key_byte0(void)
{
    // With a one-byte buffer, prev_byte starts at 0 so the result is
    // just plain[0] = enc[0] ^ key_bytes[0]. Confirms the (prev >> 1)
    // term contributes nothing for i == 0.
    uint8_t buf[] = {0x77};
    LoRaCrypto::decrypt(buf, 1, 0x000000AA); // key_bytes[0] == 0xAA
    TEST_ASSERT_EQUAL_UINT8(0x77 ^ 0xAA, buf[0]);
}

static void test_decrypt_key_zero_returns_modified_buffer(void)
{
    // With key = 0, the cipher reduces to data[i] ^= (prev_enc[i-1] >> 1).
    // Hand-trace for [0x10, 0x20, 0x30]:
    //   i=0: prev=0   -> data[0] = 0x10 ^ 0 ^ 0    = 0x10; prev = 0x10
    //   i=1: prev=0x10-> data[1] = 0x20 ^ 0 ^ 0x08 = 0x28; prev = 0x20
    //   i=2: prev=0x20-> data[2] = 0x30 ^ 0 ^ 0x10 = 0x20
    uint8_t buf[] = {0x10, 0x20, 0x30};
    const uint8_t expected[] = {0x10, 0x28, 0x20};
    LoRaCrypto::decrypt(buf, sizeof(buf), 0);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, buf, sizeof(buf));
}

// ---------------------------------------------------------------------------
// Round-trip: encrypt-then-decrypt should restore the plaintext.
// The encrypt direction is the same transform run with a swapped
// `prev` source — useful as a property-style sanity check that the
// algorithm is invertible.
// ---------------------------------------------------------------------------

static void encrypt_inverse_of_decrypt(uint8_t *data, uint8_t len, uint32_t key)
{
    const uint8_t key_bytes[4] = {
        static_cast<uint8_t>(key & 0xFF),
        static_cast<uint8_t>((key >> 8) & 0xFF),
        static_cast<uint8_t>((key >> 16) & 0xFF),
        static_cast<uint8_t>((key >> 24) & 0xFF),
    };
    uint8_t prev_enc = 0;
    for (uint8_t i = 0; i < len; i++)
    {
        const uint8_t plain = data[i];
        const uint8_t enc = plain ^ key_bytes[i & 0x03] ^ static_cast<uint8_t>(prev_enc >> 1);
        data[i] = enc;
        prev_enc = enc;
    }
}

static void test_encrypt_then_decrypt_round_trip(void)
{
    const uint8_t original[] = {0x01, 0xFF, 0x7F, 0x80, 0x00, 0xA5, 0x5A, 0xC3};
    uint8_t buf[sizeof(original)];
    std::memcpy(buf, original, sizeof(original));

    const uint32_t key = 0xCAFEBABE;
    encrypt_inverse_of_decrypt(buf, sizeof(buf), key);
    LoRaCrypto::decrypt(buf, sizeof(buf), key);

    TEST_ASSERT_EQUAL_UINT8_ARRAY(original, buf, sizeof(buf));
}

// ---------------------------------------------------------------------------

int main(int, char **)
{
    UNITY_BEGIN();

    RUN_TEST(test_checksum_empty_buffer_is_zero);
    RUN_TEST(test_checksum_single_byte_returns_that_byte);
    RUN_TEST(test_checksum_known_xor_fold);
    RUN_TEST(test_checksum_xor_self_is_zero);

    RUN_TEST(test_validate_rejects_too_short);
    RUN_TEST(test_validate_accepts_valid_packet);
    RUN_TEST(test_validate_rejects_corrupted_payload);
    RUN_TEST(test_validate_rejects_wrong_checksum);

    RUN_TEST(test_decrypt_known_answer);
    RUN_TEST(test_decrypt_empty_buffer_is_noop);
    RUN_TEST(test_decrypt_wrong_key_does_not_recover_plaintext);
    RUN_TEST(test_decrypt_first_byte_only_uses_key_byte0);
    RUN_TEST(test_decrypt_key_zero_returns_modified_buffer);

    RUN_TEST(test_encrypt_then_decrypt_round_trip);

    return UNITY_END();
}
