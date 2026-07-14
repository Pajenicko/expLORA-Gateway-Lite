/**
 * expLORA Gateway Lite
 *
 * Sensor data structure header file
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
#include <ArduinoJson.h>
#include "SensorTypes.h"
#include "SensorHealth.h"

/**
 * Structure for storing sensor data
 *
 * This structure stores both configuration data and current measurement values
 * from sensors. Thanks to integration with SensorTypes, it allows flexible
 * handling of different sensor types and their specific data.
 */
struct SensorData
{
    // Basic sensor identification
    SensorType deviceType; // Device type
    uint32_t serialNumber; // Serial number
    uint32_t deviceKey;    // Key for data decryption
    String name;           // User-defined sensor name

    // Extended configuration
    String customUrl; // Complete URL with placeholders
    int altitude;     // Altitude (m) - for BME280

    // Sensor status
    unsigned long lastSeen; // Time of last seen packet
    bool configured;        // Whether the sensor is configured

    // General sensor data - values are valid according to device type
    float temperature; // Temperature (°C) - BME280, SCD40
    float humidity;    // Humidity (%) - BME280, SCD40
    float pressure;    // Pressure (hPa) - BME280
    float ppm;         // CO2 concentration (ppm) - SCD40
    float lux;         // Light intensity (lux) - VEML7700

    // New variables for METEO sensor
    float windSpeed;             // Wind speed (m/s)
    uint16_t windDirection;      // Wind direction (degrees)
    float rainAmount;            // Rain amount (mm)
    float rainRate;              // Rain intensity (mm/h)
    float dailyRainTotal;        // Daily precipitation total (mm) - resets at midnight
    unsigned long lastRainReset; // Timestamp of last daily total reset

    float temperatureCorrection; // Correction offset for temperature
    float humidityCorrection;    // Correction offset for humidity
    float pressureCorrection;    // Correction offset for pressure
    float ppmCorrection;         // Correction offset for CO2 concentration
    float luxCorrection;         // Correction offset for light intensity
    float windSpeedCorrection;   // Correction factor for wind speed (multiplier)
    int windDirectionCorrection; // Correction offset for wind direction (degrees)
    float rainAmountCorrection;  // Correction factor for rain amount (multiplier)
    float rainRateCorrection;    // Correction factor for rain rate (multiplier)

    // General device data
    float batteryVoltage; // Battery voltage (V)
    int rssi;             // Signal strength (dBm)

    // Per-sensor health tracking (rejection counters, streaks, 24h window).
    // Updated by SensorManager::recordSensor{Success,Rejection}/tickSensorHealth.
    SensorHealth::Stats health;

    // Constructor for initialization with default values
    SensorData() : deviceType(SensorType::UNKNOWN),
                   serialNumber(0),
                   deviceKey(0),
                   name(""),
                   customUrl(""),
                   altitude(0),
                   lastSeen(0),
                   configured(false),
                   temperature(0.0f),
                   humidity(0.0f),
                   pressure(0.0f),
                   ppm(0.0f),
                   lux(0.0f),
                   batteryVoltage(0.0f),
                   rssi(0),
                   windSpeed(0.0f),
                   windDirection(0),
                   rainAmount(0.0f),
                   rainRate(0.0f),
                   dailyRainTotal(0.0f),
                   lastRainReset(0),
                   temperatureCorrection(0.0f),
                   humidityCorrection(0.0f),
                   pressureCorrection(0.0f),
                   ppmCorrection(0.0f),
                   luxCorrection(0.0f),
                   windSpeedCorrection(1.0f), // Multiplier of 1.0 = no change
                   windDirectionCorrection(0),
                   rainAmountCorrection(1.0f),
                   rainRateCorrection(1.0f)
    {
    }

    // Helper methods to verify if the sensor provides a specific type of data
    bool hasTemperature() const
    {
        return getSensorTypeInfo(deviceType).hasTemperature;
    }

    bool hasHumidity() const
    {
        return getSensorTypeInfo(deviceType).hasHumidity;
    }

    bool hasPressure() const
    {
        return getSensorTypeInfo(deviceType).hasPressure;
    }

    bool hasPPM() const
    {
        return getSensorTypeInfo(deviceType).hasPPM;
    }

    bool hasLux() const
    {
        return getSensorTypeInfo(deviceType).hasLux;
    }

    // Helper methods to verify if the sensor provides a specific type of data
    bool hasWindSpeed() const
    {
        return getSensorTypeInfo(deviceType).hasWindSpeed;
    }

    bool hasWindDirection() const
    {
        return getSensorTypeInfo(deviceType).hasWindDirection;
    }

