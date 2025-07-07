/**
 * expLORA Gateway Lite
 *
 * Configuration manager implementation file
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

#include "ConfigManager.h"
#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// Constructor
ConfigManager::ConfigManager(Logger &log, const char *file)
    : logger(log), preferencesInitialized(false), configFile(file)
{

    // Default values
    resetToDefaults();
}

// Destructor
ConfigManager::~ConfigManager()
{
    // End Preferences if initialized
    if (preferencesInitialized)
    {
        preferences.end();
    }
}

// Initialization
bool ConfigManager::init()
{
    // Initialize Preferences
    preferencesInitialized = preferences.begin("sverio", false);

    if (!preferencesInitialized)
    {
        logger.error("Failed to initialize Preferences");
    }
    else
    {
        logger.debug("Preferences initialized successfully");
    }

    // Load configuration
    return load();
}

// Load configuration from both sources
bool ConfigManager::load()
{
    bool fsSuccess = loadFromFS();
    bool prefSuccess = loadFromPreferences();

    if (fsSuccess)
    {
        logger.info("Configuration loaded from file system");
    }

    if (prefSuccess)
    {
        logger.info("Persistent configuration loaded from Preferences");
    }

    return fsSuccess || prefSuccess;
}

// Save configuration to both sources
bool ConfigManager::save()
{
    bool fsSuccess = saveToFS();
    bool prefSuccess = saveToPreferences();

    if (fsSuccess)
    {
        logger.info("Configuration saved to file system");
    }
    else
    {
        logger.error("Failed to save configuration to file system");
    }

    if (prefSuccess)
    {
        logger.info("Persistent configuration saved to Preferences");
    }
    else
    {
        logger.error("Failed to save configuration to Preferences");
    }

    return fsSuccess && prefSuccess;
}

// Loading from LittleFS
bool ConfigManager::loadFromFS()
{
    // Check if file exists
    if (!LittleFS.exists(configFile))
    {
        logger.warning("Config file not found: " + String(configFile));
        return false;
    }

    // Open file
    File file = LittleFS.open(configFile, "r");
    if (!file)
    {
        logger.error("Failed to open config file for reading: " + String(configFile));
        return false;
    }

    // Log the raw content for debugging
    String fileContent = file.readString();
    logger.debug("Raw config file content: " + fileContent);
    file.close();

    // Re-open the file for parsing
    file = LittleFS.open(configFile, "r");

    // Create JSON document
    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error)
    {
        logger.error("Failed to parse config file: " + String(error.c_str()));
        return false;
    }

    // Load values and log them
    wifiSSID = doc["ssid"].as<String>();
    wifiPassword = doc["password"].as<String>();
    configMode = doc["configMode"] | true;
    timezone = doc["timezone"] | DEFAULT_TIMEZONE;

    // MQTT configuration
    mqttHost = doc["mqttHost"] | MQTT_DEFAULT_HOST;
    mqttPort = doc["mqttPort"] | MQTT_DEFAULT_PORT;
    mqttUser = doc["mqttUser"] | MQTT_DEFAULT_USER;
    mqttPassword = doc["mqttPassword"] | MQTT_DEFAULT_PASS;
    mqttEnabled = doc["mqttEnabled"] | MQTT_DEFAULT_ENABLED;
    mqttTls = doc["mqttTls"] | MQTT_DEFAULT_TLS;
    mqttPrefix = doc["mqttPrefix"] | MQTT_DEFAULT_PREFIX;
    mqttHAPrefix = doc["mqttHAPrefix"] | HA_DISCOVERY_DEFAULT_PREFIX;
    mqttHAEnabled = doc["mqttHAEnabled"] | HA_DISCOVERY_DEFAULT_ENABLED;

    logger.debug("Loaded config - SSID: " + wifiSSID +
                 ", Password length: " + String(wifiPassword.length()) +
                 ", configMode: " + String(configMode ? "true" : "false"));

    // Set log level if available
    if (doc.containsKey("logLevel"))
    {
        String logLevelStr = doc["logLevel"].as<String>();
        logLevel = logger.levelFromString(logLevelStr);
    }

    return true;
}

// Saving to LittleFS
bool ConfigManager::saveToFS()
{
    // Open file for writing
    File file = LittleFS.open(configFile, "w");
    if (!file)
    {
        logger.error("Failed to open config file for writing: " + String(configFile));
        return false;
    }

    // Create JSON document
    DynamicJsonDocument doc(512);
    doc["ssid"] = wifiSSID;
    doc["password"] = wifiPassword;
    doc["configMode"] = configMode;
    doc["logLevel"] = logger.levelToString(logLevel);
    doc["timezone"] = timezone;
    doc["mqttHost"] = mqttHost;
    doc["mqttPort"] = mqttPort;
    doc["mqttUser"] = mqttUser;
    doc["mqttPassword"] = mqttPassword;
    doc["mqttEnabled"] = mqttEnabled;
    doc["mqttTls"] = mqttTls;
    doc["mqttPrefix"] = mqttPrefix;
    doc["mqttHAEnabled"] = mqttHAEnabled;
    doc["mqttHAPrefix"] = mqttHAPrefix;

    // Serialize to file
    if (serializeJson(doc, file) == 0)
    {
        logger.error("Failed to write config to file");
        file.close();
        return false;
    }

    file.close();
    return true;
}

// Loading from Preferences
bool ConfigManager::loadFromPreferences()
{
    if (!preferencesInitialized)
    {
        return false;
    }

    // Load basic configuration
    // Note: We let Preferences load only logLevel,
    // which we really need to preserve during firmware updates

    if (preferences.isKey("logLevel"))
    {
        String logLevelStr = preferences.getString("logLevel", "INFO");
        logLevel = logger.levelFromString(logLevelStr);
    }

    if (preferences.isKey("timezone"))
    {
        timezone = preferences.getString("timezone", DEFAULT_TIMEZONE);
    }

    // MQTT configuration from preferences
    if (preferences.isKey("mqttHost"))
    {
        mqttHost = preferences.getString("mqttHost", MQTT_DEFAULT_HOST);
    }
    if (preferences.isKey("mqttPort"))
    {
        mqttPort = preferences.getInt("mqttPort", MQTT_DEFAULT_PORT);
    }
    if (preferences.isKey("mqttUser"))
    {
        mqttUser = preferences.getString("mqttUser", MQTT_DEFAULT_USER);
    }
    if (preferences.isKey("mqttPassword"))
    {
        mqttPassword = preferences.getString("mqttPassword", MQTT_DEFAULT_PASS);
    }
    mqttEnabled = preferences.getBool("mqttEnabled", MQTT_DEFAULT_ENABLED);
    mqttTls = preferences.getBool("mqttTls", MQTT_DEFAULT_TLS);
    mqttPrefix = preferences.getString("mqttPrefix", MQTT_DEFAULT_PREFIX);
    mqttHAEnabled = preferences.getBool("mqttHAEnabled", HA_DISCOVERY_DEFAULT_ENABLED);
    mqttHAPrefix = preferences.getString("mqttHAPrefix", HA_DISCOVERY_DEFAULT_PREFIX);

    // Other values should be loaded from LittleFS, but if needed,
    // we can load them from Preferences as a backup solution
    if (wifiSSID.length() == 0 && preferences.isKey("ssid"))
    {
        wifiSSID = preferences.getString("ssid", "");
        wifiPassword = preferences.getString("password", "");
        configMode = preferences.getBool("configMode", true);
    }

    return true;
}

// Saving to Preferences
bool ConfigManager::saveToPreferences()
{
    if (!preferencesInitialized)
    {
        return false;
    }

    // Save logging level
    String logLevelStr = logger.levelToString(logLevel);
    preferences.putString("logLevel", logLevelStr);
    preferences.putString("timezone", timezone);

    // Save MQTT configuration
    preferences.putString("mqttHost", mqttHost);
    preferences.putInt("mqttPort", mqttPort);
    preferences.putString("mqttUser", mqttUser);
    preferences.putString("mqttPassword", mqttPassword);
    preferences.putBool("mqttEnabled", mqttEnabled);
    preferences.putBool("mqttTls", mqttTls);
    preferences.putString("mqttPrefix", mqttPrefix);
    preferences.putBool("mqttHAEnabled", mqttHAEnabled);
    preferences.putString("mqttHAPrefix", mqttHAPrefix);

    // Save WiFi configuration (as backup)
    preferences.putString("ssid", wifiSSID);
    preferences.putString("password", wifiPassword);
    preferences.putBool("configMode", configMode);

    return true;
}

// Add a new method to set MQTT configuration
bool ConfigManager::setMqttConfig(const String &host, int port, const String &user,
                                  const String &password, bool enabled, bool tls,
                                  const String &rootPrefix, const String &haPrefix, bool haEnable,
                                  bool saveConfig)
{
    // Validate MQTT topic format (basic validation)
    if (!isValidMqttTopic(rootPrefix) || !isValidMqttTopic(haPrefix))
    {
        logger.error("Invalid MQTT topic format");
        return false;
    }

    mqttHost = host;
    mqttPort = port;
    mqttUser = user;
    mqttPassword = password;
    mqttEnabled = enabled;
    mqttTls = tls;
    mqttPrefix = rootPrefix;
    mqttHAPrefix = haPrefix;
    mqttHAEnabled = haEnable;

    return saveConfig ? save() : true;
}

// Add a new method to set the timezone
bool ConfigManager::setTimezone(const String &newTimezone, bool saveConfig)
{
    timezone = newTimezone;

    // Configure the time with new timezone
    configTime(0, 0, NTP_SERVER);      // First set to UTC
    setenv("TZ", timezone.c_str(), 1); // Set the TZ environment variable
    tzset();                           // Apply the time zone

    if (saveConfig)
    {
        return save();
    }

    return true;
}

// Helper function to validate MQTT topic format
bool ConfigManager::isValidMqttTopic(const String &topic)
{
    // Basic MQTT topic validation
    if (topic.length() == 0 || topic.length() > 127)
        return false;
    
    // Check for invalid characters
    if (topic.indexOf('#') != -1 || topic.indexOf('+') != -1)
        return false;
    
    // Check for null character
    if (topic.indexOf('\0') != -1)
        return false;
    
    return true;
}

// Reset configuration to default values
void ConfigManager::resetToDefaults()
{
    wifiSSID = "";
    wifiPassword = "";
    configMode = true;
    lastWifiAttempt = 0;
    logLevel = LogLevel::INFO;
    timezone = DEFAULT_TIMEZONE;
    mqttHost = MQTT_DEFAULT_HOST;
    mqttPort = MQTT_DEFAULT_PORT;
    mqttUser = MQTT_DEFAULT_USER;
    mqttPassword = MQTT_DEFAULT_PASS;
    mqttEnabled = MQTT_DEFAULT_ENABLED;
    mqttPrefix = MQTT_DEFAULT_PREFIX;
    mqttHAEnabled = HA_DISCOVERY_DEFAULT_ENABLED;
    mqttHAPrefix = HA_DISCOVERY_DEFAULT_PREFIX;
}

// Get firmware version
String ConfigManager::getFirmwareVersion() const
{
    return FIRMWARE_VERSION;
}

// Get device MAC address
String ConfigManager::getMacAddress() const
{
    uint8_t mac[6];
    WiFi.macAddress(mac);

    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    return String(macStr);
}

// Get device name for AP mode
String ConfigManager::getDeviceName() const
{
    uint8_t mac[6];
    WiFi.macAddress(mac);

    String deviceName = "expLORA-GW-";
    for (int i = 0; i < 6; i++)
    {
        if (mac[i] < 16)
            deviceName += "0";
        deviceName += String(mac[i], HEX);
    }

    String result = deviceName;
    result.toUpperCase();
    return result;
}

// Set WiFi configuration
bool ConfigManager::setWiFiConfig(const String &ssid, const String &password, bool saveConfig)
{
    wifiSSID = ssid;
    wifiPassword = password;
    configMode = false;

    if (saveConfig)
    {
        return save();
    }

    return true;
}

// Switch to configuration mode (AP)
void ConfigManager::enableConfigMode(bool enable, bool saveConfig)
{
    configMode = enable;

    if (saveConfig)
    {
        save();
    }
}

// Set logging level
void ConfigManager::setLogLevel(LogLevel level, bool saveConfig)
{
    logLevel = level;

    // Update level in logger
    logger.setLogLevel(level);

    if (saveConfig)
    {
        save();
    }
}
