/**
 * expLORA Gateway Lite
 *
 * MQTT communication manager header file
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
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <vector>
#include "Data/SensorManager.h"
#include "Data/Logging.h"
#include "Storage/ConfigManager.h"
#include "Hardware/Network_Manager.h"

/**
 * Class for managing MQTT communication with Home Assistant
 *
 * Handles automatic sensor detection in Home Assistant using MQTT discovery
 * and regular publishing of sensor data.
 */
class MQTTManager
{
private:
    WiFiClient wifiClient;              // WiFi client for MQTT
    WiFiClientSecure wifiClientSecure;  // WiFi client for MQTTS
    PubSubClient mqttClient;            // MQTT client
    SensorManager &sensorManager;       // Reference to sensor manager
    ConfigManager &configManager;       // Reference to configuration
    Logger &logger;                     // Reference to logger
    NetworkManager &networkManager;     // Reference to network manager

    String clientId;                    // MQTT Client ID
    unsigned long lastReconnectAttempt; // Time of last connection attempt
    unsigned long lastDiscoveryUpdate;  // Time of last discovery update

    // Connect to MQTT broker
    bool connect();

    // Create discovery topic for sensor
    String buildDiscoveryTopic(const SensorData &sensor, const String &valueType);

    // Create configuration JSON for Home Assistant discovery
    String buildDiscoveryJson(const SensorData &sensor, const String &valueType, const String &stateTopic);

    // Helper function to capitalize first letter
    String capitalizeFirst(const String &input);

public:
    // Constructor
    MQTTManager(SensorManager &sensors, ConfigManager &config, Logger &log, NetworkManager &nm);

    // Initialization
    bool init();

    // Process MQTT communication (call in main loop)
    void process();

    // Publish discovery configuration for sensors
    void publishDiscovery();

    // Publish sensor data
    void publishSensorData(int sensorIndex);

    // Publish discovery for specific sensor
    void publishDiscoveryForSensor(int sensorIndex);

    // Remove discovery for deleted sensor
    void removeDiscoveryForSensor(uint32_t serialNumber);

    // Check connection
    bool isConnected();

    // Disconnect from MQTT broker
    void disconnect();
};
