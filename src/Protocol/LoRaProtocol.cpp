/**
 * expLORA Gateway Lite
 *
 * LoRa protocol handler implementation file
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

#include "LoRaProtocol.h"
#include "LoRaCrypto.h"
#include "SensorParsers.h"

// Constructor
LoRaProtocol::LoRaProtocol(LoRaModule &module, SensorManager &manager, Logger &log)
    : loraModule(module), sensorManager(manager), logger(log), lastProcessedSensorIndex(-1)
{
}

// Destructor
LoRaProtocol::~LoRaProtocol()
{
    // Nothing specific to release
}

// Process received packet
bool LoRaProtocol::processReceivedPacket()
{
    yield();
    // Check if data was received
    if (!loraModule.hasInterrupt())
    {
        return false;
    }

    // Reset interrupt flag
    loraModule.clearInterrupt();

    // Attempt to receive packet
    uint8_t length = 0;
    if (!loraModule.receivePacket(packetBuffer, &length))
    {
        logger.warning("Failed to receive packet from LoRa module");
        return false;
    }

    // Log received packet in hexadecimal format
    String hexData = "Received data (HEX): ";
    for (uint8_t i = 0; i < length; i++)
    {
        if (packetBuffer[i] < 0x10)
            hexData += "0";
        hexData += String(packetBuffer[i], HEX) + " ";
    }
    logger.debug(hexData);

    // Get RSSI for diagnostics
    int rssi = loraModule.getRSSI();
    logger.debug("RSSI: " + String(rssi) + " dBm, SNR: " + String(loraModule.getSNR()) + " dB");

    // Attempt to decrypt packet
    int sensorIndex = tryDecryptWithAllKeys(packetBuffer, length, decryptedBuffer);

    // If no known sensor is found
    if (sensorIndex < 0)
    {
        logger.debug("Unknown sensor detected - cannot process packet");
        return false;
    }

    // Log decrypted data
    hexData = "Decrypted data (HEX): ";
    for (uint8_t i = 0; i < length; i++)
    {
        if (decryptedBuffer[i] < 0x10)
            hexData += "0";
        hexData += String(decryptedBuffer[i], HEX) + " ";
    }
    logger.debug(hexData);

    // Check checksum
    unsigned long nowMillis = millis();
    if (!validateChecksum(decryptedBuffer, length))
    {
        logger.warning("Invalid checksum in received packet - data corrupted");
        sensorManager.recordSensorRejection(sensorIndex, nowMillis, "invalid checksum");
        return false;
    }

    // Check packet validity
    String reason;
    if (!isValidPacket(decryptedBuffer, length, reason))
    {
        logger.warning("Received packet has invalid format: " + reason);
        sensorManager.recordSensorRejection(sensorIndex, nowMillis, reason.c_str());
        return false;
    }

    // Get sensor type from decrypted data
    SensorType deviceType = static_cast<SensorType>(decryptedBuffer[1]);

    lastProcessedSensorIndex = sensorIndex;

    // Process packet according to sensor type
    bool ok = processPacketByType(deviceType, decryptedBuffer, length, sensorIndex, rssi);
    if (ok)
    {
        sensorManager.recordSensorSuccess(sensorIndex, nowMillis);
    }
    else
    {
        // Parser rejections, unknown type, updateSensorData failure
        sensorManager.recordSensorRejection(sensorIndex, nowMillis, "processing failed");
    }
    return ok;
}

bool LoRaProtocol::processPacketByType(SensorType type, uint8_t *data, uint8_t len, int sensorIndex, int rssi)
{
    switch (type)
    {
    case SensorType::BME280:
        return processBME280Packet(data, len, sensorIndex, rssi);

    case SensorType::SCD40:
        return processSCD40Packet(data, len, sensorIndex, rssi);

    case SensorType::VEML7700:
        return processVEML7700Packet(data, len, sensorIndex, rssi);

    case SensorType::METEO:
        return processMeteoPacket(data, len, sensorIndex, rssi);
            
    case SensorType::DIY_TEMP: 
        return processDIYTempPacket(data, len, sensorIndex, rssi);

    default:
        logger.warning("Unknown device type: 0x" + String(static_cast<uint8_t>(type), HEX));
        return false;
    }
}

// Process packet from BME280 sensor
bool LoRaProtocol::processBME280Packet(uint8_t *data, uint8_t len, int sensorIndex, int rssi)
{
    SensorParsers::CommonHeader header;
    SensorParsers::BME280Data   payload;
    if (!SensorParsers::parseCommonHeader(data, len, header) ||
        !SensorParsers::parseBME280(data, len, payload))
    {
        logger.warning("Packet too short for BME280");
        return false;
    }

    SensorData *sensor = sensorManager.getSensor(sensorIndex);
    if (!sensor)
    {
        logger.error("Error accessing sensor data at index " + String(sensorIndex));
        return false;
    }

    bool result = sensorManager.updateSensorData(sensorIndex,
                                                 payload.temperature, payload.humidity, payload.pressure,
                                                 0.0f, 0.0f, header.batteryVoltage, rssi);

    if (result)
    {
        logger.info(sensor->name + " data updated - Temp: " + String(sensor->temperature, 2) + "°C, Hum: " +
                    String(sensor->humidity, 2) + "%, Press: " + String(sensor->pressure, 2) + " hPa, Batt: " +
                    String(header.batteryVoltage, 2) + "V");
    }

    return result;
}

// Process packet from DIY temperature sensor
bool LoRaProtocol::processDIYTempPacket(uint8_t *data, uint8_t len, int sensorIndex, int rssi) {
    SensorParsers::CommonHeader header;
    SensorParsers::DIYTempData  payload;
    if (!SensorParsers::parseCommonHeader(data, len, header) ||
        !SensorParsers::parseDIYTemp(data, len, payload))
    {
        logger.warning("Packet too short for DS18B20");
        return false;
    }

    SensorData* sensor = sensorManager.getSensor(sensorIndex);
    if (!sensor) {
        logger.error("Error accessing sensor data at index " + String(sensorIndex));
        return false;
    }

    bool result = sensorManager.updateSensorData(sensorIndex,
                                                 payload.temperature, 0.0f, 0.0f,
                                                 0.0f, 0.0f, header.batteryVoltage, rssi);

    if (result) {
        logger.info(sensor->name + " data updated - Temp: " + String(payload.temperature, 2) + "°C, Batt: " +
                  String(header.batteryVoltage, 2) + "V");
    }

    return result;
}

// Process packet from SCD40 sensor
bool LoRaProtocol::processSCD40Packet(uint8_t *data, uint8_t len, int sensorIndex, int rssi)
{
    SensorParsers::CommonHeader header;
    SensorParsers::SCD40Data    payload;
    if (!SensorParsers::parseCommonHeader(data, len, header) ||
        !SensorParsers::parseSCD40(data, len, payload))
    {
        logger.warning("Packet too short for SCD40");
        return false;
    }

    SensorData *sensor = sensorManager.getSensor(sensorIndex);
    if (!sensor)
    {
        logger.error("Error accessing sensor data at index " + String(sensorIndex));
        return false;
    }

    bool result = sensorManager.updateSensorData(sensorIndex,
                                                 payload.temperature, payload.humidity, 0.0f,
                                                 payload.ppm, 0.0f, header.batteryVoltage, rssi);

    if (result)
    {
        logger.info(sensor->name + " data updated - Temp: " + String(sensor->temperature, 2) + "°C, Hum: " +
                    String(sensor->humidity, 2) + "%, CO2: " + String(sensor->ppm, 0) + " ppm, Batt: " +
                    String(header.batteryVoltage, 2) + "V");
    }

    return result;
}

// Process packet from VEML7700 sensor
bool LoRaProtocol::processVEML7700Packet(uint8_t *data, uint8_t len, int sensorIndex, int rssi)
{
    SensorParsers::CommonHeader header;
    SensorParsers::VEML7700Data payload;
    if (!SensorParsers::parseCommonHeader(data, len, header) ||
        !SensorParsers::parseVEML7700(data, len, payload))
    {
        logger.warning("Packet too short for VEML7700");
        return false;
    }

    SensorData *sensor = sensorManager.getSensor(sensorIndex);
    if (!sensor)
    {
        logger.error("Error accessing sensor data at index " + String(sensorIndex));
        return false;
    }

    bool result = sensorManager.updateSensorData(sensorIndex, 0.0f, 0.0f, 0.0f, 0.0f,
                                                 payload.lux, header.batteryVoltage, rssi);

    if (result)
    {
        logger.info(sensor->name + " data updated - Light: " + String(payload.lux, 1) + " lux, Batt: " +
                    String(header.batteryVoltage, 2) + "V");
    }

    return result;
}

bool LoRaProtocol::processMeteoPacket(uint8_t *data, uint8_t len, int sensorIndex, int rssi)
{
    SensorParsers::CommonHeader header;
    SensorParsers::METEOData    payload;
    if (!SensorParsers::parseCommonHeader(data, len, header) ||
        !SensorParsers::parseMETEO(data, len, payload))
    {
        logger.warning("Packet too short for METEO: " + String(len) + " bytes");
        return false;
    }

    logger.debug("METEO packet: SN=" + formatSN(header.serialNumber) +
                 ", battery=" + String(header.batteryVoltage) + "V, values=" + String(data[7]));

    logger.debug("METEO values: temp=" + String(payload.temperature) + "°C, press=" + String(payload.pressure) +
                 "hPa, hum=" + String(payload.humidity) + "%, wind=" + String(payload.windSpeed) +
                 "m/s at " + String(payload.windDirection) + "°, rain=" + String(payload.rainAmount) +
                 "mm, rate=" + String(payload.rainRate) + "mm/h");

    SensorData *sensor = sensorManager.getSensor(sensorIndex);
    if (!sensor)
    {
        logger.error("Error accessing sensor data at index " + String(sensorIndex));
        return false;
    }

    bool result = sensorManager.updateSensorData(sensorIndex,
                                                 payload.temperature, payload.humidity, payload.pressure,
                                                 0.0f, 0.0f, header.batteryVoltage, rssi,
                                                 payload.windSpeed, payload.windDirection,
                                                 payload.rainAmount, payload.rainRate);

    if (result)
    {
        logger.info(sensor->name + " data updated - Temp: " + String(sensor->temperature, 2) + "°C, Hum: " +
                    String(sensor->humidity, 2) + "%, Press: " + String(sensor->pressure, 2) + " hPa, Wind: " +
                    String(sensor->windSpeed, 1) + " m/s at " + String(sensor->windDirection) + "°, Rain: " +
                    String(sensor->rainAmount, 1) + " mm (rate: " + String(sensor->rainRate, 1) + " mm/h), Batt: " +
                    String(header.batteryVoltage, 2) + "V");
    }

    return result;
}

// Decrypt data with key — delegated to LoRaCrypto (covered by native unit tests).
void LoRaProtocol::decryptData(uint8_t *data, uint8_t data_len, uint32_t key)
{
    LoRaCrypto::decrypt(data, data_len, key);
}

// Try decryption with all known keys
int LoRaProtocol::tryDecryptWithAllKeys(uint8_t *encData, uint8_t len, uint8_t *decData)
{
    // Get list of all active sensors
    std::vector<SensorData> activeSensors = sensorManager.getActiveSensors();

    // Go through all sensors and try to decrypt
    for (size_t i = 0; i < activeSensors.size(); i++)
    {
        // Copy encrypted data
        memcpy(decData, encData, len);

        // Try to decrypt with this sensor's key
        decryptData(decData, len, activeSensors[i].deviceKey);

        // Verify checksum
        if (validateChecksum(decData, len))
        {
            // Verify that serial number in decrypted data matches the sensor
            uint32_t packetSN = ((uint32_t)decData[2] << 16) | ((uint32_t)decData[3] << 8) | decData[4];

            if (packetSN == activeSensors[i].serialNumber)
            {
                // We found a match, return sensor index
                logger.debug("Packet successfully decrypted with key from sensor " +
                             activeSensors[i].name + " (SN: " + formatSN(activeSensors[i].serialNumber) + ")");

                // Return sensor index in global array
                return sensorManager.findSensorBySN(activeSensors[i].serialNumber);
            }
        }
    }

    // If we didn't find a match, try to decrypt with any key for unknown sensor detection
    if (!activeSensors.empty())
    {
        // Use key from first sensor
        memcpy(decData, encData, len);
        decryptData(decData, len, activeSensors[0].deviceKey);
    }
    else
    {
        // We don't have any keys, just copy data
        memcpy(decData, encData, len);
    }

    return -1; // No sensor found
}

// Validate checksum — delegated to LoRaCrypto (covered by native unit tests).
bool LoRaProtocol::validateChecksum(uint8_t *buf, uint8_t len)
{
    return LoRaCrypto::validateChecksum(buf, len);
}

// Calculate checksum — delegated to LoRaCrypto (covered by native unit tests).
uint8_t LoRaProtocol::calculateChecksum(const uint8_t *data, uint8_t length)
{
    return LoRaCrypto::checksum(data, length);
}

// Check packet validity. On failure fills `reasonOut` with a short
// human-readable description (suitable for both logging and the web UI).
bool LoRaProtocol::isValidPacket(uint8_t *buf, uint8_t len, String &reasonOut)
{
    if (len < 9)
    {
        reasonOut = "packet too short (" + String(len) + " bytes)";
        return false;
    }

    uint8_t deviceType = buf[1];
    uint8_t numValues  = buf[7];

    // Length must match the declared value count (METEO has fixed sizes).
    if (deviceType == SENSOR_TYPE_METEO)
    {
        if (len != 23 && len != 21)
        {
            reasonOut = "invalid METEO packet length: " + String(len) + " (expected 21 or 23)";
            return false;
        }
        if (len == 23 && numValues == 6)
        {
            logger.info("Detected extended METEO packet with 7 values (including rain rate)");
        }
    }
    else
    {
        if (len != 8 + (numValues * 2) + 1)
        {
            reasonOut = "invalid packet length: " + String(len) +
                        " (expected " + String(8 + (numValues * 2) + 1) +
                        " for " + String(numValues) + " values)";
            return false;
        }
    }

    if (numValues > 10)
    {
        reasonOut = "invalid number of values: " + String(numValues);
        return false;
    }

    if (deviceType == 0 || deviceType > 256)
    {
        reasonOut = "invalid device type: " + String(deviceType);
        return false;
    }

    // Per-type range checks.
    if (deviceType == SENSOR_TYPE_METEO && numValues >= 6)
    {
        int16_t tempSigned = (int16_t)(((uint16_t)buf[8] << 8) | buf[9]);
        if (tempSigned < -5000 || tempSigned > 6000)
        {
            reasonOut = "invalid temperature: " + String(tempSigned);
            return false;
        }
        uint16_t press = ((uint16_t)buf[10] << 8) | buf[11];
        if (press < 8500 || press > 11000)
        {
            reasonOut = "invalid pressure: " + String(press);
            return false;
        }
        uint16_t hum = ((uint16_t)buf[12] << 8) | buf[13];
        if (hum > 10000)
        {
            reasonOut = "invalid humidity: " + String(hum);
            return false;
        }
        uint16_t windSpeed = ((uint16_t)buf[14] << 8) | buf[15];
        if (windSpeed > 6000)
        {
            reasonOut = "invalid wind speed: " + String(windSpeed);
            return false;
        }
        uint16_t windDir = ((uint16_t)buf[16] << 8) | buf[17];
        if (windDir > 360)
        {
            reasonOut = "invalid wind direction: " + String(windDir);
            return false;
        }
    }
    else if (numValues >= 3)
    {
        int16_t tempSigned = (int16_t)(((uint16_t)buf[8] << 8) | buf[9]);
        if (tempSigned < -5000 || tempSigned > 6000)
        {
            reasonOut = "invalid temperature: " + String(tempSigned);
            return false;
        }
        if (deviceType == SENSOR_TYPE_BME280)
        {
            uint16_t press = ((uint16_t)buf[10] << 8) | buf[11];
            if (press < 8500 || press > 11000)
            {
                reasonOut = "invalid pressure: " + String(press);
                return false;
            }
        }
        else if (deviceType == SENSOR_TYPE_SCD40)
        {
            uint16_t ppm = ((uint16_t)buf[10] << 8) | buf[11];
            if (ppm > 10000)
            {
                reasonOut = "invalid CO2 PPM: " + String(ppm);
                return false;
            }
        }
        uint16_t hum = ((uint16_t)buf[12] << 8) | buf[13];
        if (hum > 10000)
        {
            reasonOut = "invalid humidity: " + String(hum);
            return false;
        }
    }

    return true;
}
