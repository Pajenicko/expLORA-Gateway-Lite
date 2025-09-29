/**
 * expLORA Gateway Lite
 *
 * Web portal implementation file
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

#include "WebServer.h"
#include "HTMLGenerator.h"
#include <WiFi.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <esp_ota_ops.h>
#include <esp_task_wdt.h>
#include <esp_heap_caps.h>
#include <LittleFS.h>
#include <HTTPClient.h>
#include <Update.h>
#include "../config.h"

// Constructor
WebPortal::WebPortal(SensorManager &sensors, Logger &log, String &ssid, String &password,
                     bool &config_mode, ConfigManager &config, String &tz)
    : server(HTTP_PORT), sensorManager(sensors), logger(log), isAPMode(false),
      wifiSSID(ssid), wifiPassword(password), configMode(config_mode),
      timezone(tz), configManager(config), mqttManager(nullptr)
{
}

// Add this static task function to WebPortal in WebServer.cpp
// void WebPortal::webServerTask(void *parameter) {
//     WebPortal *server = (WebPortal*)parameter;

//     for (;;) {
//         if (server->isAPMode) {
//             // Process DNS requests (captive portal)
//             server->dnsServer.processNextRequest();
//         }

//         // Give other tasks time to run
//         vTaskDelay(1);
//     }
// }

// Modify the init function to create the task
bool WebPortal::init()
{
    logger.info("Initializing web portal");

    // Create AP name (if needed)
    uint8_t mac[6];
    WiFi.macAddress(mac);

    String macAddress = WiFi.macAddress();
    macAddress.replace(":", "");                      // Remove colons
    apName = "expLORA-GW-" + macAddress.substring(6); // Use last 6 characters of MAC address

    // Set mode based on current configuration
    isAPMode = configMode || (WiFi.status() != WL_CONNECTED);

    if (isAPMode)
    {
        setupAP();
    }

    // Setup routes (idempotent)
    setupRoutes();

    // Start server once
    if (!serverStarted)
    {
        server.begin();
        serverStarted = true;
        logger.info("Web server started on port " + String(HTTP_PORT));
    }


    // Create task on core 0 for DNS and other processing
    // xTaskCreatePinnedToCore(
    //     webServerTask,        // Task function
    //     "WebServerTask",      // Task name
    //     4096,                 // Stack size (bytes)
    //     this,                 // Parameter to pass
    //     1,                    // Task priority (1 is low)
    //     &webServerTaskHandle, // Task handle
    //     0                     // Run on core 0
    // );

    // logger.info("Web server task created on core 0");

    return true;
}

// AP mode initialization
void WebPortal::setupAP()
{
    logger.info("Starting AP mode: " + apName);

    // Full WiFi reset sequence
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(200);

    // AP configuration with optimized parameters
    WiFi.setTxPower(WIFI_POWER_19_5dBm); // Maximum power

    // Set up AP mode with optimized settings
    WiFi.mode(WIFI_AP);
    logger.info("Setting up AP: " + apName);

    // AP configuration with 4 clients max and channel 6 (less crowded usually)
    bool apStarted = WiFi.softAP(apName.c_str());
    delay(1000); // Give AP time to fully initialize

    if (apStarted)
    {
        logger.info("AP setup successful");
    }
    else
    {
        logger.error("AP setup failed");
    }

    IPAddress apIP = WiFi.softAPIP();
    logger.info("AP IP assigned: " + apIP.toString());

    // Configure DNS server with custom TTL for faster responses
    dnsServer.setTTL(30); // TTL in seconds (lower value = more responsive)
    dnsServer.start(DNS_PORT, "*", apIP);
    logger.info("DNS server started on port " + String(DNS_PORT));

    isAPMode = true;
}

// Setup routes for web server
// Modify WebPortal::setupRoutes() in WebServer.cpp
void WebPortal::setupRoutes()
{
    if (routesRegistered)
    {
        return;
    }

    logger.info("Registering web server routes (once)");

    server.on("/", HTTP_GET, std::bind(&WebPortal::handleRoot, this, std::placeholders::_1));
    server.on("/config", HTTP_GET, std::bind(&WebPortal::handleConfig, this, std::placeholders::_1));
    server.on("/config", HTTP_POST, std::bind(&WebPortal::handleConfigPost, this, std::placeholders::_1));

    // Sensor management
    server.on("/sensors/add", HTTP_GET, std::bind(&WebPortal::handleSensorAdd, this, std::placeholders::_1));
    server.on("/sensors/add", HTTP_POST, std::bind(&WebPortal::handleSensorAddPost, this, std::placeholders::_1));
    server.on("/sensors/edit", HTTP_GET, std::bind(&WebPortal::handleSensorEdit, this, std::placeholders::_1));
    server.on("/sensors/update", HTTP_POST, std::bind(&WebPortal::handleSensorEditPost, this, std::placeholders::_1));
    server.on("/sensors/delete", HTTP_GET, std::bind(&WebPortal::handleSensorDelete, this, std::placeholders::_1));
    server.on("/sensors", HTTP_GET, std::bind(&WebPortal::handleSensors, this, std::placeholders::_1));

    // Logs
    server.on("/logs/clear", HTTP_GET, std::bind(&WebPortal::handleLogsClear, this, std::placeholders::_1));
    server.on("/logs/level", HTTP_POST, std::bind(&WebPortal::handleLogLevel, this, std::placeholders::_1));
    server.on("/logs", HTTP_GET, std::bind(&WebPortal::handleLogs, this, std::placeholders::_1));

    // MQTT
    server.on("/mqtt", HTTP_GET, std::bind(&WebPortal::handleMqtt, this, std::placeholders::_1));
    server.on("/mqtt", HTTP_POST, std::bind(&WebPortal::handleMqttPost, this, std::placeholders::_1));

    // API
    server.on("/api", HTTP_GET, std::bind(&WebPortal::handleAPI, this, std::placeholders::_1));

    // Diagnostics
    server.on("/diag/heap", HTTP_GET, std::bind(&WebPortal::handleDiagHeap, this, std::placeholders::_1));

    // Firmware update endpoints
    server.on("/firmware/version", HTTP_GET, std::bind(&WebPortal::handleFirmwareVersion, this, std::placeholders::_1));
    server.on("/firmware/check", HTTP_GET, std::bind(&WebPortal::handleFirmwareCheck, this, std::placeholders::_1));
    server.on("/firmware/update", HTTP_POST, std::bind(&WebPortal::handleFirmwareUpdate, this, std::placeholders::_1));
    server.on("/firmware", HTTP_GET, std::bind(&WebPortal::handleFirmwarePage, this, std::placeholders::_1));
    server.on("/firmware", HTTP_POST,
        [this](AsyncWebServerRequest *request) { this->handleFirmwareUploadComplete(request); },
        [this](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
            this->handleFirmwareUploadChunk(request, filename, index, data, len, final);
        }
    );

    // Reboot
    server.on("/reboot", HTTP_GET, std::bind(&WebPortal::handleReboot, this, std::placeholders::_1));

    // Unknown pages (404)
    server.onNotFound(std::bind(&WebPortal::handleNotFound, this, std::placeholders::_1));

    routesRegistered = true;
    logger.info("Routes registered");
}

// Modify the handleClient method to avoid duplicate processing
void WebPortal::handleClient()
{
    // No need to do anything here since the task handles it
    // We keep this method for compatibility

    // This will handle auto-reboot after OTA process
}

// Ensure proper cleanup in the destructor
WebPortal::~WebPortal()
{
    // Stop the task if it's running
    // if (webServerTaskHandle != NULL) {
    //    vTaskDelete(webServerTaskHandle);
    //}

    // Stop DNS server
    dnsServer.stop();
}

// Process DNS requests
void WebPortal::processDNS()
{
    dnsServer.processNextRequest();
    // called automatically in handleClient()
}

// Switch between AP and client modes
void WebPortal::setAPMode(bool enable)
{
    if (enable == isAPMode)
    {
        return; // No change
    }

    if (enable)
    {
        // Switch to AP mode
        setupAP();
    }
    else
    {
        // Switch to client mode
        dnsServer.stop();
        isAPMode = false;
    }
}

// Get current mode
bool WebPortal::isInAPMode() const
{
    return isAPMode;
}

// Restart web server
void WebPortal::restart()
{
    server.end();
    serverStarted = false;

    // Reset DNS server if it was running
    if (isAPMode)
    {
        dnsServer.stop();
    }

    // Re-initialize
    init();
}

// Process HTTP requests

// Root page
void WebPortal::handleRoot(AsyncWebServerRequest *request)
{
    logger.debug("HTTP request: GET /");

    // In AP mode, redirect to config page
    if (isAPMode)
    {
        request->redirect("/config");
        return;
    }

    // Get list of sensors
    std::vector<SensorData> sensorsList = sensorManager.getActiveSensors();

    // Generate HTML
    String html = HTMLGenerator::generateHomePage(sensorsList);

    // Send response
    request->send(200, "text/html", html);
}

// Guard: if AP mode, redirect to /config
bool WebPortal::guardAPOnly(AsyncWebServerRequest *request)
{
    if (isAPMode)
    {
        request->redirect("/config");
        return true;
    }
    return false;
}

// WiFi configuration page
void WebPortal::handleConfig(AsyncWebServerRequest *request)
{
    logger.debug("HTTP request: GET /config");

    String currentIP = isAPMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();

    // Generate HTML (use isAPMode to drive UI state)
    String html = HTMLGenerator::generateConfigPage(wifiSSID, wifiPassword, isAPMode, currentIP, timezone);

    // Send response
    request->send(200, "text/html", html);
}

// Process WiFi configuration form
void WebPortal::handleConfigPost(AsyncWebServerRequest *request)
{
    logger.debug("HTTP request: POST /config");

    if (request->hasParam("ssid", true) && request->hasParam("password", true))
    {
        wifiSSID = request->getParam("ssid", true)->value();
        wifiPassword = request->getParam("password", true)->value();

        // Get timezone if present
        if (request->hasParam("timezone", true))
        {
            String newTimezone = request->getParam("timezone", true)->value();
            timezone = newTimezone;
        }

        logger.info("New WiFi configuration - SSID: " + wifiSSID);

        // IMPORTANT: Make sure to save both to ConfigManager and to Preferences
        // This ensures config persists across reboots
        configMode = false;

        // Since you have direct reference to these variables, you need to save them
        // to persistent storage before restarting
        // Add this code that explicitly saves to both file system and preferences

        // Save to HTML response
        String html = "<!DOCTYPE html><html><head>";
        html += "<meta http-equiv='refresh' content='10;url=/'>";
        html += "<title>Configuration Saved</title></head>";
        html += "<body><h1>Configuration Saved</h1>";
        html += "<p>New WiFi settings have been saved. The device will restart in a few seconds.</p>";
        html += "</body></html>";

        request->send(200, "text/html", html);

        // Make sure configMode is saved as FALSE
        // This is critical to ensure it doesn't start in AP mode after restart
        File file = LittleFS.open(CONFIG_FILE, "w");
        if (file)
        {
            DynamicJsonDocument doc(512);
            doc["ssid"] = wifiSSID;
            doc["password"] = wifiPassword;
            doc["configMode"] = false; // Explicitly set to false
            doc["timezone"] = timezone;
            serializeJson(doc, file);
            file.close();
            logger.info("Configuration saved to file system");
        }

        // Restart ESP32 after 1 second
        delay(1000);
        ESP.restart();
    }
    else
    {
        // Missing parameters
        request->send(400, "text/plain", "Missing parameters");
    }
}

// Sensors list page
void WebPortal::handleSensors(AsyncWebServerRequest *request)
{
    logger.debug("HTTP request: GET /sensors");
    if (guardAPOnly(request)) return;

    // Get list of sensors
    std::vector<ActiveSensorEntry> sensorsList = sensorManager.getActiveSensorEntries();

    // Generate HTML
    String html = HTMLGenerator::generateSensorsPage(sensorsList);

    // Send response
    request->send(200, "text/html", html);
}

// Sensor add page
void WebPortal::handleSensorAdd(AsyncWebServerRequest *request)
{
    logger.debug("HTTP request: GET /sensors/add");
    if (guardAPOnly(request)) return;

    // Generate HTML
    String html = HTMLGenerator::generateSensorAddPage();

    // Send response
    request->send(200, "text/html", html);
}

// Process sensor add form
void WebPortal::handleSensorAddPost(AsyncWebServerRequest *request)
{
    logger.debug("HTTP request: POST /sensors/add");
    if (guardAPOnly(request)) return;

    // Check parameters
    if (request->hasParam("name", true) &&
        request->hasParam("deviceType", true) &&
        request->hasParam("serialNumber", true) &&
        request->hasParam("deviceKey", true))
    {

        // Get form data
        String name = request->getParam("name", true)->value();
        uint8_t deviceTypeValue = request->getParam("deviceType", true)->value().toInt();
        String serialNumberHex = request->getParam("serialNumber", true)->value();
        String deviceKeyHex = request->getParam("deviceKey", true)->value();
        String customUrl = request->hasParam("customUrl", true) ? request->getParam("customUrl", true)->value() : "";
        int altitude = 0;
        if (request->hasParam("altitude", true))
        {
            altitude = request->getParam("altitude", true)->value().toInt();
        }

        // Convert values
        SensorType deviceType = static_cast<SensorType>(deviceTypeValue);
        uint32_t serialNumber = strtoul(serialNumberHex.c_str(), NULL, 16);
        uint32_t deviceKey = strtoul(deviceKeyHex.c_str(), NULL, 16);

        // Get correction values with defaults if not present
        float tempCorr = 0.0f;
        float humCorr = 0.0f;
        float pressCorr = 0.0f;
        float ppmCorr = 0.0f;
        float luxCorr = 0.0f;
        float windSpeedCorr = 1.0f;
        int windDirCorr = 0;
        float rainAmountCorr = 1.0f;
        float rainRateCorr = 1.0f;

        if (request->hasParam("tempCorr", true))
        {
            tempCorr = request->getParam("tempCorr", true)->value().toFloat();
        }
        if (request->hasParam("humCorr", true))
        {
            humCorr = request->getParam("humCorr", true)->value().toFloat();
        }
        if (request->hasParam("pressCorr", true))
        {
            pressCorr = request->getParam("pressCorr", true)->value().toFloat();
        }
        if (request->hasParam("ppmCorr", true))
        {
            ppmCorr = request->getParam("ppmCorr", true)->value().toFloat();
        }
        if (request->hasParam("luxCorr", true))
        {
            luxCorr = request->getParam("luxCorr", true)->value().toFloat();
        }
        if (request->hasParam("windSpeedCorr", true))
        {
            windSpeedCorr = request->getParam("windSpeedCorr", true)->value().toFloat();
        }
        if (request->hasParam("windDirCorr", true))
        {
            windDirCorr = request->getParam("windDirCorr", true)->value().toInt();
        }
        if (request->hasParam("rainAmountCorr", true))
        {
            rainAmountCorr = request->getParam("rainAmountCorr", true)->value().toFloat();
        }
        if (request->hasParam("rainRateCorr", true))
        {
            rainRateCorr = request->getParam("rainRateCorr", true)->value().toFloat();
        }

        // Add sensor
        int sensorIndex = sensorManager.addSensor(deviceType, serialNumber, deviceKey, name);

        if (sensorIndex >= 0)
        {
            // Get added sensor and update additional data
            SensorData *sensor = sensorManager.getSensor(sensorIndex);
            if (sensor)
            {
                sensor->customUrl = customUrl;
                sensor->altitude = altitude;

                // Set correction values
                sensor->temperatureCorrection = tempCorr;
                sensor->humidityCorrection = humCorr;
                sensor->pressureCorrection = pressCorr;
                sensor->ppmCorrection = ppmCorr;
                sensor->luxCorrection = luxCorr;
                sensor->windSpeedCorrection = windSpeedCorr;
                sensor->windDirectionCorrection = windDirCorr;
                sensor->rainAmountCorrection = rainAmountCorr;
                sensor->rainRateCorrection = rainRateCorr;

                sensorManager.saveSensors(true);

                logger.info("Added new sensor: " + name + " (SN: " + serialNumberHex + ")");

                // If MQTT is enabled, publish discovery message
                if (mqttManager && mqttManager->isConnected())
                {
                    mqttManager->publishDiscoveryForSensor(sensorIndex);
                }

                // Redirect to sensors list
                request->redirect("/sensors");
                return;
            }
        }

        // Error adding sensor
        request->send(500, "text/plain", "Failed to add sensor");
    }
    else
    {
        // Missing parameters
        request->send(400, "text/plain", "Missing parameters");
    }
}

// Sensor edit page
void WebPortal::handleSensorEdit(AsyncWebServerRequest *request)
{
    logger.debug("HTTP request: GET /sensors/edit");
    if (guardAPOnly(request)) return;

    // Check index parameter
    if (request->hasParam("index"))
    {
        int index = request->getParam("index")->value().toInt();

        // Get sensor
        const SensorData *sensor = sensorManager.getSensor(index);

        if (sensor && sensor->configured)
        {
            // Generate HTML
            String html = HTMLGenerator::generateSensorEditPage(*sensor, index);

            // Send response
            request->send(200, "text/html", html);
            return;
        }
    }

    // Sensor not found, redirect to sensors list
    request->redirect("/sensors");
}

// Process sensor edit form
void WebPortal::handleSensorEditPost(AsyncWebServerRequest *request)
{
    logger.debug("HTTP request: POST /sensors/update");
    if (guardAPOnly(request)) return;

    // Check parameters
    if (request->hasParam("index", true) &&
        request->hasParam("name", true) &&
        request->hasParam("deviceType", true) &&
        request->hasParam("serialNumber", true) &&
        request->hasParam("deviceKey", true))
    {
        // Get form data for corrections with defaults if not present
        float tempCorr = 0.0f;
        float humCorr = 0.0f;
        float pressCorr = 0.0f;
        float ppmCorr = 0.0f;
        float luxCorr = 0.0f;
        float windSpeedCorr = 1.0f;
        int windDirCorr = 0;
        float rainAmountCorr = 1.0f;
        float rainRateCorr = 1.0f;

        if (request->hasParam("tempCorr", true))
        {
            tempCorr = request->getParam("tempCorr", true)->value().toFloat();
        }
        if (request->hasParam("humCorr", true))
        {
            humCorr = request->getParam("humCorr", true)->value().toFloat();
        }
        if (request->hasParam("pressCorr", true))
        {
            pressCorr = request->getParam("pressCorr", true)->value().toFloat();
        }
        if (request->hasParam("ppmCorr", true))
        {
            ppmCorr = request->getParam("ppmCorr", true)->value().toFloat();
        }
        if (request->hasParam("luxCorr", true))
        {
            luxCorr = request->getParam("luxCorr", true)->value().toFloat();
        }
        if (request->hasParam("windSpeedCorr", true))
        {
            windSpeedCorr = request->getParam("windSpeedCorr", true)->value().toFloat();
        }
        if (request->hasParam("windDirCorr", true))
        {
            windDirCorr = request->getParam("windDirCorr", true)->value().toInt();
        }
        if (request->hasParam("rainAmountCorr", true))
        {
            rainAmountCorr = request->getParam("rainAmountCorr", true)->value().toFloat();
        }
        if (request->hasParam("rainRateCorr", true))
        {
            rainRateCorr = request->getParam("rainRateCorr", true)->value().toFloat();
        }

        // Get form data
        int index = request->getParam("index", true)->value().toInt();
        String name = request->getParam("name", true)->value();
        uint8_t deviceTypeValue = request->getParam("deviceType", true)->value().toInt();
        String serialNumberHex = request->getParam("serialNumber", true)->value();
        String deviceKeyHex = request->getParam("deviceKey", true)->value();
        String customUrl = request->hasParam("customUrl", true) ? request->getParam("customUrl", true)->value() : "";
        int altitude = 0;
        if (request->hasParam("altitude", true))
        {
            altitude = request->getParam("altitude", true)->value().toInt();
        }

        // Convert values
        SensorType deviceType = static_cast<SensorType>(deviceTypeValue);
        uint32_t serialNumber = strtoul(serialNumberHex.c_str(), NULL, 16);
        uint32_t deviceKey = strtoul(deviceKeyHex.c_str(), NULL, 16);

        // Update sensor - Update this function parameter list too (see next step)
        bool success = sensorManager.updateSensorConfig(
            index, name, deviceType, serialNumber, deviceKey, customUrl, altitude,
            tempCorr, humCorr, pressCorr, ppmCorr, luxCorr,
            windSpeedCorr, windDirCorr, rainAmountCorr, rainRateCorr);

        if (success)
        {
            logger.info("Updated sensor: " + name + " (SN: " + serialNumberHex + ")");

            // If MQTT is enabled, publish discovery message
            if (mqttManager && mqttManager->isConnected())
            {
                logger.info("Updating MQTT discovery for edited sensor");
                mqttManager->publishDiscoveryForSensor(index);
            }

            // Redirect to sensors list
            request->redirect("/sensors");
        }
        else
        {
            // Error updating sensor
            request->send(500, "text/plain", "Failed to update sensor");
        }
    }
    else
    {
        // Missing parameters
        request->send(400, "text/plain", "Missing parameters");
    }
}

// Delete sensor
void WebPortal::handleSensorDelete(AsyncWebServerRequest *request)
{
    logger.debug("HTTP request: GET /sensors/delete");
    if (guardAPOnly(request)) return;

    // Check index parameter
    if (request->hasParam("index"))
    {
        int index = request->getParam("index")->value().toInt();

        // Get sensor
        const SensorData *sensor = sensorManager.getSensor(index);

        if (sensor && sensor->configured)
        {
            String name = sensor->name;
            uint32_t serialNumber = sensor->serialNumber;

            // Delete sensor
            bool success = sensorManager.deleteSensor(index);

            if (success)
            {
                logger.info("Deleted sensor: " + name + " (SN: " + String(serialNumber, HEX) + ")");

                // If MQTT is enabled, remove discovery message
                if (mqttManager && mqttManager->isConnected())
                {
                    logger.info("Removing MQTT discovery for deleted sensor");
                    mqttManager->removeDiscoveryForSensor(serialNumber);
                }
            }
            else
            {
                logger.warning("Failed to delete sensor with index " + String(index));
            }
        }
        else
        {
            logger.warning("Attempt to delete non-existent sensor with index " + String(index));
        }
    }

    // Redirect to sensors list
    request->redirect("/sensors");
}

// Logs page
void WebPortal::handleLogs(AsyncWebServerRequest *request)
{
    logger.debug("HTTP request: GET /logs");
    if (guardAPOnly(request)) return;

    // Get logs
    size_t logCount = 0;
    const LogEntry *logs = logger.getLogs(logCount);

    // Generate HTML
    String html = HTMLGenerator::generateLogsPage(logs, logCount, logger.getLogLevel());

    // Send response
    request->send(200, "text/html", html);
}

// Clear logs
void WebPortal::handleLogsClear(AsyncWebServerRequest *request)
{
    logger.debug("HTTP request: GET /logs/clear");
    if (guardAPOnly(request)) return;

    // Clear logs
    logger.clearLogs();

    logger.info("Memory after log clear - Free heap: " + String(ESP.getFreeHeap()) +
                " bytes, Largest block: " + String(ESP.getMaxAllocHeap()) + " bytes");

    // Redirect back to logs page
    request->redirect("/logs");
}

// Set logging level
void WebPortal::handleLogLevel(AsyncWebServerRequest *request)
{
    logger.debug("HTTP request: POST /logs/level");
    if (guardAPOnly(request)) return;

    // Check level parameter
    if (request->hasParam("level", true))
    {
        String levelStr = request->getParam("level", true)->value();
        LogLevel level = logger.levelFromString(levelStr);

        // Set new level
        configManager.setLogLevel(level);
    }

    // Redirect back to logs page
    request->redirect("/logs");
}

// MQTT Configuration Page
void WebPortal::handleMqtt(AsyncWebServerRequest *request)
{
    // logger.debug("HTTP request: GET /mqtt");
    if (guardAPOnly(request)) return;

    // Get MQTT configuration from ConfigManager
    // ConfigManager* configManager = (ConfigManager*)request->getParam("configManager")->value().toInt();
    logger.debug("HTTP request: GET /mqtt");

    // Generate HTML
    String html = HTMLGenerator::generateMqttPage(
        configManager.mqttHost,
        configManager.mqttPort,
        configManager.mqttUser,
        configManager.mqttPassword,
        configManager.mqttEnabled,
        configManager.mqttTls,
        configManager.mqttPrefix,
        configManager.mqttHAEnabled,
        configManager.mqttHAPrefix
    );

    // Send response
    request->send(200, "text/html", html);
}

// MQTT Configuration Post Handler
void WebPortal::handleMqttPost(AsyncWebServerRequest *request)
{
    logger.debug("HTTP request: POST /mqtt");
    if (guardAPOnly(request)) return;

    // Check parameters
    if (request->hasParam("host", true) &&
        request->hasParam("port", true) &&
        request->hasParam("user", true) &&
        request->hasParam("password", true))
    {

        // Get form data
        String host = request->getParam("host", true)->value();
        int port = request->getParam("port", true)->value().toInt();
        String user = request->getParam("user", true)->value();
        String password = request->getParam("password", true)->value();
        bool enabled = request->hasParam("enabled", true);
        bool tls = request->hasParam("tls", true);
        String prefix = request->getParam("prefix", true)->value();
        bool haEnabled = request->hasParam("haEnabled", true);
        String haPrefix = request->getParam("haPrefix", true)->value();

        // Update configuration
        configManager.setMqttConfig(host, port, user, password, enabled, tls, prefix, haPrefix, haEnabled);

        logger.info("MQTT configuration updated");
        logger.info("  Host: " + host + ":" + String(port));
        logger.info("  Enabled: " + String(enabled));
        logger.info("  TLS: " + String(tls));
        logger.info("  Root topic: " + prefix);
        logger.info("  HA Enabled: " + String(haEnabled));
        logger.info("  HA Topic: " + haPrefix);

        // If MQTT is enabled, reinitialize the MQTT manager
        if (mqttManager)
        {
            logger.info("Reinitializing MQTT with new configuration...");
            mqttManager->disconnect();
            mqttManager->init();
        }

        // Redirect to MQTT page
        request->redirect("/mqtt");
    }
    else
    {
        request->send(400, "text/plain", "Missing parameters");
    }
}

// API for retrieving sensor data
void WebPortal::handleAPI(AsyncWebServerRequest *request)
{
    logger.debug("HTTP request: GET /api");
    if (guardAPOnly(request)) return;

    // Check format parameter
    String format = request->hasParam("format") ? request->getParam("format")->value() : "html";

    // Check sensor parameter
    String sensorParam = request->hasParam("sensor") ? request->getParam("sensor")->value() : "";

    // Get list of sensors
    std::vector<SensorData> sensorsList;

    if (sensorParam.length() > 0)
    {
        // Filter by specific sensor
        uint32_t serialNumber = strtoul(sensorParam.c_str(), NULL, 16);
        int sensorIndex = sensorManager.findSensorBySN(serialNumber);

        if (sensorIndex >= 0)
        {
            const SensorData *sensor = sensorManager.getSensor(sensorIndex);
            if (sensor && sensor->configured)
            {
                sensorsList.push_back(*sensor);
            }
        }
    }
    else
    {
        // All sensors
        sensorsList = sensorManager.getActiveSensors();
    }

    if (format.equalsIgnoreCase("json"))
    {
        // JSON format
        String jsonOutput = HTMLGenerator::generateAPIJson(sensorsList);

        // Send JSON response
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        response->print(jsonOutput);
        response->addHeader(CORS_HEADER_NAME, CORS_HEADER_VALUE);
        request->send(response);
    }
    else if (format.equalsIgnoreCase("csv"))
    {
        // CSV format
        AsyncResponseStream *response = request->beginResponseStream("text/csv");
        response->print("name,type,serialNumber,lastSeen,temperature,humidity,pressure,ppm,lux,batteryVoltage,rssi\r\n");

        for (const auto &sensor : sensorsList)
        {
            response->print(sensor.name);
            response->print(",");
            response->print(static_cast<uint8_t>(sensor.deviceType));
            response->print(",");
            response->print(String(sensor.serialNumber, HEX));
            response->print(",");
            response->print(sensor.lastSeen > 0 ? String((millis() - sensor.lastSeen) / 1000) : "-1");
            response->print(",");

            response->print(sensor.hasTemperature() ? String(sensor.temperature, 2) : "");
            response->print(",");
            response->print(sensor.hasHumidity() ? String(sensor.humidity, 2) : "");
            response->print(",");
            response->print(sensor.hasPressure() ? String(sensor.pressure, 2) : "");
            response->print(",");
            response->print(sensor.hasPPM() ? String(sensor.ppm, 0) : "");
            response->print(",");
            response->print(sensor.hasLux() ? String(sensor.lux, 1) : "");
            response->print(",");
            response->print(String(sensor.batteryVoltage, 2));
            response->print(",");
            response->print(String(sensor.rssi));
            response->print("\r\n");
        }

        response->addHeader(CORS_HEADER_NAME, CORS_HEADER_VALUE);
        request->send(response);
    }
    else if (format.equalsIgnoreCase("html"))
    {
        // HTML format (API documentation)
        String html = HTMLGenerator::generateAPIPage(sensorsList);
        request->send(200, "text/html", html);
    }
    else
    {
        // Unknown format
        request->send(400, "text/plain", "Invalid format parameter. Supported formats: json, csv, html");
    }
}

// Restart device
void WebPortal::handleReboot(AsyncWebServerRequest *request)
{
    logger.info("HTTP request: GET /reboot - Rebooting device");

    // Send confirmation
    request->send(200, "text/html",
                  "<html><head><meta http-equiv='refresh' content='10;url=/'></head>"
                  "<body><h1>Rebooting</h1>"
                  "<p>The device is rebooting. You will be redirected in 10 seconds...</p></body></html>");

    // Restart ESP32 after 500ms (to allow response to be sent)
    delay(500);
    ESP.restart();
}

// Handle unknown pages
// In WebServer.cpp, modify the handleNotFound method
// Update WebPortal::handleNotFound in WebServer.cpp
void WebPortal::handleNotFound(AsyncWebServerRequest *request)
{
    String requestUrl = request->url();
    logger.debug("HTTP 404: " + requestUrl);

    if (isAPMode)
    {
        // In AP mode - captive portal - redirect all requests to config page

        // Skip redirecting for certain files that might be requested by browsers
        if (requestUrl.endsWith(".ico") || requestUrl.endsWith(".jpg") ||
            requestUrl.endsWith(".png") || requestUrl.endsWith(".gif") ||
            requestUrl.endsWith(".css") || requestUrl.endsWith(".js"))
        {
            request->send(404, "text/plain", "Not found");
            return;
        }

        // For Apple devices
        if (requestUrl == "/hotspot-detect.html" || requestUrl.indexOf("captive.apple.com") >= 0)
        {
            request->redirect("http://" + WiFi.softAPIP().toString() + "/config");
            return;
        }

        // For Android and Windows devices
        if (requestUrl == "/generate_204" || requestUrl == "/ncsi.txt" ||
            requestUrl.indexOf("detectportal.firefox.com") >= 0)
        {
            request->redirect("http://" + WiFi.softAPIP().toString() + "/config");
            return;
        }

        // General case - redirect to config
        request->redirect("/config");
    }
    else
    {
        // In normal mode - 404
        request->send(404, "text/plain", "404: Not Found");
    }
}

// Diagnostics endpoint: heap and system info (visible only on DEBUG/VERBOSE)
void WebPortal::handleDiagHeap(AsyncWebServerRequest *request)
{
    LogLevel lvl = logger.getLogLevel();
    if (!(lvl == LogLevel::DEBUG || lvl == LogLevel::VERBOSE))
    {
        // Pretend it doesn't exist when not in debug/verbose
        request->send(404, "text/plain", "Not found");
        return;
    }

    size_t freeHeap = ESP.getFreeHeap();
    size_t maxAlloc = ESP.getMaxAllocHeap();
#ifdef BOARD_HAS_PSRAM
    size_t psramSize = ESP.getPsramSize();
    size_t psramFree = ESP.getFreePsram();
#else
    size_t psramSize = 0;
    size_t psramFree = 0;
#endif
    size_t free8 = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t largest8 = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    float frag = (free8 > 0) ? (1.0f - (float)largest8 / (float)free8) : 0.0f;

    AsyncResponseStream *res = request->beginResponseStream("application/json");
    res->print("{");
    res->print("\"freeHeap\":"); res->print((unsigned long)freeHeap); res->print(",");
    res->print("\"maxAllocHeap\":"); res->print((unsigned long)maxAlloc); res->print(",");
    res->print("\"free8bit\":"); res->print((unsigned long)free8); res->print(",");
    res->print("\"largest8bit\":"); res->print((unsigned long)largest8); res->print(",");
    res->print("\"fragmentation\":"); res->print(frag, 4); res->print(",");
    res->print("\"psramTotal\":"); res->print((unsigned long)psramSize); res->print(",");
    res->print("\"psramFree\":"); res->print((unsigned long)psramFree); res->print(",");
    res->print("\"uptimeMs\":"); res->print((unsigned long)millis()); res->print(",");
    res->print("\"wifiStatus\":"); res->print((int)WiFi.status()); res->print(",");
    res->print("\"apMode\":"); res->print(isAPMode ? "true" : "false");
    res->print("}");
    request->send(res);
}

// Handle firmware version request
void WebPortal::handleFirmwareVersion(AsyncWebServerRequest *request)
{
    logger.debug("HTTP request: GET /firmware/version");
    if (guardAPOnly(request)) return;
    
    String json = "{\"version\":\"" + String(FIRMWARE_VERSION) + "\"}";
    request->send(200, "application/json", json);
}

// Handle firmware check request
void WebPortal::handleFirmwareCheck(AsyncWebServerRequest *request)
{
    logger.debug("HTTP request: GET /firmware/check");
    if (guardAPOnly(request)) return;
    
    // Create HTTP client to check remote version
    HTTPClient http;
    http.setTimeout(5000);
    http.setReuse(false);
    http.begin(FIRMWARE_UPDATE_URL);
    http.addHeader("User-Agent", "expLORA-Gateway/" + String(FIRMWARE_VERSION));
    
    int httpCode = http.GET();
    String response = "";
    
    if (httpCode == HTTP_CODE_OK) {
        response = http.getString();
        logger.info("Firmware check successful");
    } else {
        logger.error("Firmware check failed: HTTP " + String(httpCode));
        response = "{\"error\":\"Failed to check remote version\",\"code\":" + String(httpCode) + "}";
    }
    
    http.end();
    request->send(httpCode == HTTP_CODE_OK ? 200 : 500, "application/json", response);
}

// Handle firmware update request
void WebPortal::handleFirmwareUpdate(AsyncWebServerRequest *request) {
  logger.debug("HTTP request: POST /firmware/update");
  if (guardAPOnly(request)) return;

  if (!request->hasParam("url", true)) {
    request->send(400, "application/json", "{\"error\":\"Missing firmware URL\"}");
    return;
  }

  const String firmwareUrl = request->getParam("url", true)->value();
  logger.info("Starting firmware update from: " + firmwareUrl);

  HTTPClient http;
  http.setTimeout(10000);
  http.setReuse(false);
  if (!http.begin(firmwareUrl)) {
    request->send(500, "application/json", "{\"error\":\"HTTP begin failed\"}");
    return;
  }
  http.addHeader("User-Agent", "explora-gw-lite/" + String(FIRMWARE_VERSION));

  const int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    logger.error("FW download failed: HTTP " + String(httpCode));
    request->send(502, "application/json", "{\"error\":\"Firmware download failed\"}");
    http.end();
    return;
  }

  // Content-Length can be -1 for chunked transfer
  const int contentLength = http.getSize();

  // Target OTA slot + size check
  const esp_partition_t* ota = esp_ota_get_next_update_partition(nullptr);
  if (!ota) {
    request->send(500, "application/json", "{\"error\":\"No OTA partition\"}");
    http.end();
    return;
  }
  if (contentLength > 0 && (size_t)contentLength > ota->size) {
    logger.error("FW too large: " + String(contentLength) + " > " + String(ota->size));
    request->send(413, "application/json", "{\"error\":\"Firmware too large for OTA slot\"}");
    http.end();
    return;
  }

  // Unmap filesystem so flash is not held by MMU
  LittleFS.end();

  // Start OTA with real size (if known), otherwise UPDATE_SIZE_UNKNOWN
  const size_t beginSize = (contentLength > 0) ? (size_t)contentLength : (size_t)UPDATE_SIZE_UNKNOWN;
  if (!Update.begin(beginSize)) {
    logger.error("Update.begin failed: " + String(Update.getError()));
    request->send(500, "application/json", "{\"error\":\"Update begin failed\"}");
    http.end();
    return;
  }

  // Immediate response to client; actual flashing continues within the handler
  request->send(200, "application/json",
                "{\"status\":\"update-started\",\"slot_size\":" + String(ota->size) +
                ",\"content_length\":" + String(contentLength) + "}");

  // === STREAM + YIELD loop ===
  WiFiClient *stream = http.getStreamPtr();
  static const size_t kBufSize = 2048;
  uint8_t buf[kBufSize];

  const int expected = contentLength;     // -1 pokud chunked
  int remaining = expected;
  size_t totalWritten = 0;
  int lastPct = -1;

  const uint32_t noDataTimeoutMs = 15000; // 15 s bez dat => konec
  uint32_t lastDataMs = millis();

  while (true) {
    int avail = stream->available();

    if (avail > 0) {
      if (avail > (int)kBufSize) avail = (int)kBufSize;
      int r = stream->readBytes((char*)buf, avail);
      if (r <= 0) {
        delay(1);
        esp_task_wdt_reset();
        continue;
      }

      // Protection against slot overflow even with chunked transfer
      if ((totalWritten + (size_t)r) > ota->size) {
        logger.error("Aborting: data exceed OTA slot size (" + String(ota->size) + ")");
        break;
      }

      size_t w = Update.write(buf, r);
      if (w != (size_t)r) {
        logger.error("Short write to flash: wrote " + String(w) + " of " + String(r));
        break;
      }

      totalWritten += w;
      lastDataMs = millis();
      if (remaining > 0) remaining -= r;

      // Progres
      if (expected > 0) {
        int pct = (int)((totalWritten * 100ULL) / (size_t)expected);
        if (pct != lastPct && (pct == 1 || pct % 5 == 0 || pct >= 99)) {
          logger.info("OTA progress: " + String(pct) + "% (" + String(totalWritten) + " / " + String(expected) + " bytes)");
          lastPct = pct;
        }
      } else {
        // chunked - occasional log
        if ((totalWritten % (128 * 1024)) < (size_t)r) {
          logger.info("OTA written: " + String(totalWritten) + " bytes (chunked)");
        }
      }

      // YIELD: give CPU to other tasks and feed WDT
      esp_task_wdt_reset();
      delay(0);
    } else {
      // Nothing to read - short yield
      delay(1);
      esp_task_wdt_reset();
    }

    // End if we know length and everything is written
    if (expected > 0 && remaining <= 0) break;

    // End when server disconnected and nothing flows anymore
    if (!http.connected() && stream->available() == 0) break;

    // Timeout bez dat
    if (millis() - lastDataMs > noDataTimeoutMs) {
      logger.error("OTA timeout: no data for " + String(noDataTimeoutMs) + " ms");
      break;
    }
  }

  logger.info("OTA written: " + String(totalWritten) + " bytes");

  if (!Update.end()) {
    logger.error("Update.end error: " + String(Update.getError()));
    http.end();
    delay(500);
    ESP.restart();  // restart for clean state (FS will remount in setup)
    return;
  }
  if (!Update.isFinished()) {
    logger.error("Update not finished");
    http.end();
    delay(500);
    ESP.restart();
    return;
  }

  logger.info("OTA complete, rebooting...");
  http.end();
  delay(1000);
  ESP.restart();
}

void WebPortal::handleFirmwarePage(AsyncWebServerRequest *request)
{
    logger.debug("HTTP request: GET /firmware");
    if (guardAPOnly(request)) return;
    String html = HTMLGenerator::generateFirmwarePage();
    request->send(200, "text/html", html);
}

// POST /firmware - completion callback (sending response)
void WebPortal::handleFirmwareUploadComplete(AsyncWebServerRequest *request)
{
    logger.debug("HTTP request: POST /firmware (complete)");
    if (guardAPOnly(request)) return;

    if (otaUploadHasError)
    {
        String msg = "{\"error\":\"" + otaUploadErrorMsg + "\"}";
        request->send(500, "application/json", msg);
        // reset state for next attempts
        otaUploadHasError = false;
        otaUploadErrorMsg = "";
        otaUploadExpected = otaUploadWritten = 0;
        return;
    }

    // Success
    String msg = "{\"status\":\"update-started\",\"message\":\"Firmware uploaded. Installing… device will reboot automatically.\",\"bytes\":" + String(otaUploadWritten) + "}";
    request->send(200, "application/json", msg);

    // restart after short delay (so response gets sent)
    delay(800);
    ESP.restart();
}

// Upload chunk callback
void WebPortal::handleFirmwareUploadChunk(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
    if (guardAPOnly(request)) return;
    if (index == 0)
    {
        // Upload start (first chunk)
        otaUploadHasError = false;
        otaUploadErrorMsg = "";
        otaUploadExpected = request->contentLength(); // HINT: may be inaccurate with multipart
        otaUploadWritten = 0;

        logger.info("Firmware upload started: " + filename + " size=" + String(otaUploadExpected));

        // Target OTA slot
        const esp_partition_t *ota = esp_ota_get_next_update_partition(nullptr);
        if (!ota)
        {
            otaUploadHasError = true;
            otaUploadErrorMsg = "No OTA partition";
            return;
        }

        // If server/client specified expected size, at least verify it fits in slot
        if (otaUploadExpected > 0 && otaUploadExpected > ota->size)
        {
            otaUploadHasError = true;
            otaUploadErrorMsg = "Firmware too large for OTA slot";
            return;
        }

        // Unmap FS (for MMU)
        LittleFS.end();

        // IMPORTANT: use size UNKNOWN for multipart - otherwise risk "premature end"
        if (!Update.begin((size_t)UPDATE_SIZE_UNKNOWN))
        {
            otaUploadHasError = true;
            otaUploadErrorMsg = "Update.begin failed: " + String(Update.getError());
            return;
        }
    }

    if (otaUploadHasError)
        return;

    // Write chunk
    if (len)
    {
        // Safety: never overwrite OTA slot size
        const esp_partition_t *ota = esp_ota_get_next_update_partition(nullptr);
        if (ota && (otaUploadWritten + len) > ota->size)
        {
            otaUploadHasError = true;
            otaUploadErrorMsg = "Data exceed OTA slot size";
            return;
        }

        size_t w = Update.write(data, len);
        if (w != len)
        {
            otaUploadHasError = true;
            otaUploadErrorMsg = "Short write: " + String(w) + "/" + String(len);
            return;
        }
        otaUploadWritten += w;

        // Progress log
        if (otaUploadExpected > 0)
        {
            int pct = (int)((otaUploadWritten * 100ULL) / (size_t)otaUploadExpected);
            if (pct == 1 || pct % 5 == 0 || pct >= 99)
            {
                logger.info("OTA progress: " + String(pct) + "% (" + String(otaUploadWritten) + " / " + String(otaUploadExpected) + " bytes)");
            }
        }
        else if ((otaUploadWritten % (128 * 1024)) < len)
        {
            logger.info("OTA written: " + String(otaUploadWritten) + " bytes (chunked)");
        }

        // Yield + WDT
        esp_task_wdt_reset();
        delay(0);
    }

    if (final)
    {
        logger.info("Firmware upload finished: total " + String(otaUploadWritten) + " bytes");

        if (otaUploadExpected > 0 && otaUploadWritten != otaUploadExpected)
        {
            logger.warning("Upload size hint mismatch: written=" + String(otaUploadWritten) + " expected=" + String(otaUploadExpected));
        }

        // IMPORTANT: finish multipart upload with evenIfRemaining=true
        if (!Update.end(true))
        {
            otaUploadHasError = true;
            otaUploadErrorMsg = "Update.end error: " + String(Update.getError());
            return;
        }
        if (!Update.isFinished())
        {
            otaUploadHasError = true;
            otaUploadErrorMsg = "Update not finished";
            return;
        }
        // response will be sent by handleFirmwareUploadComplete()
    }
}
