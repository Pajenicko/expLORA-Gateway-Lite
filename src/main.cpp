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
Logger logger;                // Logging system
ConfigManager *configManager; // Configuration manager
SPIManager *spiManager;       // SPI communication manager
LoRaModule *loraModule;       // LoRa module
SensorManager *sensorManager; // Sensor manager
LoRaProtocol *loraProtocol;   // LoRa protocol
MQTTManager *mqttManager;     // MQTT Manager
WebPortal *webPortal;         // Web interface

// Timer for disabling AP mode
unsigned long apStartTime = 0;
bool temporaryAPMode = false;

// NTP synchronization helper - returns true if time was successfully synchronized
bool syncNTPTime(const String &timezone, unsigned long timeoutMs = 10000)
{
    configTime(0, 0, NTP_SERVER);
    setenv("TZ", timezone.c_str(), 1);
    tzset();

    // Wait for actual NTP sync with timeout
    struct tm timeinfo;
    unsigned long startWait = millis();
    while (!getLocalTime(&timeinfo, 100))
    {
        if (millis() - startWait > timeoutMs)
        {
            return false;
        }
        delay(100);
    }
    return true;
}

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

    // Sensor manager initialization
    sensorManager = new SensorManager(logger);
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

    // Setting timer for temporary AP mode
    apStartTime = millis();
    temporaryAPMode = true;

    String uniqueSSID = makeApName();

    if (configManager->configMode || configManager->wifiSSID.length() == 0)
    {
        // We're in configuration mode or don't have credentials - AP mode only
        logger.info("Starting in AP mode only");
        WiFi.mode(WIFI_AP);

        // Configure AP with unique SSID (channel 6, max 4 clients)
        WiFi.softAP(uniqueSSID.c_str(), nullptr, 6, 0, 4);
        logger.info("AP started with SSID: " + uniqueSSID + ", IP: " + WiFi.softAPIP().toString());

        configManager->enableConfigMode(true);
        webPortal = new WebPortal(*sensorManager, logger,
                                  configManager->wifiSSID, configManager->wifiPassword,
                                  configManager->configMode, *configManager, configManager->timezone);
    }
    else
    {
        // We have credentials - start AP+STA mode
        logger.info("Starting in AP+STA mode (dual mode)");
        WiFi.mode(WIFI_AP_STA);

        // Configure AP part with unique SSID (channel 6, max 4 clients)
        WiFi.softAP(uniqueSSID.c_str(), nullptr, 6, 0, 4);
        logger.info("Temporary AP started with SSID: " + uniqueSSID +
                    " (will be active for 5 minutes). IP: " + WiFi.softAPIP().toString());

        // Configure STA part (client)
        logger.info("Attempting to connect to WiFi: " + configManager->wifiSSID);
        WiFi.begin(configManager->wifiSSID.c_str(), configManager->wifiPassword.c_str());

        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20)
        {
            delay(500);
            Serial.print(".");
            attempts++;
        }

        if (WiFi.status() == WL_CONNECTED)
        {
            logger.info("WiFi connected! IP: " + WiFi.localIP().toString());
            configManager->enableConfigMode(false);

            // Initialize NTP with sync verification
            if (syncNTPTime(configManager->timezone))
            {
                logger.info("NTP time synchronized successfully");
                logger.setTimeInitialized(true);
            }
            else
            {
                logger.warning("NTP sync failed - time not available");
            }
        }
        else
        {
            logger.warning("Failed to connect to WiFi after " + String(attempts) + " attempts. SSID: " +
                           configManager->wifiSSID + ", Continuing in AP mode only");
            // Switch to AP mode only
            WiFi.mode(WIFI_AP);
        }

        // Web interface initialization - will be accessible via AP and client (if connected)
        webPortal = new WebPortal(*sensorManager, logger,
                                  configManager->wifiSSID, configManager->wifiPassword,
                                  configManager->configMode, *configManager, configManager->timezone);
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
        webPortal = new WebPortal(*sensorManager, logger, configManager->wifiSSID, configManager->wifiPassword, configManager->configMode, *configManager,
                                  configManager->timezone);
    }

    // Initialize MQTT Manager if WiFi is connected
    mqttManager = new MQTTManager(*sensorManager, *configManager, logger);
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

    // Detect WiFi status transitions and adapt web server routes
    static wl_status_t lastWifiStatus = WiFi.status();
    wl_status_t currentWifiStatus = WiFi.status();
    if (lastWifiStatus != currentWifiStatus)
    {
        // On transition to connected, update portal mode (routes are always registered)
        if (currentWifiStatus == WL_CONNECTED)
        {
            logger.info("WiFi connected (transition). Reinitializing web portal routes.");
            if (webPortal)
            {
                webPortal->setAPMode(false);
            }
        }
        // On transition away from connected, switch portal to AP mode so handlers reflect captive state
        else if (lastWifiStatus == WL_CONNECTED && currentWifiStatus != WL_CONNECTED)
        {
            logger.warning("WiFi disconnected (transition). Switching web portal to AP mode.");
            if (webPortal)
            {
                webPortal->setAPMode(true);
                // No immediate restart to avoid flapping; routes will still serve config via handlers
            }
        }
        lastWifiStatus = currentWifiStatus;
    }

    // Check timer for temporary AP mode
    if (temporaryAPMode && !configManager->configMode && configManager->wifiSSID.length() > 0)
    {
        if (millis() - apStartTime > AP_TIMEOUT)
        {
            // Time expired, switch to client mode if successfully connected
            if (WiFi.status() == WL_CONNECTED)
            {
                logger.info("Temporary AP timeout reached. Switching to client mode only.");
                WiFi.mode(WIFI_STA);
                temporaryAPMode = false;
            }
            else
            {
                // Not connected as a client, keep AP running
                logger.info("Temporary AP timeout reached but WiFi client not connected. Keeping AP mode active.");
                temporaryAPMode = false; // Stop timer, but AP remains active
            }
        }
    }

    // Process LoRa packets
    // if (loraModule && loraProtocol) {
    //    if (LoRaModule::hasInterrupt()) {
    //        logger.debug("LoRa interrupt detected");
    //        loraProtocol->processReceivedPacket();
    //    }
    //}

    // If in AP mode, process DNS captive portal:
    if (webPortal && webPortal->isInAPMode())
    {
        webPortal->processDNS(); // This method internally calls dnsServer.processNextRequest();
    }

    // Handle web interface
    if (webPortal)
    {
        webPortal->handleClient();
    }

    // Process MQTT communication
    if (mqttManager && WiFi.status() == WL_CONNECTED)
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
            if (mqttManager && WiFi.status() == WL_CONNECTED)
            {
                mqttManager->publishSensorData(loraProtocol->getLastProcessedSensorIndex());
            }
        }
    }

    // Check WiFi connection and reconnect if needed.
    // Non-blocking state machine: kick off WiFi.begin() once per
    // WIFI_RECONNECT_INTERVAL window, then poll status across loop iterations
    // up to RECONNECT_ATTEMPT_TIMEOUT_MS before giving up. Keeps web/DNS/MQTT/LoRa
    // responsive throughout the reconnect window.
    {
        enum class ReconnectState { Idle, InProgress };
        static ReconnectState reconnectState = ReconnectState::Idle;
        static unsigned long reconnectStartedAt = 0;
        constexpr unsigned long RECONNECT_ATTEMPT_TIMEOUT_MS = 5000; // matches old 10 * 500ms

        if (!configManager->configMode && WiFi.status() != WL_CONNECTED)
        {
            unsigned long now = millis();

            if (reconnectState == ReconnectState::Idle)
            {
                if (now - configManager->lastWifiAttempt > WIFI_RECONNECT_INTERVAL)
                {
                    logger.info("Attempting to reconnect to WiFi...");
                    configManager->lastWifiAttempt = now;
                    WiFi.begin(configManager->wifiSSID.c_str(), configManager->wifiPassword.c_str());
                    reconnectStartedAt = now;
                    reconnectState = ReconnectState::InProgress;
                }
            }
            else // ReconnectState::InProgress
            {
                if (WiFi.status() == WL_CONNECTED)
                {
                    logger.info("WiFi reconnected! IP: " + WiFi.localIP().toString());
                    // NTP sync verification (note: this call may still block briefly)
                    if (syncNTPTime(configManager->timezone))
                    {
                        logger.info("NTP time re-synchronized successfully");
                        logger.setTimeInitialized(true);
                    }
                    else
                    {
                        logger.warning("NTP re-sync failed after WiFi reconnect");
                    }
                    reconnectState = ReconnectState::Idle;
                }
                else if (now - reconnectStartedAt > RECONNECT_ATTEMPT_TIMEOUT_MS)
                {
                    logger.warning("Failed to reconnect to WiFi");
                    reconnectState = ReconnectState::Idle;
                }
                // else: keep waiting; loop continues to service web/MQTT/LoRa
            }
        }
        else if (reconnectState == ReconnectState::InProgress)
        {
            // Connected (or no longer trying) via some other path while we were waiting.
            reconnectState = ReconnectState::Idle;
        }
    }

    // Short delay for stability
    delay(5);

    // Memory diagnostics and time check every 10 minutes
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

        // Periodic time check - re-sync if time was lost
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo, 100))
        {
            logger.warning("System time invalid - attempting NTP re-sync");
            if (WiFi.status() == WL_CONNECTED)
            {
                if (syncNTPTime(configManager->timezone))
                {
                    logger.info("NTP time recovered successfully");
                    logger.setTimeInitialized(true);
                }
                else
                {
                    logger.error("NTP re-sync failed - time remains unavailable");
                    logger.setTimeInitialized(false);
                }
            }
            else
            {
                logger.warning("Cannot sync time - WiFi not connected");
                logger.setTimeInitialized(false);
            }
        }
    }
}