    bool hasRainAmount() const
    {
        return getSensorTypeInfo(deviceType).hasRainAmount;
    }

    bool hasRainRate() const
    {
        return getSensorTypeInfo(deviceType).hasRainRate;
    }

    // Get information about sensor type
    const SensorTypeInfo &getTypeInfo() const
    {
        return getSensorTypeInfo(deviceType);
    }

    // Convert to JSON for web API and storage
    void toJson(JsonObject &json) const
    {
        json["deviceType"] = static_cast<uint8_t>(deviceType);
        json["typeName"] = getTypeInfo().name;
        json["serialNumber"] = formatSN(serialNumber);
        json["name"] = name;

        // Add data specific to sensor type with validity check
        // if (lastSeen > 0) {
        if (hasTemperature())
        {
            json["temperature"] = round(temperature * 100) / 100.0;
        }

        if (hasHumidity())
        {
            json["humidity"] = round(humidity * 100) / 100.0;
        }

        if (hasPressure())
        {
            json["pressure"] = round(pressure * 100) / 100.0;
        }

        if (hasPPM())
        {
            json["ppm"] = round(ppm);
        }

        if (hasLux())
        {
            json["lux"] = round(lux * 10) / 10.0;
        }

        if (hasWindSpeed())
        {
            json["windSpeed"] = round(windSpeed * 10) / 10.0;
        }

        if (hasWindDirection())
        {
            json["windDirection"] = windDirection;
        }

        if (hasRainAmount())
        {
            json["rainAmount"] = round(rainAmount * 100) / 100.0;
            json["dailyRainTotal"] = round(dailyRainTotal * 100) / 100.0;
        }

        if (hasRainRate())
        {
            json["rainRate"] = round(rainRate * 100) / 100.0;
        }

        json["batteryVoltage"] = round(batteryVoltage * 100) / 100.0;
        json["rssi"] = rssi;

        if (lastSeen > 0)
        {
            json["lastSeen"] = (millis() - lastSeen) / 1000; // seconds since last seen
        }
        else
        {
            json["lastSeen"] = -1;
        }
    }

    // Format data for web display
    String getDataString() const
    {
        String dataStr = "";
        bool first = true;

        if (lastSeen == 0)
        {
            return "-";
        }

        if (hasTemperature())
        {
            if (!first)
                dataStr += ", ";
            dataStr += String(temperature, 2) + " °C";
            first = false;
        }

        if (hasHumidity())
        {
            if (!first)
                dataStr += ", ";
            dataStr += String(humidity, 2) + " %";
            first = false;
        }

        if (hasPressure())
        {
            if (!first)
                dataStr += ", ";
            dataStr += String(pressure, 2) + " hPa";
            first = false;
        }

        if (hasPPM())
        {
            if (!first)
                dataStr += ", ";
            dataStr += String(int(ppm)) + " ppm CO2";
            first = false;
        }

        if (hasLux())
        {
            if (!first)
                dataStr += ", ";
            dataStr += String(lux, 1) + " lux";
            first = false;
        }

        if (hasWindSpeed())
        {
            if (!first)
                dataStr += ", ";
            dataStr += String(windSpeed, 1) + " m/s";
            first = false;
        }

        if (hasWindDirection())
        {
            if (!first)
                dataStr += ", ";
            dataStr += String(windDirection) + "°";
            first = false;
        }

        if (hasRainAmount())
        {
            if (!first)
                dataStr += ", ";
            dataStr += String(rainAmount, 1) + " mm";
            dataStr += " (daily precipitation total: " + String(dailyRainTotal, 1) + " mm)";
            first = false;
        }

        if (hasRainRate())
        {
            if (!first)
                dataStr += ", ";
            dataStr += String(rainRate, 1) + " mm/h";
            first = false;
        }

        // Add battery voltage
        if (!first)
            dataStr += ", ";
        dataStr += String(batteryVoltage, 2) + " V";

        return dataStr;
    }

    // Format time of last seen packet
    String getLastSeenString() const
    {
        if (lastSeen == 0)
        {
            return "Never";
        }

        unsigned long seconds = (millis() - lastSeen) / 1000;
        if (seconds < 60)
        {
            return String(seconds) + " seconds ago";
        }
        else if (seconds < 3600)
        {
            return String(seconds / 60) + " minutes ago";
        }
        else
        {
            return String(seconds / 3600) + " hours ago";
        }
    }
};

struct ActiveSensorEntry {
    size_t       index;   // real index in the sensors array
    SensorData   data;
};
