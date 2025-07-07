/**
 * expLORA Gateway Lite
 *
 * Sensor manager implementation file
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

#include "SensorManager.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <HTTPClient.h>

// Constructor
SensorManager::SensorManager(Logger &log, NetworkManager &nm, const char *file)
    : sensorCount(0), logger(log), networkManager(nm), sensorsFile(file)
{
}

// Destructor
SensorManager::~SensorManager()
{
    // Nothing special needs to be released, all objects will be released automatically
}

// Sensor manager initialization
bool SensorManager::init()
{
    logger.info("Initializing sensor manager");
    return loadSensors();
}

// Constants for pressure conversion
#define G 9.80665   // gravitational acceleration [m/s^2]
#define M 0.0289644 // molar mass of air [kg/mol]
#define R 8.3144598 // universal gas constant [J/(mol·K)]
#define L 0.0065    // temperature gradient [K/m]

// Convert relative pressure to absolute pressure
double SensorManager::relativeToAbsolutePressure(double p_rel_hpa, int altitude_m, double temp_c)
{
    if (altitude_m == 0)
    {
        return p_rel_hpa;
    }
    double T = temp_c + 273.15;
    double exponent = (G * M) / (R * L);
    return p_rel_hpa / pow(1 - (L * altitude_m) / T, exponent);
}

// Add a new sensor
int SensorManager::addSensor(SensorType deviceType, uint32_t serialNumber, uint32_t deviceKey, const String &name)
{
    std::lock_guard<std::mutex> lock(sensorMutex);

    // Check if sensor already exists
    int existingIndex = findSensorBySN(serialNumber);
    if (existingIndex >= 0)
    {
        // Update existing sensor
        sensors[existingIndex].deviceType = deviceType;
        sensors[existingIndex].deviceKey = deviceKey;
        sensors[existingIndex].name = name;
        sensors[existingIndex].configured = true;

        logger.info("Updated existing sensor: " + name + " (SN: " + String(serialNumber, HEX) + ")");
        saveSensors(false);
        return existingIndex;
    }

    // Check if we still have space
    if (sensorCount >= MAX_SENSORS)
    {
        logger.error("Failed to add sensor: maximum number of sensors reached");
        return -1;
    }

    // Find first free slot
    int newIndex = -1;
    for (size_t i = 0; i < MAX_SENSORS; i++)
    {
        if (!sensors[i].configured)
        {
            newIndex = i;
            break;
        }
    }

    // If no free slot, use the last index
    if (newIndex == -1)
    {
        newIndex = sensorCount;
        sensorCount++;
    }
    else if (newIndex >= sensorCount)
    {
        sensorCount = newIndex + 1;
    }

    // Initialize new sensor
    sensors[newIndex].deviceType = deviceType;
    sensors[newIndex].serialNumber = serialNumber;
    sensors[newIndex].deviceKey = deviceKey;
    sensors[newIndex].name = name;
    sensors[newIndex].customUrl = "";
    sensors[newIndex].lastSeen = 0;
    sensors[newIndex].temperature = 0.0f;
    sensors[newIndex].humidity = 0.0f;
    sensors[newIndex].pressure = 0.0f;
    sensors[newIndex].ppm = 0.0f;
    sensors[newIndex].lux = 0.0f;
    sensors[newIndex].batteryVoltage = 0.0f;
    sensors[newIndex].rssi = 0;
    sensors[newIndex].configured = true;

    logger.info("Added new sensor: " + name + " (SN: " + String(serialNumber, HEX) + ")");
    saveSensors(false);
    return newIndex;
}

// Find sensor by serial number
int SensorManager::findSensorBySN(uint32_t serialNumber)
{
    for (size_t i = 0; i < sensorCount; i++)
    {
        if (sensors[i].configured && sensors[i].serialNumber == serialNumber)
        {
            return i;
        }
    }
    return -1;
}

// Update sensor data
bool SensorManager::updateSensor(int index, const SensorData &data)
{
    std::lock_guard<std::mutex> lock(sensorMutex);

    if (index < 0 || index >= sensorCount || !sensors[index].configured)
    {
        logger.warning("Attempt to update non-existent sensor at index " + String(index));
        return false;
    }

    // Update data while preserving configuration
    sensors[index].deviceType = data.deviceType;
    sensors[index].temperature = data.temperature;
    sensors[index].humidity = data.humidity;
    sensors[index].pressure = data.pressure;
    sensors[index].ppm = data.ppm;
    sensors[index].lux = data.lux;
    sensors[index].batteryVoltage = data.batteryVoltage;
    sensors[index].rssi = data.rssi;
    sensors[index].lastSeen = millis();

    return true;
}

// Update sensor data by type
bool SensorManager::updateSensorData(int index, float temperature, float humidity, float pressure,
                                     float ppm, float lux, float batteryVoltage, int rssi,
                                     float windSpeed, uint16_t windDirection,
                                     float rainAmount, float rainRate)
{
    std::lock_guard<std::mutex> lock(sensorMutex);

    if (index < 0 || index >= sensorCount || !sensors[index].configured)
    {
        logger.warning("Attempt to update non-existent sensor at index " + String(index));
        return false;
    }

    // Store original values for logging
    float originalTemp = temperature;
    float originalHum = humidity;
    float originalPress = pressure;
    float originalPPM = ppm;
    float originalLux = lux;
    float originalWindSpeed = windSpeed;
    uint16_t originalWindDir = windDirection;
    float originalRainAmount = rainAmount;
    float originalRainRate = rainRate;

    // Apply corrections to input values before updating
    temperature += sensors[index].temperatureCorrection;
    humidity += sensors[index].humidityCorrection;
    pressure += sensors[index].pressureCorrection;
    ppm += sensors[index].ppmCorrection;
    lux += sensors[index].luxCorrection;
    windSpeed *= sensors[index].windSpeedCorrection;

    // For wind direction, ensure value stays in 0-359 range
    windDirection = (windDirection + sensors[index].windDirectionCorrection) % 360;

    rainAmount *= sensors[index].rainAmountCorrection;
    rainRate *= sensors[index].rainRateCorrection;

    // Log if corrections were applied
    bool correctionsApplied = false;
    String correctionLog = "Corrections applied to " + sensors[index].name + ": ";

    if (sensors[index].hasTemperature() && sensors[index].temperatureCorrection != 0)
    {
        correctionLog += "Temp " + String(originalTemp, 2) + "→" + String(temperature, 2) + "°C, ";
        correctionsApplied = true;
    }

    if (sensors[index].hasHumidity() && sensors[index].humidityCorrection != 0)
    {
        correctionLog += "Hum " + String(originalHum, 2) + "→" + String(humidity, 2) + "%, ";
        correctionsApplied = true;
    }

    if (sensors[index].hasPressure() && sensors[index].pressureCorrection != 0)
    {
        correctionLog += "Press " + String(originalPress, 2) + "→" + String(pressure, 2) + "hPa, ";
        correctionsApplied = true;
    }

    if (sensors[index].hasPPM() && sensors[index].ppmCorrection != 0)
    {
        correctionLog += "CO2 " + String(originalPPM, 0) + "→" + String(ppm, 0) + "ppm, ";
        correctionsApplied = true;
    }

    if (sensors[index].hasLux() && sensors[index].luxCorrection != 0)
    {
        correctionLog += "Lux " + String(originalLux, 1) + "→" + String(lux, 1) + "lx, ";
        correctionsApplied = true;
    }

    if (sensors[index].hasWindSpeed() && sensors[index].windSpeedCorrection != 1.0f)
    {
        correctionLog += "Wind " + String(originalWindSpeed, 1) + "→" + String(windSpeed, 1) + "m/s, ";
        correctionsApplied = true;
    }

    if (sensors[index].hasWindDirection() && sensors[index].windDirectionCorrection != 0)
    {
        correctionLog += "Dir " + String(originalWindDir) + "→" + String(windDirection) + "°, ";
        correctionsApplied = true;
    }

    if (sensors[index].hasRainAmount() && sensors[index].rainAmountCorrection != 1.0f)
    {
        correctionLog += "Rain " + String(originalRainAmount, 1) + "→" + String(rainAmount, 1) + "mm, ";
        correctionsApplied = true;
    }

    if (sensors[index].hasRainRate() && sensors[index].rainRateCorrection != 1.0f)
    {
        correctionLog += "Rate " + String(originalRainRate, 1) + "→" + String(rainRate, 1) + "mm/h, ";
        correctionsApplied = true;
    }

    // Log corrections if any were applied
    if (correctionsApplied)
    {
        // Remove trailing comma and space
        correctionLog = correctionLog.substring(0, correctionLog.length() - 2);
        logger.debug(correctionLog);
    }

    // Adjust pressure for altitude
    // Only adjust if the sensor has pressure capability and altitude is set
    if (sensors[index].hasPressure() && sensors[index].altitude > 0)
    {
        float adjustedPressure = relativeToAbsolutePressure(pressure, sensors[index].altitude, temperature);
        logger.debug("Adjusted pressure from " + String(pressure, 2) + " hPa to " +
                     String(adjustedPressure, 2) + " hPa at altitude " +
                     String(sensors[index].altitude) + " m");
        pressure = adjustedPressure;
    }

    // Update only relevant values according to sensor type
    if (sensors[index].hasTemperature())
    {
        sensors[index].temperature = temperature;
    }

    if (sensors[index].hasHumidity())
    {
        sensors[index].humidity = humidity;
    }

    if (sensors[index].hasPressure())
    {
        sensors[index].pressure = pressure;
    }

    if (sensors[index].hasPPM())
    {
        sensors[index].ppm = ppm;
    }

    if (sensors[index].hasLux())
    {
        sensors[index].lux = lux;
    }

    // Meteorological data
    if (sensors[index].hasWindSpeed())
    {
        sensors[index].windSpeed = windSpeed;
    }

    if (sensors[index].hasWindDirection())
    {
        sensors[index].windDirection = windDirection;
    }

    if (sensors[index].hasRainAmount())
    {
        sensors[index].rainAmount = rainAmount;

        // Check if we need to reset daily total (new day)
        if (Logger::isTimeInitialized())
        { // Using method from Logger
            // Get current time
            struct tm timeinfo;
            time_t now;
            time(&now);

            if (getLocalTime(&timeinfo))
            {
                // Create timestamps for comparing dates
                // Date of last reset
                struct tm lastResetTime;
                localtime_r((time_t *)&sensors[index].lastRainReset, &lastResetTime);

                // If last reset was on a different day than today, reset the counter
                if (sensors[index].lastRainReset == 0 ||
                    lastResetTime.tm_mday != timeinfo.tm_mday ||
                    lastResetTime.tm_mon != timeinfo.tm_mon ||
                    lastResetTime.tm_year != timeinfo.tm_year)
                {

                    logger.info("Resetting daily rain total for sensor: " + sensors[index].name);
                    sensors[index].dailyRainTotal = 0.0f;
                    sensors[index].lastRainReset = now;
                }
            }
        }

        // Add current rain amount to daily total
        sensors[index].dailyRainTotal += rainAmount;

        // Log the updated daily total if it has changed
        if (rainAmount > 0)
        {
           saveSensors(false); // Save immediately after updating daily total
        }
    }

    if (sensors[index].hasRainRate())
    {
        sensors[index].rainRate = rainRate;
    }

    // Always update general data
    sensors[index].batteryVoltage = batteryVoltage;
    sensors[index].rssi = rssi;
    sensors[index].lastSeen = millis();

    if (networkManager.isConnected())
    {
        forwardSensorData(index);
    }
    else
    {
        logger.debug("Not forwarding data - Not connected");
    }

    return true;
}

bool SensorManager::forwardSensorData(int index)
{
    if (index < 0 || index >= sensorCount || !sensors[index].configured)
    {
        logger.warning("Attempt to forward data for non-existent sensor at index " + String(index));
        return false;
    }

    // Check if the sensor has a custom endpoint configured
    if (sensors[index].customUrl.length() == 0)
    {
        // No endpoint configured, nothing to do
        return true;
    }

    // Create a WiFi client for HTTP request
    WiFiClient client;
    HTTPClient http;

    // Format the custom URL by replacing placeholders
    String url = sensors[index].customUrl;

    // Skip if no URL is configured
    if (url.length() == 0)
    {
        return true;
    }

    // Check if HTTP or HTTPS
    bool isHttps = url.startsWith("https://");

    // Process the URL - replace placeholders with actual values
    if (sensors[index].hasTemperature())
    {
        url.replace("*TEMP*", String(sensors[index].temperature, 2));
    }
    if (sensors[index].hasHumidity())
    {
        url.replace("*HUM*", String(sensors[index].humidity, 2));
    }
    if (sensors[index].hasPressure())
    {
        url.replace("*PRESS*", String(sensors[index].pressure, 2));
    }
    if (sensors[index].hasPPM())
    {
        url.replace("*PPM*", String(sensors[index].ppm, 0));
    }
    if (sensors[index].hasLux())
    {
        url.replace("*LUX*", String(sensors[index].lux, 1));
    }

    // Added placeholders for METEO data
    if (sensors[index].hasWindSpeed())
    {
        url.replace("*WIND_SPEED*", String(sensors[index].windSpeed, 1));
    }
    if (sensors[index].hasWindDirection())
    {
        url.replace("*WIND_DIR*", String(sensors[index].windDirection));
    }
    if (sensors[index].hasRainAmount())
    {
        url.replace("*RAIN*", String(sensors[index].rainAmount, 1));
        url.replace("*DAILY_RAIN*", String(sensors[index].dailyRainTotal, 1)); // New placeholder
    }
    if (sensors[index].hasRainRate())
    {
        url.replace("*RAIN_RATE*", String(sensors[index].rainRate, 1));
    }

    // Replace other common placeholders
    url.replace("*BAT*", String(sensors[index].batteryVoltage, 2));
    url.replace("*RSSI*", String(sensors[index].rssi));
    url.replace("*SN*", String(sensors[index].serialNumber, HEX));
    url.replace("*TYPE*", String(static_cast<uint8_t>(sensors[index].deviceType)));

    logger.debug("Forwarding data for sensor " + sensors[index].name + " to URL: " + url);

    // For HTTPS, use a secure client but with insecure flag
    if (isHttps)
    {
        WiFiClientSecure *secureClient = new WiFiClientSecure();
        secureClient->setInsecure(); // Skip certificate validation
        http.begin(*secureClient, url);
    }
    else
    {
        http.begin(client, url);
    }

    // Send the request
    int httpCode = http.GET();

    // Check result
    if (httpCode > 0)
    {
        logger.debug("HTTP request sent, response code: " + String(httpCode));
        if (httpCode == HTTP_CODE_OK)
        {
            String payload = http.getString();
            logger.debug("Response: " + payload.substring(0, 100)); // Only log first 100 chars
        }
    }
    else
    {
        logger.warning("HTTP request failed: " + http.errorToString(httpCode));
    }

    http.end();

    return (httpCode == HTTP_CODE_OK);
}

// Update sensor configuration
bool SensorManager::updateSensorConfig(int index, const String &name, SensorType deviceType,
                                       uint32_t serialNumber, uint32_t deviceKey,
                                       const String &customUrl, int altitude,
                                       float tempCorr, float humCorr, float pressCorr,
                                       float ppmCorr, float luxCorr,
                                       float windSpeedCorr, int windDirCorr,
                                       float rainAmountCorr, float rainRateCorr)
{
    std::lock_guard<std::mutex> lock(sensorMutex);

    if (index < 0 || index >= sensorCount || !sensors[index].configured)
    {
        logger.warning("Attempt to update non-existent sensor at index " + String(index));
        return false;
    }

    // Check if serial number is already used by another sensor
    int existingIndex = findSensorBySN(serialNumber);
    if (existingIndex >= 0 && existingIndex != index)
    {
        logger.warning("Cannot update sensor config: Serial number " +
                       String(serialNumber, HEX) + " already used by sensor " +
                       sensors[existingIndex].name);
        return false;
    }

    // Update basic configuration
    sensors[index].name = name;
    sensors[index].deviceType = deviceType;
    sensors[index].serialNumber = serialNumber;
    sensors[index].deviceKey = deviceKey;
    sensors[index].customUrl = customUrl;
    sensors[index].altitude = altitude;

    // Update correction values
    sensors[index].temperatureCorrection = tempCorr;
    sensors[index].humidityCorrection = humCorr;
    sensors[index].pressureCorrection = pressCorr;
    sensors[index].ppmCorrection = ppmCorr;
    sensors[index].luxCorrection = luxCorr;
    sensors[index].windSpeedCorrection = windSpeedCorr;
    sensors[index].windDirectionCorrection = windDirCorr;
    sensors[index].rainAmountCorrection = rainAmountCorr;
    sensors[index].rainRateCorrection = rainRateCorr;

    logger.info("Updated configuration for sensor: " + name + " (SN: " + String(serialNumber, HEX) + ")");
    saveSensors(false);
    return true;
}

// Delete sensor
bool SensorManager::deleteSensor(int index)
{
    std::lock_guard<std::mutex> lock(sensorMutex);

    if (index < 0 || index >= sensorCount || !sensors[index].configured)
    {
        logger.warning("Attempt to delete non-existent sensor at index " + String(index));
        return false;
    }

    String name = sensors[index].name;
    uint32_t serialNumber = sensors[index].serialNumber;

    // Mark as unconfigured instead of physically removing
    sensors[index].configured = false;

    logger.info("Deleted sensor: " + name + " (SN: " + String(serialNumber, HEX) + ")");
    saveSensors(false);
    return true;
}

// Get number of sensors
size_t SensorManager::getSensorCount() const
{
    return sensorCount;
}

// Get sensor by index (const version)
const SensorData *SensorManager::getSensor(int index) const
{
    if (index < 0 || index >= sensorCount || !sensors[index].configured)
    {
        return nullptr;
    }
    return &sensors[index];
}

// Get sensor by index (non-const version)
SensorData *SensorManager::getSensor(int index)
{
    if (index < 0 || index >= sensorCount || !sensors[index].configured)
    {
        return nullptr;
    }
    return &sensors[index];
}

// Get list of all sensors
std::vector<SensorData> SensorManager::getAllSensors() const
{
    std::lock_guard<std::mutex> lock(sensorMutex);

    std::vector<SensorData> result;
    for (size_t i = 0; i < sensorCount; i++)
    {
        result.push_back(sensors[i]);
    }
    return result;
}

// Get list of all active sensors (configured)
std::vector<SensorData> SensorManager::getActiveSensors() const
{
    std::lock_guard<std::mutex> lock(sensorMutex);

    std::vector<SensorData> result;
    for (size_t i = 0; i < sensorCount; i++)
    {
        if (sensors[i].configured)
        {
            result.push_back(sensors[i]);
        }
    }
    return result;
}

// Save sensor configuration to file
bool SensorManager::saveSensors(bool lockMutex)
{
    if (lockMutex)
    {
        std::lock_guard<std::mutex> lock(sensorMutex);
    }

    // Open file for writing
    File file = LittleFS.open(sensorsFile, "w");
    if (!file)
    {
        logger.info("Failed to open sensors file for writing: " + String(sensorsFile));
        return false;
    }

    // Create JSON document
    DynamicJsonDocument doc(4096); // Sufficiently large buffer
    JsonArray sensorArray = doc.createNestedArray("sensors");

    // Add sensors to JSON
    for (size_t i = 0; i < sensorCount; i++)
    {
        if (sensors[i].configured)
        {
            JsonObject sensor = sensorArray.createNestedObject();
            sensor["deviceType"] = static_cast<uint8_t>(sensors[i].deviceType);
            sensor["serialNumber"] = sensors[i].serialNumber;
            sensor["deviceKey"] = sensors[i].deviceKey;
            sensor["name"] = sensors[i].name;
            sensor["customUrl"] = sensors[i].customUrl;
            sensor["altitude"] = sensors[i].altitude;

            // Also save daily rain total and time of last reset
            if (sensors[i].hasRainAmount())
            {
                sensor["dailyRainTotal"] = sensors[i].dailyRainTotal;
                sensor["lastRainReset"] = sensors[i].lastRainReset;
            }

            sensor["temperatureCorrection"] = sensors[i].temperatureCorrection;
            sensor["humidityCorrection"] = sensors[i].humidityCorrection;
            sensor["pressureCorrection"] = sensors[i].pressureCorrection;
            sensor["ppmCorrection"] = sensors[i].ppmCorrection;
            sensor["luxCorrection"] = sensors[i].luxCorrection;
            sensor["windSpeedCorrection"] = sensors[i].windSpeedCorrection;
            sensor["windDirectionCorrection"] = sensors[i].windDirectionCorrection;
            sensor["rainAmountCorrection"] = sensors[i].rainAmountCorrection;
            sensor["rainRateCorrection"] = sensors[i].rainRateCorrection;
        }
    }
    logger.info("Serializing sensors to JSON");
    // Serialize JSON to file
    if (serializeJson(doc, file) == 0)
    {
        logger.info("Failed to write sensors to file");
        file.close();
        return false;
    }

    file.close();
    logger.info("Saved " + String(sensorArray.size()) + " sensors to " + String(sensorsFile));
    return true;
}

// Load sensor configuration from file
bool SensorManager::loadSensors()
{
    std::lock_guard<std::mutex> lock(sensorMutex);

    // Reset sensor count
    sensorCount = 0;
    for (size_t i = 0; i < MAX_SENSORS; i++)
    {
        sensors[i].configured = false;
    }

    // Check if file exists
    if (!LittleFS.exists(sensorsFile))
    {
        logger.info("Sensors file not found: " + String(sensorsFile) + ", starting with empty configuration");
        return true; // Not an error, we just start with an empty configuration
    }

    // Open file for reading
    File file = LittleFS.open(sensorsFile, "r");
    if (!file)
    {
        logger.error("Failed to open sensors file for reading: " + String(sensorsFile));
        return false;
    }

    // Create JSON document
    DynamicJsonDocument doc(4096); // Sufficiently large buffer

    // Deserialize JSON from file
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error)
    {
        logger.error("Failed to parse sensors file: " + String(error.c_str()));
        return false;
    }

    // Load sensors from JSON
    JsonArray sensorArray = doc["sensors"].as<JsonArray>();
    for (JsonObject sensorObj : sensorArray)
    {
        if (sensorCount < MAX_SENSORS)
        {
            uint8_t typeValue = sensorObj["deviceType"];
            SensorType deviceType = static_cast<SensorType>(typeValue);
            uint32_t serialNumber = sensorObj["serialNumber"];
            uint32_t deviceKey = sensorObj["deviceKey"];
            String name = sensorObj["name"].as<String>();
            String customUrl = sensorObj["customUrl"].as<String>();

            // Create sensor
            sensors[sensorCount].deviceType = deviceType;
            sensors[sensorCount].serialNumber = serialNumber;
            sensors[sensorCount].deviceKey = deviceKey;
            sensors[sensorCount].name = name;
            sensors[sensorCount].customUrl = customUrl;
            sensors[sensorCount].lastSeen = 0;
            sensors[sensorCount].temperature = 0.0f;
            sensors[sensorCount].humidity = 0.0f;
            sensors[sensorCount].pressure = 0.0f;
            sensors[sensorCount].ppm = 0.0f;
            sensors[sensorCount].lux = 0.0f;
            sensors[sensorCount].batteryVoltage = 0.0f;
            sensors[sensorCount].rssi = 0;
            sensors[sensorCount].configured = true;

            // Load daily rain total if it exists
            if (sensorObj.containsKey("dailyRainTotal"))
            {
                sensors[sensorCount].dailyRainTotal = sensorObj["dailyRainTotal"].as<float>();
            }
            else
            {
                sensors[sensorCount].dailyRainTotal = 0.0f;
            }

            // Load time of last reset if it exists
            if (sensorObj.containsKey("lastRainReset"))
            {
                sensors[sensorCount].lastRainReset = sensorObj["lastRainReset"].as<unsigned long>();
            }
            else
            {
                sensors[sensorCount].lastRainReset = 0;
            }

            if (sensorObj.containsKey("altitude"))
            {
                sensors[sensorCount].altitude = sensorObj["altitude"].as<int>();
            }
            else
            {
                sensors[sensorCount].altitude = 0;
            }

            if (sensorObj.containsKey("temperatureCorrection"))
            {
                sensors[sensorCount].temperatureCorrection = sensorObj["temperatureCorrection"].as<float>();
            }
            if (sensorObj.containsKey("humidityCorrection"))
            {
                sensors[sensorCount].humidityCorrection = sensorObj["humidityCorrection"].as<float>();
            }
            if (sensorObj.containsKey("pressureCorrection"))
            {
                sensors[sensorCount].pressureCorrection = sensorObj["pressureCorrection"].as<float>();
            }
            if (sensorObj.containsKey("ppmCorrection"))
            {
                sensors[sensorCount].ppmCorrection = sensorObj["ppmCorrection"].as<float>();
            }
            if (sensorObj.containsKey("luxCorrection"))
            {
                sensors[sensorCount].luxCorrection = sensorObj["luxCorrection"].as<float>();
            }
            if (sensorObj.containsKey("windSpeedCorrection"))
            {
                sensors[sensorCount].windSpeedCorrection = sensorObj["windSpeedCorrection"].as<float>();
            }
            if (sensorObj.containsKey("windDirectionCorrection"))
            {
                sensors[sensorCount].windDirectionCorrection = sensorObj["windDirectionCorrection"].as<int>();
            }
            if (sensorObj.containsKey("rainAmountCorrection"))
            {
                sensors[sensorCount].rainAmountCorrection = sensorObj["rainAmountCorrection"].as<float>();
            }
            if (sensorObj.containsKey("rainRateCorrection"))
            {
                sensors[sensorCount].rainRateCorrection = sensorObj["rainRateCorrection"].as<float>();
            }
            sensorCount++;
        }
        else
        {
            logger.warning("Too many sensors in configuration file, ignoring some");
            break;
        }
    }

    logger.info("Loaded " + String(sensorCount) + " sensors from configuration");
    return true;
}