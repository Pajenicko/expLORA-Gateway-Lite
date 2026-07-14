/**
 * expLORA Gateway Lite — LoRaCrypto
 *
 * Pure, header-only implementations of the LoRa packet crypto primitives.
 * Intentionally free of Arduino / ESP32 dependencies so it can be linked
 * into a native unit-test build (PlatformIO `native` env).
 *
 * The same functions are wrapped by LoRaProtocol::decryptData /
 * ::calculateChecksum / ::validateChecksum, which keeps the public API
 * stable while letting the algorithm be exercised in isolation.
 *
 * NOTE on endianness: the original decrypt reads the bytes of `key`
 * directly from memory (`(uint8_t*)&key`), which is little-endian on
 * both ESP32 and typical dev hosts (x86_64, ARM64 macOS/Linux). The
 * reimplementation here preserves that wire-compatible behaviour by
 * shifting the key explicitly, rather than relying on memory layout.
 */

#pragma once

#include <cstdint>
#include <cstddef>

namespace LoRaCrypto
{

// Decrypt `data_len` bytes in-place using a 32-bit rolling-XOR scheme that
// mirrors the sensor firmware. The cipher consumes the *encrypted* previous
// byte (shifted right by one) as part of the XOR mask, which makes the
// scheme order-sensitive but trivially reversible by the inverse operation
// in encrypt.
inline void decrypt(uint8_t *data, uint8_t data_len, uint32_t key)
{
    const uint8_t key_bytes[4] = {
        static_cast<uint8_t>(key & 0xFF),
        static_cast<uint8_t>((key >> 8) & 0xFF),
        static_cast<uint8_t>((key >> 16) & 0xFF),
        static_cast<uint8_t>((key >> 24) & 0xFF),
    };

    uint8_t prev_byte = 0;
    for (uint8_t i = 0; i < data_len; i++)
    {
        const uint8_t key_byte = key_bytes[i & 0x03];
        const uint8_t current_encrypted = data[i];
        data[i] = current_encrypted ^ key_byte ^ static_cast<uint8_t>(prev_byte >> 1);
        prev_byte = current_encrypted;
    }
}

// Simple XOR-fold checksum over `length` bytes.
inline uint8_t checksum(const uint8_t *data, uint8_t length)
{
    uint8_t cs = 0;
    for (uint8_t i = 0; i < length; i++)
    {
        cs ^= data[i];
    }
    return cs;
}

// Validates that the last byte of `buf` equals the XOR checksum of the
// preceding (len - 1) bytes. Returns false for buffers shorter than 2.
inline bool validateChecksum(const uint8_t *buf, uint8_t len)
{
    if (len < 2)
    {
        return false;
    }
    return buf[len - 1] == checksum(buf, len - 1);
}

} // namespace LoRaCrypto
