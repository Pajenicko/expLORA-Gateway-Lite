/**
 * expLORA Gateway Lite
 *
 * HTML content generator implementation file
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

 #include "HTMLGenerator.h"
#include <esp_heap_caps.h>
#include <algorithm>
#include <Arduino.h>
#include "../config.h"

// Initialization of static variables
char *HTMLGenerator::htmlBuffer = nullptr;
size_t HTMLGenerator::htmlBufferSize = WEB_BUFFER_SIZE;
bool HTMLGenerator::usePSRAM = false;

// Generator initialization
bool HTMLGenerator::init(bool usePsram, size_t bufferSize)
{
    // If buffer is already initialized, free it first
    if (htmlBuffer != nullptr)
    {
        deinit();
    }

    htmlBufferSize = bufferSize;

#ifdef BOARD_HAS_PSRAM
    usePSRAM = true;
    htmlBuffer = (char *)heap_caps_malloc(htmlBufferSize, MALLOC_CAP_SPIRAM);

    if (htmlBuffer != nullptr)
    {
        Serial.printf("HTMLGenerator: Allocated %u bytes in PSRAM for HTML content\n", htmlBufferSize);
    }
    else
    {
        // Fallback to regular RAM
        usePSRAM = false;
        htmlBuffer = new char[htmlBufferSize];
        Serial.printf("HTMLGenerator: Failed to allocate PSRAM, using %u bytes in RAM\n", htmlBufferSize);
    }
#else
    usePSRAM = false;
    htmlBuffer = new char[htmlBufferSize];
    Serial.printf("HTMLGenerator: Using %u bytes in RAM for HTML content\n", htmlBufferSize);
#endif

    // Check if memory allocation was successful
    if (htmlBuffer == nullptr)
    {
        Serial.println("HTMLGenerator: Failed to allocate memory for HTML buffer");
        return false;
    }

    return true;
}

// Resource release
void HTMLGenerator::deinit()
{
    if (htmlBuffer != nullptr)
    {
#ifdef BOARD_HAS_PSRAM
        if (usePSRAM)
        {
            heap_caps_free(htmlBuffer);
        }
        else
        {
#endif
            delete[] htmlBuffer;
#ifdef BOARD_HAS_PSRAM
        }
#endif
        htmlBuffer = nullptr;
    }
}

// Adding HTML header code
void HTMLGenerator::addHtmlHeader(String &html, const String &title, bool isAPMode = false)
{
    html += "<!DOCTYPE html><html><head>";
    html += "<meta charset='UTF-8'>";
    html += "<title>" + title + " - expLORA Gateway Lite</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";

    // Add CSS styles
    addStyles(html);

    html += "</head><body>";
    html += "<header><h1>expLORA Gateway Lite</h1><h2>" + title + "</h2></header>";

    // Add navigation only if not in AP mode
    if (!isAPMode)
    {
        addNavigation(html, title);
    }
    else
    {
        // In AP mode, just add the container div without navigation
        html += "<div class='container'>";
    }
}

// Adding HTML footer code
void HTMLGenerator::addHtmlFooter(String &html)
{
    html += "<footer>";
    html += "<p>expLORA Gateway Lite v" + String(FIRMWARE_VERSION) + " &copy; 2025</p>";
    html += "</footer>";

    // Adding JavaScript
    addJavaScript(html);

    html += "</body></html>";
}

// Adding CSS for pages
void HTMLGenerator::addStyles(String &html)
{
    html += "<style>";
    html += "* { box-sizing: border-box; }";
    html += "body { font-family: Arial, sans-serif; margin: 0; padding: 0; line-height: 1.6; }";
    html += "header { background: #0066cc; color: white; padding: 20px; text-align: center; }";
    html += "header h1 { margin: 0; }";
    html += "header h2 { margin: 5px 0 0 0; font-weight: normal; }";

    // Navigation
    html += "nav { background: #333; overflow: hidden; }";
    html += "nav a { float: left; display: block; color: white; text-align: center; padding: 14px 16px; text-decoration: none; }";
    html += "nav a:hover { background: #0066cc; }";
    html += "nav a.active { background: #0066cc; }";
    html += "nav .icon { display: none; }";

    // Cards
    html += ".container { padding: 20px; }";
    html += ".card { background: white; border-radius: 5px; padding: 20px; margin-bottom: 20px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }";

    // Tables
    html += "table { width: 100%; border-collapse: collapse; margin-bottom: 20px; }";
    html += "th, td { text-align: left; padding: 12px; border-bottom: 1px solid #ddd; }";
    html += "th { background-color: #f2f2f2; }";
    html += "tr:hover { background-color: #f5f5f5; }";

    // Forms
    html += "form { margin-top: 20px; }";
    html += "label { display: block; margin-bottom: 5px; font-weight: bold; }";
    html += "input[type='text'], input[type='password'], input[type='number'], select, textarea { ";
    html += "  width: 100%; padding: 10px; margin-bottom: 15px; border: 1px solid #ddd; border-radius: 4px; font-size: 14px; }";
    html += "input[type='submit'], button, .btn { ";
    html += "  background: #0066cc; color: white; border: none; padding: 10px 15px; border-radius: 4px; cursor: pointer; ";
    html += "  text-decoration: none; display: inline-block; font-size: 14px; margin-right: 10px; }";
    html += "input[type='submit']:hover, button:hover, .btn:hover { background: #0055aa; }";
    html += ".btn-delete { background: #cc0000; }";
    html += ".btn-delete:hover { background: #aa0000; }";

    // Responsive design
    html += "@media screen and (max-width: 600px) {";
    html += "  nav a:not(:first-child) { display: none; }";
    html += "  nav a.icon { float: right; display: block; }";
    html += "  nav.responsive { position: relative; }";
    html += "  nav.responsive a.icon { position: absolute; right: 0; top: 0; }";
    html += "  nav.responsive a { float: none; display: block; text-align: left; }";
    html += "}";

    // Special styles for logs
    html += ".log-container { background: #f8f8f8; padding: 10px; border-radius: 4px; max-height: 70vh; overflow-y: auto; }";
    html += ".log-entry { padding: 5px; border-bottom: 1px solid #ddd; font-family: monospace; white-space: pre-wrap; }";
    html += ".log-error { color: #ff5555; }";
    html += ".log-warning { color: #ffaa00; }";
    html += ".log-info { color: #2196F3; }";
    html += ".log-debug { color: #4CAF50; }";
    html += ".log-verbose { color: #9E9E9E; }";

    // Footer
    html += "footer { background: #f2f2f2; padding: 10px; text-align: center; font-size: 12px; color: #666; }";

    html += "</style>";
}

// Adding JavaScript for interactive elements
void HTMLGenerator::addJavaScript(String &html)
{
    html += "<script>";

    // Responsive menu
    html += "function toggleMenu() {";
    html += "  var x = document.getElementsByTagName('nav')[0];";
    html += "  if (x.className === '') {";
    html += "    x.className = 'responsive';";
    html += "  } else {";
    html += "    x.className = '';";
    html += "  }";
    html += "}";

    // Automatic page refresh
    html += "function startAutoRefresh(interval) {";
    html += "  setTimeout(function(){ location.reload(); }, interval);";
    html += "}";

    html += "</script>";
}

// Adding navigation
void HTMLGenerator::addNavigation(String &html, const String &activePage)
{
    html += "<nav>";
    html += "<a href='/' class='" + String(activePage == "Home" ? "active" : "") + "'>Home</a>";
    html += "<a href='/config' class='" + String(activePage == "Configuration" ? "active" : "") + "'>WiFi Setup</a>";
    html += "<a href='/sensors' class='" + String(activePage == "Sensors" ? "active" : "") + "'>Sensors</a>";
    html += "<a href='/mqtt' class='" + String(activePage == "MQTT" ? "active" : "") + "'>MQTT</a>";
    html += "<a href='/logs' class='" + String(activePage == "Logs" ? "active" : "") + "'>Logs</a>";
    html += "<a href='/api' class='" + String(activePage == "API" ? "active" : "") + "'>API</a>";
    html += "<a href='/update'>Update</a>";
    html += "<a href='/reboot' onclick=\"return confirm('Are you sure you want to reboot the device?');\">Reboot</a>";
    html += "<a href='javascript:void(0);' class='icon' onclick='toggleMenu()'>&#9776;</a>";
    html += "</nav>";

    html += "<div class='container'>";
}

// In HTMLGenerator.cpp, modify the generateHomePage method
String HTMLGenerator::generateHomePage(const std::vector<SensorData> &sensors, const NetworkManager &networkManager)
{
    String html;

    // Add header with reduced content
    addHtmlHeader(html, "Home");

    // Status card
    html += "<div class='card'>";
    html += "<h2>System Status</h2>";

    html += "<p><strong>Mode:</strong> " + String(networkManager.isWiFiConnected() ? "Client" : "Access Point") + "</p>";

    if (networkManager.isWiFiConnected())
    {
        html += "<p><strong>WiFi:</strong> Connected to " + networkManager.getWiFiSSID() + "</p>";
        html += "<p><strong>IP:</strong> " + networkManager.getWiFiIP().toString() + "</p>";
    }
    else
    {
        html += "<p><strong>WiFi:</strong> Disconnected</p>";
        if (networkManager.getWiFiMode() == WIFI_AP)
        {
            html += "<p><strong>AP IP:</strong> " + networkManager.getWiFiAPIP().toString() + "</p>";
        }
    }

    html += "<p><strong>WiFi MAC:</strong> " + networkManager.getWiFimacAddress() + "</p>";


    // Current time
    struct tm timeinfo;
    if (getLocalTime(&timeinfo))
    {
        char timeStr[64];
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
        html += "<p><strong>Time:</strong> " + String(timeStr) + "</p>";
    }
    else
    {
        html += "<p><strong>Time:</strong> Not set</p>";
    }

    unsigned long seconds = millis() / 1000;
    unsigned int days = seconds / 86400;           // 24*60*60
    unsigned int hours = (seconds % 86400) / 3600; // rest of the day
    unsigned int minutes = (seconds % 3600) / 60;  // rest of the hour
    unsigned int secs = seconds % 60;

    html += "<p><strong>Uptime:</strong> ";
    if (days)
        html += String(days) + " d ";
    if (days || hours)
        html += String(hours) + " h ";
    if (days || hours || minutes)
        html += String(minutes) + " m ";
    html += String(secs) + " s</p>";

    html += "</div>";

    // Active sensors - optimize by checking vector size first
    if (!sensors.empty())
    {
        html += "<div class='card'>";
        html += "<h2>Active Sensors</h2>";

        // Generate table with minimal content
        html += "<table>";
        html += "<tr><th>Name</th><th>Type</th><th>Last Seen</th><th>Data</th></tr>";

        for (const auto &sensor : sensors)
        {
            if (sensor.configured)
            {
                html += "<tr>";
                html += "<td>" + sensor.name + "</td>";
                html += "<td>" + sensorTypeToString(sensor.deviceType) + "</td>";
                html += "<td>" + sensor.getLastSeenString() + "</td>";

                // Add sensor data - use the getDataString() method
                html += "<td>" + sensor.getDataString() + "</td>";

                html += "</tr>";

                // Yield to other tasks periodically during table generation
                if (&sensor != &sensors.back())
                {
                    yield();
                }
            }
        }

        html += "</table>";
        html += "</div>";
    }

    // Auto-refresh with longer interval in AP mode
    html += "<script>startAutoRefresh(60000);</script>"; // 60 seconds instead of 30

    // Add footer
    addHtmlFooter(html);

    return html;
}

// Generating sensor table (optimized version for buffer)
void HTMLGenerator::generateSensorTable(char *buffer, size_t &maxLen, const std::vector<SensorData> &sensors)
{
    size_t contentLen = 0;

    // Table start
    contentLen += snprintf(buffer + contentLen, maxLen - contentLen,
                           "<table>"
                           "<tr><th>Name</th><th>Type</th><th>Serial Number</th><th>Last Seen</th><th>Sensor Data</th></tr>");

    // Process all sensors
    for (const auto &sensor : sensors)
    {
        if (sensor.configured)
        {
            contentLen += snprintf(buffer + contentLen, maxLen - contentLen,
                                   "<tr>"
                                   "<td>%s</td>"
                                   "<td>%s</td>"
                                   "<td>%X</td>",
                                   sensor.name.c_str(),
                                   sensor.getTypeInfo().name,
                                   sensor.serialNumber);

            // Last Seen
            contentLen += snprintf(buffer + contentLen, maxLen - contentLen,
                                   "<td>%s</td>",
                                   sensor.getLastSeenString().c_str());

            // Sensor Data
            contentLen += snprintf(buffer + contentLen, maxLen - contentLen,
                                   "<td>%s</td>"
                                   "</tr>",
                                   sensor.getDataString().c_str());
        }
    }

    // Table end
    contentLen += snprintf(buffer + contentLen, maxLen - contentLen, "</table>");
}

// Generating configuration page
String HTMLGenerator::generateConfigPage(const String &ssid, const String &password, bool configMode, const String &ip, const String &timezone, const NetworkManager &networkManager)
{
    String html;

    // Add header - pass configMode to suppress navigation in AP mode
    addHtmlHeader(html, "Configuration", configMode);

    // Current status
    html += "<div class='card'>";
    html += "<h2>Current Status</h2>";
    if (configMode)
    {
        html += "<p><strong>Mode:</strong> Access Point</p>";
        html += "<p><strong>AP Name:</strong> " + networkManager.getWiFiAPSSID() + "</p>";
        html += "<p><strong>AP IP:</strong> " + networkManager.getWiFiAPIP().toString() + "</p>";
    }
    else
    {
        html += "<p><strong>Mode:</strong> Client</p>";
        html += "<p><strong>SSID:</strong> " + ssid + "</p>";
        html += "<p><strong>Status:</strong> " + String(networkManager.isWiFiConnected() ? "Connected" : "Disconnected") + "</p>";
        html += "<p><strong>MAC:</strong> " + networkManager.getWiFimacAddress() + "</p>";
        if (networkManager.isWiFiConnected())
        {
            html += "<p><strong>IP:</strong> " + networkManager.getWiFiIP().toString() + "</p>";
        }
    }
    html += "</div>";

    // WiFi configuration form
    html += "<div class='card'>";
    html += "<h2>WiFi Settings</h2>";
    html += "<form method='post' action='/config'>";
    html += "<label for='ssid'>SSID:</label>";
    html += "<input type='text' id='ssid' name='ssid' value='" + ssid + "' required>";
    html += "<label for='password'>Password:</label>";
    html += "<input type='password' id='password' name='password' value='" + password + "'>";
    html += "<label for='timezone'>Timezone:</label>";
    html += "<select id='timezone' name='timezone'>";

    html += "<option value='GMT0'";
    if (timezone == "GMT0")
        html += " selected";
    html += ">GMT/UTC (00:00)</option>";

    html += "<option value='WET0WEST,M3.5.0/1,M10.5.0'";
    if (timezone == "WET0WEST,M3.5.0/1,M10.5.0")
        html += " selected";
    html += ">Western European (WET/WEST)</option>";

    html += "<option value='CET-1CEST,M3.5.0,M10.5.0/3'";
    if (timezone == "CET-1CEST,M3.5.0,M10.5.0/3")
        html += " selected";
    html += ">Central European (CET/CEST)</option>";

    html += "<option value='EET-2EEST,M3.5.0/3,M10.5.0/4'";
    if (timezone == "EET-2EEST,M3.5.0/3,M10.5.0/4")
        html += " selected";
    html += ">Eastern European (EET/EEST)</option>";

    html += "<option value='MSK-3'";
    if (timezone == "MSK-3")
        html += " selected";
    html += ">Moscow Time (MSK)</option>";

    html += "<input type='submit' value='Save and Restart'>";
    html += "</form>";
    html += "</div>";

    // Add footer
    addHtmlFooter(html);

    return html;
}
// Generate MQTT Configuration Page
String HTMLGenerator::generateMqttPage(const String &host, int port, const String &user, const String &password, bool enabled, bool tls,
                                       const String &prefix, bool haEnabled, String &haPrefix)
{
    String html;

    // Add header
    addHtmlHeader(html, "MQTT Configuration");

    // MQTT configuration form
    html += "<div class='card'>";
    html += "<h2>MQTT Settings</h2>";
    html += "<p>Configure connection to Home Assistant MQTT broker for automatic sensor discovery.</p>";
    html += "<form method='post' action='/mqtt'>";

    // Enable MQTT checkbox
    html += "<div class='form-group'>";
    html += "<label for='enabled'>Enable MQTT:</label>";
    html += "<input type='checkbox' id='enabled' name='enabled' value='1'" + String(enabled ? " checked" : "") + ">";
    html += "</div>";

    // TLS MQTTS checkbox
    html += "<div class='form-group'>";
    html += "<label for='tls'>Enable TLS:</label>";
    html += "<input type='checkbox' id='tls' name='tls' value='0'" + String(tls ? " checked" : "") + ">";
    html += "</div>";

    // MQTT Broker Host
    html += "<div class='form-group'>";
    html += "<label for='host'>MQTT Broker Host:</label>";
    html += "<input type='text' id='host' name='host' value='" + host + "' required>";
    html += "</div>";

    // MQTT Port
    html += "<div class='form-group'>";
    html += "<label for='port'>MQTT Port:</label>";
    html += "<input type='number' id='port' name='port' value='" + String(port) + "' required min='1' max='65535'>";
    html += "</div>";

    // Username
    html += "<div class='form-group'>";
    html += "<label for='user'>Username (optional):</label>";
    html += "<input type='text' id='user' name='user' value='" + user + "'>";
    html += "</div>";

    // Password
    html += "<div class='form-group'>";
    html += "<label for='password'>Password (optional):</label>";
    html += "<input type='password' id='password' name='password' value='" + password + "'>";
    html += "</div>";

    // Root prefix
    html += "<div class='form-group'>";
    html += "<label for='prefix'>Root topic:</label>";
    html += "<input type='text' id='prefix' name='prefix' value='" + prefix + "'>";
    html += "</div>";

    // Home assistant discovery checkbox
    html += "<div class='form-group'>";
    html += "<label for='tls'>Enable HA Discovery:</label>";
    html += "<input type='checkbox' id='haEnabled' name='haEnabled' value='0'" + String(haEnabled ? " checked" : "") + ">";
    html += "</div>";

    // HA Prefix
    html += "<div class='form-group'>";
    html += "<label for='haPrefix'>HA discovery topic:</label>";
    html += "<input type='text' id='haPrefix' name='haPrefix' value='" + haPrefix + "'>";
    html += "</div>";

    html += "<input type='submit' value='Save MQTT Settings'>";
    html += "</form></div>";

    // Add footer
    addHtmlFooter(html);
    return html;
}

// Generating page with sensor list
String HTMLGenerator::generateSensorsPage(const std::vector<SensorData> &sensors)
{
    String html;
    // Adding header
    addHtmlHeader(html, "Sensors");

    // Sensor list
    html += "<div class='card'>";
    html += "<h2>Configured Sensors</h2>";

    if (sensors.empty())
    {
        html += "<p>No sensors configured yet.</p>";
    }
    else
    {
        html += "<table>";
        html += "<tr><th>Name</th><th>Type</th><th>Serial Number</th><th>Last Seen</th><th>Actions</th></tr>";

        for (size_t i = 0; i < sensors.size(); i++)
        {
            const auto &sensor = sensors[i];
            if (sensor.configured)
            {
                html += "<tr>";
                html += "<td>" + sensor.name + "</td>";
                html += "<td>" + sensorTypeToString(sensor.deviceType) + "</td>";
                html += "<td>" + String(sensor.serialNumber, HEX) + "</td>";
                html += "<td>" + sensor.getLastSeenString() + "</td>";
                html += "<td>";
                html += "<a href='/sensors/edit?index=" + String(i) + "' class='btn'>Edit</a> ";
                html += "<a href='/sensors/delete?index=" + String(i) + "' class='btn btn-delete' onclick='return confirm(\"Are you sure you want to delete this sensor?\")'>Delete</a>";
                html += "</td>";
                html += "</tr>";
            }
        }

        html += "</table>";
    }

    html += "<p><a href='/sensors/add' class='btn'>Add New Sensor</a></p>";
    html += "</div>";

    // Adding footer
    addHtmlFooter(html);

    return html;
}

// Generating page for adding a sensor
String HTMLGenerator::generateSensorAddPage()
{
    String html;

    // Adding header
    addHtmlHeader(html, "Add Sensor");

    // Form for adding a sensor
    html += "<div class='card'>";
    html += "<h2>Add New Sensor</h2>";
    html += "<form method='post' action='/sensors/add'>";

    // Sensor name
    html += "<label for='name'>Sensor Name:</label>";
    html += "<input type='text' id='name' name='name' required>";

    // Sensor type
    html += "<label for='deviceType'>Device Type:</label>";
    html += "<select id='deviceType' name='deviceType'>";

    // Dynamically generate sensor types from definition
    for (const auto &type : SENSOR_TYPE_DEFINITIONS)
    {
        if (type.type != SensorType::UNKNOWN)
        {
            html += "<option value='" + String(static_cast<uint8_t>(type.type)) + "'>" +
                    String(type.name);

            // Adding information about sensor capabilities
            html += " - ";
            bool first = true;

            if (type.hasTemperature)
            {
                html += "Temperature";
                first = false;
            }

            if (type.hasHumidity)
            {
                html += (first ? "" : ", ") + String("Humidity");
                first = false;
            }

            if (type.hasPressure)
            {
                html += (first ? "" : ", ") + String("Pressure");
                first = false;
            }

            if (type.hasPPM)
            {
                html += (first ? "" : ", ") + String("CO2");
                first = false;
            }

            if (type.hasLux)
            {
                html += (first ? "" : ", ") + String("Light");
            }

            if (type.hasRainAmount)
            {
                html += (first ? "" : ", ") + String("Rain");
            }
            if (type.hasWindSpeed)
            {
                html += (first ? "" : ", ") + String("Wind");
            }

            html += "</option>";
        }
    }

    html += "</select>";

    // Serial number
    html += "<label for='serialNumber'>Serial Number:</label>";
    html += "<input type='text' id='serialNumber' name='serialNumber' placeholder='e.g. 1234567A' required>";

    // Device key
    html += "<label for='deviceKey'>Device Key:</label>";
    html += "<input type='text' id='deviceKey' name='deviceKey' placeholder='e.g. DEADBEEF' required>";

    // New code with a single field
    html += "<label for='customUrl'>Custom URL with Placeholders (Optional):</label>";
    html += "<div id='urlHelp' style='margin-bottom: 10px; font-size: 0.9em;'>";
    html += "<p>Available placeholders:</p>";
    html += "<style>";
    html += ".placeholder-grid { display: grid; grid-template-columns: repeat(3, 1fr); gap: 8px; margin-bottom: 15px; }";
    html += ".placeholder-item { background: #f5f5f5; padding: 8px; border-radius: 4px; }";
    html += ".placeholder-item code { background: #e0e0e0; padding: 2px 4px; border-radius: 3px; font-family: monospace; color: #0066cc; }";
    html += "</style>";
    html += "<div class='placeholder-grid' style='display: grid; grid-template-columns: repeat(3, 1fr); gap: 5px;'>";
    // Environment placeholders
    html += "<div id='tempPlaceholder' class='placeholder-item'>Temperature <code>*TEMP*</code></div>";
    html += "<div id='humPlaceholder' class='placeholder-item'>Humidity <code>*HUM*</code></div>";
    html += "<div id='pressPlaceholder' class='placeholder-item'>Pressure <code>*PRESS*</code></div>";
    html += "<div id='ppmPlaceholder' class='placeholder-item'>CO2 <code>*PPM*</code></div>";
    html += "<div id='luxPlaceholder' class='placeholder-item'>Light <code>*LUX*</code></div>";
    // Weather specific
    html += "<div id='windSpeedPlaceholder' class='placeholder-item'>Wind Speed <code>*WIND_SPEED*</code></div>";
    html += "<div id='windDirPlaceholder' class='placeholder-item'>Wind Direction <code>*WIND_DIR*</code></div>";
    html += "<div id='rainPlaceholder' class='placeholder-item'>Rain Amount <code>*RAIN*</code></div>";
    html += "<div id='dailyRainPlaceholder' class='placeholder-item'>Rain since midnight <code>*DAILY_RAIN*</code></div>";
    html += "<div id='rainRatePlaceholder' class='placeholder-item'>Rain Rate <code>*RAIN_RATE*</code></div>";
    // Device info
    html += "<div class='placeholder-item'>Battery <code>*BAT*</code></div>";
    html += "<div class='placeholder-item'>Signal <code>*RSSI*</code></div>";
    html += "<div class='placeholder-item'>Serial Number <code>*SN*</code></div>";
    html += "<div class='placeholder-item'>Device Type <code>*TYPE*</code></div>";
    html += "</div>"; // end of grid
    html += "</div>"; // end of urlHelp
    html += "<input type='text' id='customUrl' name='customUrl' placeholder='https://example.com/api?temp=*TEMP*&hum=*HUM*'>";

    html += "<h3 style='margin-top: 20px;'>Sensor Reading Corrections</h3>";
    html += "<p>Adjust sensor readings by adding offsets or applying multipliers. Leave at 0 for no correction.</p>";

    // Create a container for correction inputs with two columns
    html += "<div style='display: grid; grid-template-columns: 1fr 1fr; gap: 10px;'>";

    // Add the correction fields with ids that match the dynamic visibility code
    html += "<div id='tempCorrDiv' style='display: none;'>";
    html += "<label for='tempCorr'>Temperature Correction (±°C):</label>";
    html += "<input type='number' id='tempCorr' name='tempCorr' value='0.0' step='0.1'>";
    html += "<small>Positive values increase the reading, negative values decrease it</small>";
    html += "</div>";

    // Add similar divs for other correction fields with default values
    // Humidity
    html += "<div id='humCorrDiv' style='display: none;'>";
    html += "<label for='humCorr'>Humidity Correction (±%):</label>";
    html += "<input type='number' id='humCorr' name='humCorr' value='0.0' step='0.1'>";
    html += "<small>Positive values increase the reading, negative values decrease it</small>";
    html += "</div>";

    // Pressure
    html += "<div id='pressCorrDiv' style='display: none;'>";
    html += "<label for='pressCorr'>Pressure Correction (±hPa):</label>";
    html += "<input type='number' id='pressCorr' name='pressCorr' value='0.0' step='0.1'>";
    html += "<small>Positive values increase the reading, negative values decrease it</small>";
    html += "</div>";

    // CO2
    html += "<div id='ppmCorrDiv' style='display: none;'>";
    html += "<label for='ppmCorr'>CO2 Correction (±ppm):</label>";
    html += "<input type='number' id='ppmCorr' name='ppmCorr' value='0' step='1'>";
    html += "<small>Positive values increase the reading, negative values decrease it</small>";
    html += "</div>";

    // Lux
    html += "<div id='luxCorrDiv' style='display: none;'>";
    html += "<label for='luxCorr'>Light Correction (±lux):</label>";
    html += "<input type='number' id='luxCorr' name='luxCorr' value='0.0' step='0.1'>";
    html += "<small>Positive values increase the reading, negative values decrease it</small>";
    html += "</div>";

    // Wind speed
    html += "<div id='windSpeedCorrDiv' style='display: none;'>";
    html += "<label for='windSpeedCorr'>Wind Speed Correction (multiplier):</label>";
    html += "<input type='number' id='windSpeedCorr' name='windSpeedCorr' value='1.0' step='0.01' min='0.1' max='10'>";
    html += "<small>Values greater than 1.0 increase the reading, less than 1.0 decrease it</small>";
    html += "</div>";

    // Wind direction
    html += "<div id='windDirCorrDiv' style='display: none;'>";
    html += "<label for='windDirCorr'>Wind Direction Correction (±degrees):</label>";
    html += "<input type='number' id='windDirCorr' name='windDirCorr' value='0' step='1' min='-180' max='180'>";
    html += "<small>Positive values rotate clockwise, negative counter-clockwise</small>";
    html += "</div>";

    // Rain amount
    html += "<div id='rainAmountCorrDiv' style='display: none;'>";
    html += "<label for='rainAmountCorr'>Rain Amount Correction (multiplier):</label>";
    html += "<input type='number' id='rainAmountCorr' name='rainAmountCorr' value='1.0' step='0.01' min='0.1' max='10'>";
    html += "<small>Values greater than 1.0 increase the reading, less than 1.0 decrease it</small>";
    html += "</div>";

    // Rain rate
    html += "<div id='rainRateCorrDiv' style='display: none;'>";
    html += "<label for='rainRateCorr'>Rain Rate Correction (multiplier):</label>";
    html += "<input type='number' id='rainRateCorr' name='rainRateCorr' value='1.0' step='0.01' min='0.1' max='10'>";
    html += "<small>Values greater than 1.0 increase the reading, less than 1.0 decrease it</small>";
    html += "</div>";

    // Altitude correction
    html += "<div id='altitudeCorrDiv' style='display: none;'>";
    html += "<label for='altitude'>Altitude (m) - For pressure adjustment:</label>";
    html += "<input type='number' id='altitude' name='altitude' value='0' min='0' max='8848'>";
    html += "<small>Used to convert relative pressure to absolute pressure</small>";
    html += "</div>";

    html += "</div>"; // End of grid

    // Buttons
    html += "<input type='submit' value='Add Sensor'>";
    html += "<a href='/sensors' class='btn' style='background: #999;'>Cancel</a>";
    html += "</form>";
    html += "</div>";
    html += "<script>";
    html += "document.addEventListener('DOMContentLoaded', function() {";
    html += "  var deviceTypeSelect = document.getElementById('deviceType');";
    html += "  var altitudeDiv = document.getElementById('altitudeDiv');";
    html += "  function updateFieldVisibility() {";
    html += "    var type = parseInt(deviceTypeSelect.value);";
    html += "    console.log('Selected device type:', type);";

    // Define which devices have which capabilities
    html += "    var tempDevices = [1, 2, 3, 81];"; // BME280, SCD40, METEO, DIY_TEMP
    html += "    var humDevices = [1, 2, 3];";      // BME280, SCD40, METEO
    html += "    var pressureDevices = [1, 3];";    // BME280, METEO
    html += "    var co2Devices = [2];";            // SCD40
    html += "    var luxDevices = [4];";            // VEML7700
    html += "    var weatherDevices = [3];";        // METEO

    // Update regular fields
    html += "    document.getElementById('altitudeCorrDiv').style.display = pressureDevices.includes(type) ? 'block' : 'none';";

    // Update correction fields
    html += "    document.getElementById('tempCorrDiv').style.display = tempDevices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('humCorrDiv').style.display = humDevices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('pressCorrDiv').style.display = pressureDevices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('ppmCorrDiv').style.display = co2Devices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('luxCorrDiv').style.display = luxDevices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('windSpeedCorrDiv').style.display = weatherDevices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('windDirCorrDiv').style.display = weatherDevices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('rainAmountCorrDiv').style.display = weatherDevices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('rainRateCorrDiv').style.display = weatherDevices.includes(type) ? 'block' : 'none';";

    // Update placeholder visibility
    html += "    updatePlaceholderVisibility();";
    html += "  }";

    html += "  function updatePlaceholderVisibility() {";
    html += "    var type = parseInt(deviceTypeSelect.value);";
    html += "    console.log('Selected device type:', type);"; // Debugging

    // Device type definitions for individual placeholders
    html += "    var tempDevices = [1, 2, 3, 81];"; // BME280, SCD40, METEO, DIY_TEMP
    html += "    var humDevices = [1, 2, 3];";      // BME280, SCD40, METEO
    html += "    var pressureDevices = [1, 3];";    // BME280, METEO
    html += "    var co2Devices = [2];";            // SCD40
    html += "    var luxDevices = [4];";            // VEML7700
    html += "    var weatherDevices = [3];";        // METEO

    // Update display
    html += "    document.getElementById('tempPlaceholder').style.display = tempDevices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('humPlaceholder').style.display = humDevices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('pressPlaceholder').style.display = pressureDevices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('ppmPlaceholder').style.display = co2Devices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('luxPlaceholder').style.display = luxDevices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('windSpeedPlaceholder').style.display = weatherDevices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('windDirPlaceholder').style.display = weatherDevices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('rainPlaceholder').style.display = weatherDevices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('dailyRainPlaceholder').style.display = weatherDevices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('rainRatePlaceholder').style.display = weatherDevices.includes(type) ? 'block' : 'none';";
    html += "  }";

    // Run updateFieldVisibility when the page loads and when the type changes
    html += "  updateFieldVisibility();";
    html += "  deviceTypeSelect.addEventListener('change', updateFieldVisibility);";
    html += "});";
    html += "</script>";
    // Adding footer
    addHtmlFooter(html);

    return html;
}

// Generating page for editing a sensor
String HTMLGenerator::generateSensorEditPage(const SensorData &sensor, int index)
{
    String html;

    // Adding header
    addHtmlHeader(html, "Edit Sensor");

    // Form for editing a sensor
    html += "<div class='card'>";
    html += "<h2>Edit Sensor</h2>";
    html += "<form method='post' action='/sensors/update'>";

    // Hidden index
    html += "<input type='hidden' name='index' value='" + String(index) + "'>";

    // Sensor name
    html += "<label for='name'>Sensor Name:</label>";
    html += "<input type='text' id='name' name='name' value='" + sensor.name + "' required>";

    // Sensor type
    html += "<label for='deviceType'>Device Type:</label>";
    html += "<select id='deviceType' name='deviceType'>";

    // Dynamically generate sensor types from definition
    for (const auto &type : SENSOR_TYPE_DEFINITIONS)
    {
        if (type.type != SensorType::UNKNOWN)
        {
            bool isSelected = (type.type == sensor.deviceType);
            html += "<option value='" + String(static_cast<uint8_t>(type.type)) + "'";
            if (isSelected)
            {
                html += " selected";
            }
            html += ">" + String(type.name);

            // Adding information about sensor capabilities
            html += " - ";
            bool first = true;

            if (type.hasTemperature)
            {
                html += "Temperature";
                first = false;
            }

            if (type.hasHumidity)
            {
                html += (first ? "" : ", ") + String("Humidity");
                first = false;
            }

            if (type.hasPressure)
            {
                html += (first ? "" : ", ") + String("Pressure");
                first = false;
            }

            if (type.hasPPM)
            {
                html += (first ? "" : ", ") + String("CO2");
                first = false;
            }

            if (type.hasLux)
            {
                html += (first ? "" : ", ") + String("Light");
            }

            if (type.hasRainAmount)
            {
                html += (first ? "" : ", ") + String("Rain");
            }
            if (type.hasWindSpeed)
            {
                html += (first ? "" : ", ") + String("Wind");
            }

            html += "</option>";
        }
    }

    html += "</select>";

    // Serial number
    html += "<label for='serialNumber'>Serial Number:</label>";
    html += "<input type='text' id='serialNumber' name='serialNumber' value='" + String(sensor.serialNumber, HEX) + "' required>";

    // Device key
    html += "<label for='deviceKey'>Device Key:</label>";
    html += "<input type='text' id='deviceKey' name='deviceKey' value='" + String(sensor.deviceKey, HEX) + "' required>";

    // New code with a single field
    html += "<label for='customUrl'>Custom URL with Placeholders (Optional):</label>";
    html += "<div id='urlHelp' style='margin-bottom: 10px; font-size: 0.9em;'>";
    html += "<p>Available placeholders:</p>";

    html += "<style>";
    html += ".placeholder-grid { display: grid; grid-template-columns: repeat(3, 1fr); gap: 8px; margin-bottom: 15px; }";
    html += ".placeholder-item { background: #f5f5f5; padding: 8px; border-radius: 4px; }";
    html += ".placeholder-item code { background: #e0e0e0; padding: 2px 4px; border-radius: 3px; font-family: monospace; color: #0066cc; }";
    html += "</style>";
    html += "<div class='placeholder-grid' style='display: grid; grid-template-columns: repeat(3, 1fr); gap: 5px;'>";
    // Environment placeholders
    html += "<div id='tempPlaceholder' class='placeholder-item'>Temperature <code>*TEMP*</code></div>";
    html += "<div id='humPlaceholder' class='placeholder-item'>Humidity <code>*HUM*</code></div>";
    html += "<div id='pressPlaceholder' class='placeholder-item'>Pressure <code>*PRESS*</code></div>";
    html += "<div id='ppmPlaceholder' class='placeholder-item'>CO2 <code>*PPM*</code></div>";
    html += "<div id='luxPlaceholder' class='placeholder-item'>Light <code>*LUX*</code></div>";
    // Weather specific
    html += "<div id='windSpeedPlaceholder' class='placeholder-item'>Wind Speed <code>*WIND_SPEED*</code></div>";
    html += "<div id='windDirPlaceholder' class='placeholder-item'>Wind Direction <code>*WIND_DIR*</code></div>";
    html += "<div id='rainPlaceholder' class='placeholder-item'>Rain Amount <code>*RAIN*</code></div>";
    html += "<div id='dailyRainPlaceholder' class='placeholder-item'>Rain since midnight <code>*DAILY_RAIN*</code></div>";
    html += "<div id='rainRatePlaceholder' class='placeholder-item'>Rain Rate <code>*RAIN_RATE*</code></div>";
    // Device info
    html += "<div class='placeholder-item'>Battery <code>*BAT*</code></div>";
    html += "<div class='placeholder-item'>Signal <code>*RSSI*</code></div>";
    html += "<div class='placeholder-item'>Serial Number <code>*SN*</code></div>";
    html += "<div class='placeholder-item'>Device Type <code>*TYPE*</code></div>";
    html += "</div>"; // end of grid
    html += "</div>"; // end of urlHelp
    html += "<input type='text' id='customUrl' name='customUrl' placeholder='https://example.com/api?temp=*TEMP*&hum=*HUM*' value='" + sensor.customUrl + "'>";

    html += "<h3 style='margin-top: 20px;'>Sensor Reading Corrections</h3>";
    html += "<p>Adjust sensor readings by adding offsets or applying multipliers. Leave at 0 for no correction.</p>";

    // Create a container for correction inputs with two columns
    html += "<div style='display: grid; grid-template-columns: 1fr 1fr; gap: 10px;'>";

    // Temperature correction
    html += "<div id='tempCorrDiv' style='display: " +
            String(sensor.hasTemperature() ? "block" : "none") + ";'>";
    html += "<label for='tempCorr'>Temperature Correction (±°C):</label>";
    html += "<input type='number' id='tempCorr' name='tempCorr' value='" +
            String(sensor.temperatureCorrection, 2) + "' step='0.1'>";
    html += "<small>Positive values increase the reading, negative values decrease it</small>";
    html += "</div>";

    // Humidity correction
    html += "<div id='humCorrDiv' style='display: " +
            String(sensor.hasHumidity() ? "block" : "none") + ";'>";
    html += "<label for='humCorr'>Humidity Correction (±%):</label>";
    html += "<input type='number' id='humCorr' name='humCorr' value='" +
            String(sensor.humidityCorrection, 2) + "' step='0.1'>";
    html += "<small>Positive values increase the reading, negative values decrease it</small>";
    html += "</div>";

    // Pressure correction
    html += "<div id='pressCorrDiv' style='display: " +
            String(sensor.hasPressure() ? "block" : "none") + ";'>";
    html += "<label for='pressCorr'>Pressure Correction (±hPa):</label>";
    html += "<input type='number' id='pressCorr' name='pressCorr' value='" +
            String(sensor.pressureCorrection, 2) + "' step='0.1'>";
    html += "<small>Positive values increase the reading, negative values decrease it</small>";
    html += "</div>";

    // CO2 correction
    html += "<div id='ppmCorrDiv' style='display: " +
            String(sensor.hasPPM() ? "block" : "none") + ";'>";
    html += "<label for='ppmCorr'>CO2 Correction (±ppm):</label>";
    html += "<input type='number' id='ppmCorr' name='ppmCorr' value='" +
            String(sensor.ppmCorrection, 0) + "' step='1'>";
    html += "<small>Positive values increase the reading, negative values decrease it</small>";
    html += "</div>";

    // Light correction
    html += "<div id='luxCorrDiv' style='display: " +
            String(sensor.hasLux() ? "block" : "none") + ";'>";
    html += "<label for='luxCorr'>Light Correction (±lux):</label>";
    html += "<input type='number' id='luxCorr' name='luxCorr' value='" +
            String(sensor.luxCorrection, 1) + "' step='0.1'>";
    html += "<small>Positive values increase the reading, negative values decrease it</small>";
    html += "</div>";

    // Wind speed correction
    html += "<div id='windSpeedCorrDiv' style='display: " +
            String(sensor.hasWindSpeed() ? "block" : "none") + ";'>";
    html += "<label for='windSpeedCorr'>Wind Speed Correction (multiplier):</label>";
    html += "<input type='number' id='windSpeedCorr' name='windSpeedCorr' value='" +
            String(sensor.windSpeedCorrection, 2) + "' step='0.01' min='0.1' max='10'>";
    html += "<small>Values greater than 1.0 increase the reading, less than 1.0 decrease it</small>";
    html += "</div>";

    // Wind direction correction
    html += "<div id='windDirCorrDiv' style='display: " +
            String(sensor.hasWindDirection() ? "block" : "none") + ";'>";
    html += "<label for='windDirCorr'>Wind Direction Correction (±degrees):</label>";
    html += "<input type='number' id='windDirCorr' name='windDirCorr' value='" +
            String(sensor.windDirectionCorrection) + "' step='1' min='-180' max='180'>";
    html += "<small>Positive values rotate clockwise, negative counter-clockwise</small>";
    html += "</div>";

    // Rain amount correction
    html += "<div id='rainAmountCorrDiv' style='display: " +
            String(sensor.hasRainAmount() ? "block" : "none") + ";'>";
    html += "<label for='rainAmountCorr'>Rain Amount Correction (multiplier):</label>";
    html += "<input type='number' id='rainAmountCorr' name='rainAmountCorr' value='" +
            String(sensor.rainAmountCorrection, 2) + "' step='0.01' min='0.1' max='10'>";
    html += "<small>Values greater than 1.0 increase the reading, less than 1.0 decrease it</small>";
    html += "</div>";

    // Rain rate correction
    html += "<div id='rainRateCorrDiv' style='display: " +
            String(sensor.hasRainRate() ? "block" : "none") + ";'>";
    html += "<label for='rainRateCorr'>Rain Rate Correction (multiplier):</label>";
    html += "<input type='number' id='rainRateCorr' name='rainRateCorr' value='" +
            String(sensor.rainRateCorrection, 2) + "' step='0.01' min='0.1' max='10'>";
    html += "<small>Values greater than 1.0 increase the reading, less than 1.0 decrease it</small>";
    html += "</div>";

    // Altitude - show for all pressure sensors
    html += "<div id='altitudeCorrDiv' style='display: " +
            String(sensor.hasPressure() ? "block" : "none") + ";'>";
    html += "<label for='altitude'>Altitude (m) - For pressure adjustment:</label>";
    html += "<input type='number' id='altitude' name='altitude' value='" +
            String(sensor.altitude) + "' min='0' max='8848'>";
    html += "<small>Used to convert relative pressure to absolute pressure</small>";
    html += "</div>";

    html += "</div>"; // End of grid for corrections

    // Buttons
    html += "<div style='margin-top: 20px;'>";
    html += "<input type='submit' value='Update Sensor'>";
    html += "<a href='/sensors' class='btn' style='background: #999;'>Cancel</a>";
    html += "</div>";

    html += "</form>"; // End of form - make sure this comes after all inputs
    html += "</div>";  // End of card

    // Enhanced JavaScript to dynamically update both sensor-specific fields and correction fields
    html += "<script>";
    html += "document.addEventListener('DOMContentLoaded', function() {";
    html += "  var deviceTypeSelect = document.getElementById('deviceType');";
    html += "  var altitudeDiv = document.getElementById('altitudeDiv');";

    // Create an update function that handles both regular fields and correction fields
    html += "  function updateFieldVisibility() {";
    html += "    var type = parseInt(deviceTypeSelect.value);";
    html += "    console.log('Selected device type:', type);";

    // Define which devices have which capabilities
    html += "    var tempDevices = [1, 2, 3, 81];"; // BME280, SCD40, METEO, DIY_TEMP
    html += "    var humDevices = [1, 2, 3];";      // BME280, SCD40, METEO
    html += "    var pressureDevices = [1, 3];";    // BME280, METEO
    html += "    var co2Devices = [2];";            // SCD40
    html += "    var luxDevices = [4];";            // VEML7700
    html += "    var weatherDevices = [3];";        // METEO

    // Update regular fields
    html += "    document.getElementById('altitudeCorrDiv').style.display = pressureDevices.includes(type) ? 'block' : 'none';";

    // Update correction fields
    html += "    document.getElementById('tempCorrDiv').style.display = tempDevices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('humCorrDiv').style.display = humDevices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('pressCorrDiv').style.display = pressureDevices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('ppmCorrDiv').style.display = co2Devices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('luxCorrDiv').style.display = luxDevices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('windSpeedCorrDiv').style.display = weatherDevices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('windDirCorrDiv').style.display = weatherDevices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('rainAmountCorrDiv').style.display = weatherDevices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('rainRateCorrDiv').style.display = weatherDevices.includes(type) ? 'block' : 'none';";

    // Update placeholders visibility (from existing code)
    html += "    updatePlaceholderVisibility();";
    html += "  }";

    // Keep your existing placeholder visibility updating code
    html += "  function updatePlaceholderVisibility() {";
    html += "    var type = parseInt(deviceTypeSelect.value);";
    html += "    console.log('Selected device type:', type);"; // Debugging

    // Device type definitions for individual placeholders
    html += "    var tempDevices = [1, 2, 3, 81];"; // BME280, SCD40, METEO, DIY_TEMP
    html += "    var humDevices = [1, 2, 3];";      // BME280, SCD40, METEO
    html += "    var pressureDevices = [1, 3];";    // BME280, METEO
    html += "    var co2Devices = [2];";            // SCD40
    html += "    var luxDevices = [4];";            // VEML7700
    html += "    var weatherDevices = [3];";        // METEO

    // Update display
    html += "    document.getElementById('tempPlaceholder').style.display = tempDevices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('humPlaceholder').style.display = humDevices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('pressPlaceholder').style.display = pressureDevices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('ppmPlaceholder').style.display = co2Devices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('luxPlaceholder').style.display = luxDevices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('windSpeedPlaceholder').style.display = weatherDevices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('windDirPlaceholder').style.display = weatherDevices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('rainPlaceholder').style.display = weatherDevices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('dailyRainPlaceholder').style.display = weatherDevices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('rainRatePlaceholder').style.display = weatherDevices.includes(type) ? 'block' : 'none';";
    html += "  }";

    // Make sure the form is set to the current sensor type
    html += "  deviceTypeSelect.value = '" + String(static_cast<uint8_t>(sensor.deviceType)) + "';";

    // Run the update function initially and add event listener for changes
    html += "  updateFieldVisibility();";
    html += "  deviceTypeSelect.addEventListener('change', updateFieldVisibility);";
    html += "});";
    html += "</script>";

    // Adding footer
    addHtmlFooter(html);

    return html;
}

// Generating logs page
String HTMLGenerator::generateLogsPage(const LogEntry *logs, size_t logCount, LogLevel currentLevel)
{
    String html;

    // Adding header
    addHtmlHeader(html, "Logs");

    // Log control panel
    html += "<div class='card'>";
    html += "<h2>Log Settings</h2>";

    // Log level selector
    html += "<form method='post' action='/logs/level'>";
    html += "<label for='level'>Log Level:</label>";
    html += "<select id='level' name='level'>";
    html += "<option value='ERROR'" + String(currentLevel == LogLevel::ERROR ? " selected" : "") + ">ERROR</option>";
    html += "<option value='WARNING'" + String(currentLevel == LogLevel::WARNING ? " selected" : "") + ">WARNING</option>";
    html += "<option value='INFO'" + String(currentLevel == LogLevel::INFO ? " selected" : "") + ">INFO</option>";
    html += "<option value='DEBUG'" + String(currentLevel == LogLevel::DEBUG ? " selected" : "") + ">DEBUG</option>";
    html += "<option value='VERBOSE'" + String(currentLevel == LogLevel::VERBOSE ? " selected" : "") + ">VERBOSE</option>";
    html += "</select>";
    html += "<input type='submit' value='Set Level'>";
    html += "</form>";

    // Buttons for working with logs
    html += "<div style='margin-top: 20px;'>";
    html += "<a href='/logs' class='btn'>Refresh</a> ";
    html += "<a href='/logs/clear' class='btn btn-delete' onclick='return confirm(\"Are you sure you want to clear all logs?\")'>Clear Logs</a>";
    html += "</div>";
    html += "</div>";

    // Log content
    html += "<div class='card'>";
    html += "<h2>System Logs</h2>";
    html += "<div class='log-container'>";

    // Using optimized method for log generation
    if (htmlBuffer != nullptr)
    {
        memset(htmlBuffer, 0, htmlBufferSize);
        size_t maxLen = htmlBufferSize;
        generateLogTable(htmlBuffer, maxLen, logs, logCount);
        html += htmlBuffer;
    }
    else
    {
        // Fallback if buffer is not available
        if (logCount > 0)
        {
            for (size_t i = 0; i < logCount; i++)
            {
                size_t index = (logCount - 1 - i); // Display from newest
                String logClass = "log-";

                switch (logs[index].level)
                {
                case LogLevel::ERROR:
                    logClass += "error";
                    break;
                case LogLevel::WARNING:
                    logClass += "warning";
                    break;
                case LogLevel::INFO:
                    logClass += "info";
                    break;
                case LogLevel::DEBUG:
                    logClass += "debug";
                    break;
                case LogLevel::VERBOSE:
                    logClass += "verbose";
                    break;
                default:
                    logClass += "info";
                    break;
                }

                html += "<div class='log-entry " + logClass + "'>" + logs[index].getFormattedLog() + "</div>";
            }
        }
        else
        {
            html += "<div class='log-entry'>No logs to display</div>";
        }
    }

    html += "</div>"; // log-container
    html += "</div>"; // card

    // Auto-refresh for logs
    html += "<script>startAutoRefresh(30000);</script>";

    // Adding footer
    addHtmlFooter(html);

    return html;
}
// Generating log table
// In HTMLGenerator.cpp
void HTMLGenerator::generateLogTable(char *buffer, size_t &maxLen, const LogEntry *logs, size_t logCount)
{
    size_t contentLen = 0;

    if (logCount > 0)
    {
        // Loop through from newest to oldest
        for (size_t i = 0; i < logCount; i++)
        {
            // Calculate correct index in the circular buffer
            // This is critical to display logs correctly
            size_t logIdx = (Logger::getLogIndex() - 1 - i + Logger::getLogBufferSize()) % Logger::getLogBufferSize();

            const char *logClass = "";
            switch (logs[logIdx].level)
            {
            case LogLevel::ERROR:
                logClass = "log-error";
                break;
            case LogLevel::WARNING:
                logClass = "log-warning";
                break;
            case LogLevel::INFO:
                logClass = "log-info";
                break;
            case LogLevel::DEBUG:
                logClass = "log-debug";
                break;
            case LogLevel::VERBOSE:
                logClass = "log-verbose";
                break;
            default:
                logClass = "log-info";
                break;
            }

            contentLen += snprintf(buffer + contentLen, maxLen - contentLen,
                                   "<div class='log-entry %s'>%s</div>",
                                   logClass,
                                   logs[logIdx].getFormattedLog().c_str());

            // Safety check to avoid buffer overflow
            if (contentLen >= maxLen - 100)
            {
                contentLen += snprintf(buffer + contentLen, maxLen - contentLen,
                                       "<div class='log-entry log-warning'>Log output truncated due to buffer size limitations</div>");
                break;
            }
        }
    }
    else
    {
        contentLen += snprintf(buffer + contentLen, maxLen - contentLen,
                               "<div class='log-entry'>No logs to display</div>");
    }
}

// Generating JSON for API
String HTMLGenerator::generateAPIJson(const std::vector<SensorData> &sensors, const NetworkManager &networkManager)
{
    // Estimating JSON document size
    const size_t capacity = JSON_OBJECT_SIZE(5) + JSON_ARRAY_SIZE(sensors.size()) +
                            sensors.size() * JSON_OBJECT_SIZE(15) + 1024; // Extra space for safety

    DynamicJsonDocument doc(capacity);

    // Basic information
    doc["version"] = FIRMWARE_VERSION;

    // Current time
    struct tm timeinfo;
    if (getLocalTime(&timeinfo))
    {
        char timeStr[64];
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
        doc["time"] = timeStr;
    }
    else
    {
        doc["time"] = "Time not set";
    }

    doc["status"] = networkManager.isWiFiConnected() ? "connected" : "disconnected";

    // Sensors array
    JsonArray sensorsArray = doc.createNestedArray("sensors");

    for (const auto &sensor : sensors)
    {
        if (sensor.configured)
        {
            JsonObject sensorObj = sensorsArray.createNestedObject();
            sensor.toJson(sensorObj);
        }
    }

    // Serialization to string
    String result;
    serializeJson(doc, result);
    return result;
}

// Generating API page
String HTMLGenerator::generateAPIPage(const std::vector<SensorData> &sensors)
{
    String html;

    // Adding header
    addHtmlHeader(html, "API");

    // API documentation
    html += "<div class='card'>";
    html += "<h2>API Documentation</h2>";
    html += "<p>This gateway provides a JSON API for accessing sensor data.</p>";

    html += "<h3>Endpoints</h3>";
    html += "<table>";
    html += "<tr><th>URL</th><th>Description</th></tr>";
    html += "<tr><td><code>/api?format=json</code></td><td>Returns all sensor data in JSON format</td></tr>";
    html += "<tr><td><code>/api?format=csv</code></td><td>Returns sensor data in CSV format</td></tr>";
    html += "<tr><td><code>/api?sensor=XXXX</code></td><td>Returns data for a specific sensor by serial number</td></tr>";
    html += "</table>";

    html += "<h3>Example JSON Response</h3>";
    html += "<pre style='background: #f5f5f5; padding: 10px; overflow-x: auto;'>";
    html += "{\n";
    html += "  \"version\": \"" + String(FIRMWARE_VERSION) + "\",\n";
    html += "  \"time\": \"2023-08-01 12:34:56\",\n";
    html += "  \"status\": \"connected\",\n";
    html += "  \"sensors\": [\n";

    // Generating sensor data example
    if (!sensors.empty())
    {
        const auto &sensor = sensors[0];
        html += "    {\n";
        html += "      \"name\": \"" + sensor.name + "\",\n";
        html += "      \"type\": " + String(static_cast<uint8_t>(sensor.deviceType)) + ",\n";
        html += "      \"typeName\": \"" + String(sensor.getTypeInfo().name) + "\",\n";
        html += "      \"serialNumber\": \"" + String(sensor.serialNumber, HEX) + "\",\n";
        html += "      \"lastSeen\": " + (sensor.lastSeen > 0 ? String((millis() - sensor.lastSeen) / 1000) : "-1") + ",\n";

        if (sensor.hasTemperature())
        {
            html += "      \"temperature\": " + String(sensor.temperature, 2) + ",\n";
        }

        if (sensor.hasHumidity())
        {
            html += "      \"humidity\": " + String(sensor.humidity, 2) + ",\n";
        }

        if (sensor.hasPressure())
        {
            html += "      \"pressure\": " + String(sensor.pressure, 2) + ",\n";
        }

        if (sensor.hasPPM())
        {
            html += "      \"ppm\": " + String(sensor.ppm, 0) + ",\n";
        }

        if (sensor.hasLux())
        {
            html += "      \"lux\": " + String(sensor.lux, 1) + ",\n";
        }

        html += "      \"batteryVoltage\": " + String(sensor.batteryVoltage, 2) + ",\n";
        html += "      \"rssi\": " + String(sensor.rssi) + "\n";
        html += "    }\n";
    }
    else
    {
        html += "    {\n";
        html += "      \"name\": \"Example Sensor\",\n";
        html += "      \"type\": 1,\n";
        html += "      \"typeName\": \"CLIMA\",\n";
        html += "      \"serialNumber\": \"123456\",\n";
        html += "      \"lastSeen\": 300,\n";
        html += "      \"temperature\": 21.50,\n";
        html += "      \"humidity\": 45.20,\n";
        html += "      \"pressure\": 1013.20,\n";
        html += "      \"batteryVoltage\": 3.82,\n";
        html += "      \"rssi\": -72\n";
        html += "    }\n";
    }

    html += "  ]\n";
    html += "}\n";
    html += "</pre>";

    html += "<h3>Live API</h3>";
    html += "<p>Access the live API here: <a href='/api?format=json' target='_blank'>/api?format=json</a></p>";
    html += "</div>";

    // Adding footer
    addHtmlFooter(html);

    return html;
}
