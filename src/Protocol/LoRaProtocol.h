/**
 * expLORA Gateway Lite
 *
 * LoRa protocol handler header file
 *
 * Copyright Pajenicko s.r.o., Igor Sverma (C) 2025
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <Arduino.h>
#include "../Hardware/LoRa_Module.h"
#include "../Data/SensorManager.h"
#include "../Data/Logging.h"

/**
 * Class for LoRa protocol processing
 *
 * Handles reception and decoding of LoRa packets, their decryption
 * and processing according to sensor type.
 */
class LoRaProtocol
{
private:
    LoRaModule &loraModule;       // Reference to LoRa module
    SensorManager &sensorManager; // Reference to sensor manager
    Logger &logger;               // Reference to logger

    // Maximum packet length
    static const size_t MAX_PACKET_LENGTH = 256;

    // Buffer for received packet
    uint8_t packetBuffer[MAX_PACKET_LENGTH];
    uint8_t decryptedBuffer[MAX_PACKET_LENGTH];

    // Process packet according to sensor type
    bool processBME280Packet(uint8_t *data, uint8_t len, int sensorIndex, int rssi);
    bool processSCD40Packet(uint8_t *data, uint8_t len, int sensorIndex, int rssi);
    bool processVEML7700Packet(uint8_t *data, uint8_t len, int sensorIndex, int rssi);
    bool processMeteoPacket(uint8_t *data, uint8_t len, int sensorIndex, int rssi);
    bool processDIYTempPacket(uint8_t *data, uint8_t len, int sensorIndex, int rssi);

    int lastProcessedSensorIndex; // Index of last processed sensor

    // Validate checksum
    bool validateChecksum(uint8_t *buf, uint8_t len);

    // Calculate checksum
    uint8_t calculateChecksum(const uint8_t *data, uint8_t length);

    // Check packet validity
    bool isValidPacket(uint8_t *buf, uint8_t len);

    // Factory method for processing packet according to sensor type
    bool processPacketByType(SensorType type, uint8_t *data, uint8_t len, int sensorIndex, int rssi);

public:
    // Constructor
    LoRaProtocol(LoRaModule &module, SensorManager &manager, Logger &log);

    // Destructor
    ~LoRaProtocol();

    // Process received packet
    bool processReceivedPacket();

    // Decrypt data with key
    void decryptData(uint8_t *data, uint8_t data_len, uint32_t key);

    // Try decryption with all known keys
    int tryDecryptWithAllKeys(uint8_t *encData, uint8_t len, uint8_t *decData);

    // Getter for lastProcessedSensorIndex
    int getLastProcessedSensorIndex() const { return lastProcessedSensorIndex; }
};
