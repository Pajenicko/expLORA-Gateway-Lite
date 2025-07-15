/**
 * expLORA Gateway Lite
 *
 * Sensor manager header file
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
#include <mutex>
#include <vector>
#include "SensorData.h"
#include "Logging.h"
#include "../Hardware/Network_Manager.h"

/**
 * Class for managing a collection of sensors
 *
 * Handles adding, modifying, and deleting sensors, searching for them,
 * and managing their data. Stores sensor configuration in the file system.
 */
class SensorManager
{
private:
    SensorData sensors[MAX_SENSORS]; // Array of sensors - limited to MAX_SENSORS
    size_t sensorCount;              // Current number of sensors
    mutable std::mutex sensorMutex;  // Mutex for safe multi-threaded access
    Logger &logger;                  // Reference to logger
    NetworkManager &networkManager;  // Reference to network manager

    // Filename for storing sensor configuration
    const char *sensorsFile;

public:
    // Constructor
    SensorManager(Logger &log, NetworkManager &nm, const char *file = SENSORS_FILE);

    // Destructor
    ~SensorManager();

    // Sensor manager initialization
    bool init();

    // Add a new sensor
    int addSensor(SensorType deviceType, uint32_t serialNumber, uint32_t deviceKey, const String &name);

    // Find sensor by serial number
    int findSensorBySN(uint32_t serialNumber);

    // Update sensor data
    bool updateSensor(int index, const SensorData &data);

    // Update sensor data by type (overloaded method for easier use)
    bool updateSensorData(int index, float temperature, float humidity, float pressure,
                          float ppm, float lux, float batteryVoltage, int rssi,
                          float windSpeed = 0.0f, uint16_t windDirection = 0,
                          float rainAmount = 0.0f, float rainRate = 0.0f);

    // Update sensor configuration
    bool updateSensorConfig(int index, const String &name, SensorType deviceType,
                            uint32_t serialNumber, uint32_t deviceKey,
                            const String &customUrl, int altitude,
                            float tempCorr, float humCorr, float pressCorr, float ppmCorr, float luxCorr,
                            float windSpeedCorr, int windDirCorr, float rainAmountCorr, float rainRateCorr);

    // Delete sensor
    bool deleteSensor(int index);

    // Get number of sensors
    size_t getSensorCount() const;

    // Get sensor by index
    const SensorData *getSensor(int index) const;
    SensorData *getSensor(int index);

    // Get list of all sensors
    std::vector<SensorData> getAllSensors() const;

    // Get list of all active sensors (configured)
    std::vector<SensorData> getActiveSensors() const;

    // Save sensor configuration to file
    bool saveSensors(bool lockMutex);

    // Load sensor configuration from file
    bool loadSensors();

    // Forward sensor data to HTTP
    bool forwardSensorData(int index);

    // Convert relative pressure to absolute pressure
    double relativeToAbsolutePressure(double p0_hpa, int altitude_m, double temp_c);
};