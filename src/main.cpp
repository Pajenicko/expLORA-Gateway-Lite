/**
 * expLORA Gateway Lite
 *
 * Main program file for the expLORA Gateway
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

#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <time.h>
#include <esp_task_wdt.h>
#include <ArduinoJson.h>

// Configuration headers
#include "config.h"

// Data
#include "Data/SensorTypes.h"
#include "Data/SensorData.h"
#include "Data/Logging.h"
#include "Data/SensorManager.h"

// Hardware
#include "Hardware/LoRa_Module.h"
#include "Hardware/PSRAM_Manager.h"
#include "Hardware/SPI_Manager.h"
#include "Hardware/Network_Manager.h"

// Storage
#include "Storage/ConfigManager.h"

// Protocol
#include "Protocol/LoRaProtocol.h"
// MQTT
#include "Protocol/MQTTManager.h"

// Web interface
#include "Web/WebServer.h"
#include "Web/HTMLGenerator.h"

// Global object instances
Logger logger;                  // Logging system
ConfigManager *configManager;   // Configuration manager
SPIManager *spiManager;         // SPI communication manager
LoRaModule *loraModule;         // LoRa module
SensorManager *sensorManager;   // Sensor manager
LoRaProtocol *loraProtocol;     // LoRa protocol
MQTTManager *mqttManager;       // MQTT Manager
WebPortal *webPortal;           // Web interface
NetworkManager *networkManager; // Network manager

// File system initialization
bool initFileSystem()
{
    if (!LittleFS.begin(true))
    {
        Serial.println("ERROR: Failed to mount LittleFS");
        return false;
    }
    Serial.println("LittleFS mounted successfully");
    return true;
}

// Setup - device initialization
void setup()
{
    // Serial port initialization
    Serial.begin(115200);
    delay(1000); // Wait for stabilization

    Serial.println("\n\nexpLORA Gateway Lite");
    Serial.println("------------------------------------------------------------");

// Explicit PSRAM initialization
#ifdef BOARD_HAS_PSRAM
    if (psramInit())
    {
        Serial.println("PSRAM initialized manually!");
    }
#endif

    // Memory diagnostics
    Serial.printf("Total heap: %d bytes\n", ESP.getHeapSize());
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());

// PSRAM initialization
#ifdef BOARD_HAS_PSRAM
    bool psramInitialized = false;

    // Try different PSRAM detection methods
    if (esp_spiram_is_initialized())
    {
        psramInitialized = true;
        Serial.println("PSRAM initialized via esp_spiram_is_initialized()");
    }
    else if (ESP.getPsramSize() > 0)
    {
        psramInitialized = true;
        Serial.println("PSRAM detected via ESP.getPsramSize()");
    }

    if (psramInitialized)
    {
        Serial.printf("Total PSRAM: %d bytes\n", ESP.getPsramSize());
        Serial.printf("Free PSRAM: %d bytes\n", ESP.getFreePsram());
    }
    else
    {
        Serial.println("PSRAM initialization failed or not available");
    }
#endif

    // File system initialization
    if (!initFileSystem())
    {
        Serial.println("ERROR: File system initialization failed");
        return;
    }

    // Logging system initialization
    if (!logger.init(LOG_BUFFER_SIZE))
    {
        Serial.println("ERROR: Logger initialization failed");
        return;
    }

    // Basic log after initialization
    logger.info("expLORA Gateway Lite starting up - Firmware v" + String(FIRMWARE_VERSION));

    // HTML generator initialization
    if (!HTMLGenerator::init(true, WEB_BUFFER_SIZE))
    {
        logger.error("Failed to initialize HTML generator");
        return;
    }

    // Configuration manager initialization
    configManager = new ConfigManager(logger);
    if (!configManager->init())
    {
        logger.error("Failed to initialize configuration manager");
        return;
    }

    // Setting logging level according to configuration
    logger.setLogLevel(configManager->logLevel);

    // SPI manager initialization
    spiManager = new SPIManager(logger);
    if (!spiManager->init())
    {
        logger.error("Failed to initialize SPI manager");
        return;
    }

    // Network manager initialization
    networkManager = new NetworkManager(logger);

    // Sensor manager initialization
    sensorManager = new SensorManager(logger, *networkManager);
    if (!sensorManager->init())
    {
        logger.error("Failed to initialize sensor manager");
    }
    else
    {
        logger.info("Sensor manager initialized with " +
                    String(sensorManager->getSensorCount()) + " sensors");
    }

    // WiFi Initialization - MODIFIED CODE SECTION
    logger.info("Configuring WiFi. ConfigMode: " + String(configManager->configMode ? "true" : "false") +
                ", SSID length: " + String(configManager->wifiSSID.length()));

    if (configManager->configMode || configManager->wifiSSID.length() == 0)
    {
        // We're in configuration mode or don't have credentials - AP mode only
        logger.info("Starting in AP mode only");

        networkManager->setupAP();
        networkManager->setAPTimeout(0); // Disable timeout

        configManager->enableConfigMode(true);
        webPortal = new WebPortal(*sensorManager, logger,
                                  configManager->wifiSSID, configManager->wifiPassword,
                                  configManager->configMode, *configManager, *networkManager, configManager->timezone);
    }
    else
    {
        // We have credentials - start AP+STA mode
        logger.info("Starting in AP+STA mode (dual mode)");

        if (networkManager->setupAP()) {
            networkManager->setAPTimeout(AP_TIMEOUT); // In Dual mode, we have timeout for AP mode
            logger.info("Temporary AP started with SSID: " + networkManager->getWiFiAPSSID() +
                        " (will be active for 5 minutes). IP: " + networkManager->getWiFiAPIP().toString());
        }

        if (networkManager->wifiSTAConnect(configManager->wifiSSID, configManager->wifiPassword)) {
            configManager->enableConfigMode(false);

            // Initialize NTP
            configTime(0, 0, NTP_SERVER);                     // First set to UTC
            setenv("TZ", configManager->timezone.c_str(), 1); // Set the TZ environment variable
            tzset();                                          // Apply the time zone
            logger.info("NTP time set");
            logger.setTimeInitialized(true);
        }
        else {
            logger.warning("Continuing in AP mode only");

            // Switch to AP mode only
            networkManager->wifiSTADisconnect();
        }

        // Web interface initialization - will be accessible via AP and client (if connected)
        webPortal = new WebPortal(*sensorManager, logger,
                                  configManager->wifiSSID, configManager->wifiPassword,
                                  configManager->configMode, *configManager, *networkManager, configManager->timezone);
    }

    // LoRa module initialization
    loraModule = new LoRaModule(logger, spiManager);
    if (!loraModule->init())
    {
        logger.error("Failed to initialize LoRa module");
    }
    else
    {
        logger.info("LoRa module initialized successfully");
    }

    // LoRa protocol initialization
    loraProtocol = new LoRaProtocol(*loraModule, *sensorManager, logger);

    // Web portal initialization if not already initialized
    if (!webPortal)
    {
        webPortal = new WebPortal(*sensorManager, logger, configManager->wifiSSID, configManager->wifiPassword, configManager->configMode,
            *configManager, *networkManager, configManager->timezone);
    }

    // Initialize MQTT Manager if WiFi is connected
    mqttManager = new MQTTManager(*sensorManager, *configManager, logger, *networkManager);
    if (!mqttManager->init())
    {
        logger.debug("MQTT Manager initialization skipped (disabled in config)");
    }

    if (!webPortal->init())
    {
        logger.error("Failed to initialize web portal");
    }
    else
    {
        logger.info("Web portal initialized successfully");
    }

    webPortal->setMqttManager(mqttManager);

    // Initialize task watchdog
    esp_task_wdt_init(WDT_TIMEOUT, true); // Enable panic on timeout
    esp_task_wdt_add(NULL);               // Add current task to watchdog
    logger.info("Task watchdog initialized with timeout of " + String(WDT_TIMEOUT) + " seconds");

    logger.info("System initialization complete");
}

// Loop - main loop
void loop()
{
    esp_task_wdt_reset();

    // Process LoRa packets
    // if (loraModule && loraProtocol) {
    //    if (LoRaModule::hasInterrupt()) {
    //        logger.debug("LoRa interrupt detected");
    //        loraProtocol->processReceivedPacket();
    //    }
    //}

    // Handle web interface
    if (webPortal)
    {
        webPortal->handleClient();
    }

    // Process MQTT communication
    if (mqttManager && networkManager->isConnected())
    {
        mqttManager->process();
    }

    // Add a check for LoRa packet processing to publish to MQTT
    static unsigned int lastProcessedIndex = -1;
    if (loraModule && loraProtocol && LoRaModule::hasInterrupt())
    {
        if (loraProtocol->processReceivedPacket())
        {
            // If we have a mqttManager and it's connected, publish the latest sensor data
            if (mqttManager && networkManager->isConnected())
            {
                mqttManager->publishSensorData(loraProtocol->getLastProcessedSensorIndex());
            }
        }
    }

    // Process network stuff
    if (networkManager)
    {
        networkManager->process();
    }

    // TODO: Move this to network manager process loop
    // Check WiFi connection and reconnect if needed
    if (!configManager->configMode && !networkManager->isWiFiConnected())
    {
        unsigned long now = millis();
        if (now - configManager->lastWifiAttempt > WIFI_RECONNECT_INTERVAL)
        {
            logger.info("Attempting to reconnect to WiFi...");
            configManager->lastWifiAttempt = now;

            if (networkManager->wifiSTAConnect(configManager->wifiSSID, configManager->wifiPassword))
            {
                logger.info("WiFi reconnected! IP: " + WiFi.localIP().toString());

                // Update NTP time
                configTime(0, 0, NTP_SERVER);                     // First set to UTC
                setenv("TZ", configManager->timezone.c_str(), 1); // Set the TZ environment variable
                tzset();                                          // Apply the time zone
            }
            else
            {
                logger.warning("Failed to reconnect to WiFi");
            }
        }
    }

    // Short delay for stability
    delay(5);

    // Memory diagnostics every 10 minutes
    static unsigned long lastMemCheck = 0;
    if (millis() - lastMemCheck > 600000)
    { // 10 minutes
        lastMemCheck = millis();

        logger.info("Memory status - Free heap: " + String(ESP.getFreeHeap()) +
                    " bytes, Largest block: " + String(ESP.getMaxAllocHeap()) + " bytes");

#ifdef BOARD_HAS_PSRAM
        if (esp_spiram_is_initialized())
        {
            logger.debug("PSRAM status - Free: " + String(ESP.getFreePsram()) +
                         " bytes, Largest block: " + String(ESP.getMaxAllocPsram()) + " bytes");
        }
#endif
    }
}
