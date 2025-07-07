/**
 * expLORA Gateway Lite
 *
 * HTML content generator header file
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
#include <vector>
#include "../Data/SensorData.h"
#include "../Data/Logging.h"
#include "../Hardware/Network_Manager.h"

/**
 * Class for generating HTML content
 *
 * Handles creation of HTML pages for web interface, optimized
 * for use with PSRAM (if available).
 */
class HTMLGenerator
{
private:
    // Buffer for generating HTML content
    static char *htmlBuffer;
    static size_t htmlBufferSize;
    static bool usePSRAM;

    // Buffer initialization
    static bool initBuffer();

    // Free buffer
    static void freeBuffer();

    // Add HTML opening code
    static void addHtmlHeader(String &html, const String &title, bool isAPMode);

    // Add HTML closing code
    static void addHtmlFooter(String &html);

    // Add CSS for pages
    static void addStyles(String &html);

    // Add JavaScript for interactive elements
    static void addJavaScript(String &html);

    // Add navigation
    static void addNavigation(String &html, const String &activePage);

public:
    // Initialize generator
    static bool init(bool usePsram = true, size_t bufferSize = 32768);

    // Free resources
    static void deinit();

    // Generate home page
    static String generateHomePage(const std::vector<SensorData> &sensors, const NetworkManager &networkManager);

    // Generate configuration page
    static String generateConfigPage(const String &ssid, const String &password, bool configMode, const String &ip, const String &timezone, const NetworkManager &networkManager);

    // Generate MQTT settings page
    static String generateMqttPage(const String &host, int port, const String &user, const String &password, bool enabled, bool tls,
                                    const String &prefix, bool haEnabled, String &haPrefix);

    // Generate sensor list page
    static String generateSensorsPage(const std::vector<SensorData> &sensors);

    // Generate sensor add page
    static String generateSensorAddPage();

    // Generate sensor edit page
    static String generateSensorEditPage(const SensorData &sensor, int index);

    // Generate logs page
    static String generateLogsPage(const LogEntry *logs, size_t logCount, LogLevel currentLevel);

    // Generate API page
    static String generateAPIPage(const std::vector<SensorData> &sensors);

    // Generate JSON for API
    static String generateAPIJson(const std::vector<SensorData> &sensors, const NetworkManager &networkManager);

    // Optimized versions using buffer
    static void generateSensorTable(char *buffer, size_t &maxLen, const std::vector<SensorData> &sensors);
    static void generateLogTable(char *buffer, size_t &maxLen, const LogEntry *logs, size_t logCount);

    // Additional helper methods
    static String getWifiNetworkOptions(const String &currentSSID);
    static String getSensorTypeOptions(SensorType currentType);
    static String getLogLevelOptions(LogLevel currentLevel);
};
