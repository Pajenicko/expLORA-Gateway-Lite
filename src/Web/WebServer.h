/**
 * expLORA Gateway Lite
 *
 * Web portal manager header file
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
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "../Data/SensorManager.h"
#include "../Data/Logging.h"
#include "../Storage/ConfigManager.h"
#include "../Protocol/MQTTManager.h"
#include "OTAServer.h"
#include "../Hardware/Network_Manager.h"

/**
 * Class for web portal management
 *
 * Provides a web interface for device configuration and monitoring.
 * Offers API for accessing sensor data.
 */
class WebPortal
{
private:
    // Task handle for web server
    // TaskHandle_t webServerTaskHandle = NULL;

    MQTTManager *mqttManager; // Reference to MQTT manager

    // Static task function for the second core
    static void webServerTask(void *parameter);

    // Processing flag to avoid race conditions
    volatile bool isProcessing = false;

    AsyncWebServer server;        // Asynchronous web server
    SensorManager &sensorManager; // Reference to sensor manager
    Logger &logger;               // Reference to logger
    OTAServer *otaServer;         // OTA Server

    NetworkManager &networkManager; // Reference to network manager

    bool isAPMode; // AP mode (true) or client mode (false)
    String apName; // AP name in AP mode

    // Device configuration - reference to external configuration
    String &wifiSSID;
    String &wifiPassword;
    bool &configMode;
    String &timezone;

    ConfigManager &configManager; // Reference to configuration

    // Setup routes for web server
    void setupRoutes();

    // Create content for individual pages
    String createHomePage();
    String createConfigPage();
    String createSensorsPage();
    String createSensorAddPage();
    String createSensorEditPage(int index);
    String createLogsPage();
    String createAPIPage();

    // Process HTTP requests
    void handleRoot(AsyncWebServerRequest *request);
    void handleConfig(AsyncWebServerRequest *request);
    void handleConfigPost(AsyncWebServerRequest *request);
    void handleSensors(AsyncWebServerRequest *request);
    void handleSensorAdd(AsyncWebServerRequest *request);
    void handleSensorAddPost(AsyncWebServerRequest *request);
    void handleSensorEdit(AsyncWebServerRequest *request);
    void handleSensorEditPost(AsyncWebServerRequest *request);
    void handleSensorDelete(AsyncWebServerRequest *request);
    void handleLogs(AsyncWebServerRequest *request);
    void handleLogsClear(AsyncWebServerRequest *request);
    void handleLogLevel(AsyncWebServerRequest *request);
    void handleAPI(AsyncWebServerRequest *request);
    void handleMqtt(AsyncWebServerRequest *request);
    void handleMqttPost(AsyncWebServerRequest *request);
    void handleReboot(AsyncWebServerRequest *request);
    void handleNotFound(AsyncWebServerRequest *request);

    // Receive WebSocket messages
    void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                          AwsEventType type, void *arg, uint8_t *data, size_t len);

    // Send updates via WebSocket
    void sendSensorUpdates();
    void sendLogUpdates();

public:
    // Constructor
    WebPortal(SensorManager &sensors, Logger &log, String &ssid, String &password,
              bool &configMode, ConfigManager &config, NetworkManager &nm, String &timezone);
    // Destructor
    ~WebPortal();

    // Web server initialization
    bool init();

    // Handle requests
    void handleClient();

    // Process DNS requests (for captive portal)
    void processDNS();

    // Get current mode
    bool isInAPMode() const;

    // Restart web server
    void restart();

    // Set MQTTManager reference
    void setMqttManager(MQTTManager *manager) { mqttManager = manager; }
};
