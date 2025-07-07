/**
 * expLORA Gateway Lite
 *
 * Configuration manager header file
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
#include <Preferences.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include "../Data/Logging.h"
#include "../config.h"

/**
 * Class for device configuration management
 *
 * Handles saving and loading configuration from two sources:
 * 1. LittleFS - for regular configuration that is reset during firmware updates
 * 2. Preferences - for persistent configuration that survives firmware updates
 */
class ConfigManager
{
private:
    Logger &logger;              // Reference to logger
    Preferences preferences;     // For persistent configuration
    bool preferencesInitialized; // Preferences initialization flag

    // Configuration files
    const char *configFile; // Configuration file

    // Loading and saving from/to LittleFS
    bool loadFromFS();
    bool saveToFS();

    // Loading and saving from/to Preferences
    bool loadFromPreferences();
    bool saveToPreferences();

public:
    // Device configuration
    String wifiSSID;               // WiFi network SSID
    String wifiPassword;           // WiFi network password
    bool configMode;               // Configuration mode (AP mode)
    unsigned long lastWifiAttempt; // Time of last WiFi connection attempt
    LogLevel logLevel;             // Logging level
    String timezone;               // Timezone in Posix format

    // MQTT Configuration
    String mqttHost;     // MQTT broker hostname
    int mqttPort;        // MQTT broker port
    String mqttUser;     // MQTT username
    String mqttPassword; // MQTT password
    bool mqttEnabled;    // MQTT enabled flag
    bool mqttTls;        // MQTTS flag
    String mqttPrefix;   // MQTT Topic prefix
    String mqttHAPrefix; // MQTT Homeassistant topic prefix
    bool mqttHAEnabled;  // MQTT Homeassistant discovery enable

    // Constructor
    ConfigManager(Logger &log, const char *file = CONFIG_FILE);

    // Destructor
    ~ConfigManager();

    // Initialization
    bool init();

    // Load configuration from both sources
    bool load();

    // Save configuration to both sources
    bool save();

    // Reset configuration to default values
    void resetToDefaults();

    // Get firmware version
    String getFirmwareVersion() const;

    // Get device MAC address
    String getMacAddress() const;

    // Get device name for AP mode
    String getDeviceName() const;

    // Set WiFi configuration
    bool setWiFiConfig(const String &ssid, const String &password, bool saveConfig = true);

    // Switch to configuration mode (AP)
    void enableConfigMode(bool enable = true, bool saveConfig = true);

    // Set MQTT configuration
    bool setMqttConfig(const String &host, int port, const String &user,
                       const String &password, bool enabled, bool tls,
                       const String &rootPrefix, const String &haPrefix, bool haEnable,
                       bool saveConfig = true);

    // Set logging level
    void setLogLevel(LogLevel level, bool saveConfig = true);

    // Set timezone
    bool setTimezone(const String &newTimezone, bool saveConfig = true);

private:
    // Helper function to validate MQTT topic format
    bool isValidMqttTopic(const String &topic);
};
