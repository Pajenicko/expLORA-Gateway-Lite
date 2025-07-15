
/**
 * expLORA Gateway Lite
 *
 * Network manager implementation file
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

#include "Network_Manager.h"
 
 // Constructor
NetworkManager::NetworkManager(Logger &log)
    : logger(log)
{
 
}

// Destructor
NetworkManager::~NetworkManager()
{

}

// Initialize
bool NetworkManager::init()
{
    logger.info("Initializing Networking...");
    return true;
}


void NetworkManager::process()
{
    if (_wifiAPmode) {
        processDNS();
        _processAPTimeout();
    }
}


// WiFi methods

String NetworkManager::_generateAPSSID()
{
    String macAddress = getWiFimacAddress();
    macAddress.replace(":", ""); // Remove colons
    String apName = "expLORA-GW-" + macAddress.substring(6); // Use last 6 characters of MAC address
    logger.debug("Generated AP SSID: " + apName);
    return apName;
}

void NetworkManager::_processAPTimeout() {
    if (!_wifiAPmode || _apTimeout == 0 || !_wifiSTAmode) {
        return;
    }

    if (millis() - _apStartup > _apTimeout)
    {
        // Time expired, switch to client mode if successfully connected
        if (isWiFiConnected())
        {
            logger.info("AP timeout reached. Switching to client mode only.");
            disableAP();
        }
        else
        {
            // Not connected as a client, keep AP running
            logger.info("AP timeout reached but WiFi client still not connected. Keeping AP mode active.");
            _apStartup = millis(); // Reset timer
        }
    }
}

bool NetworkManager::setupAP(String apName)
{
    _wifiAPmode = true;

    if (apName.isEmpty())
    {
        apName = _generateAPSSID(); // Generate default AP name if not provided
    }

    logger.info("Starting AP mode: " + apName);

    // Full WiFi reset sequence
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(200);

    // AP configuration with optimized parameters
    WiFi.setTxPower(WIFI_POWER_19_5dBm); // Maximum power

    // Set up AP mode with optimized settings
    WiFi.mode(WIFI_AP);

    // AP configuration with 4 clients max and channel 6 (less crowded usually)
    bool apStarted = WiFi.softAP(apName.c_str());
    delay(1000); // Give AP time to fully initialize

    if (apStarted)
    {
        logger.info("AP setup successful");
        _apStartup = millis();
    }
    else
    {
        logger.error("AP setup failed");
        return false;
    }

    IPAddress apIP = getWiFiAPIP();
    logger.info("AP IP assigned: " + apIP.toString());

    // Configure DNS server with custom TTL for faster responses
    dnsServer.setTTL(30); // TTL in seconds (lower value = more responsive)
    dnsServer.start(DNS_PORT, "*", getWiFiAPIP());
    logger.info("DNS server started on port " + String(DNS_PORT));

    return true;
}

bool NetworkManager::disableAP() {
    stopDNS();

    WiFi.mode(_wifiSTAmode ? WIFI_STA : WIFI_OFF);
    _wifiAPmode = false;

    return true;
}

void NetworkManager::setAPTimeout(unsigned long timeout) {
    logger.debug("Setting AP timeout to " + String(timeout / 1000) + " sec");
    _apTimeout = timeout;
}

bool NetworkManager::wifiSTAConnect(String ssid, String psk) {
    _wifiSTAmode = true;

    WiFi.mode(_wifiAPmode ? WIFI_AP_STA : WIFI_STA);

    logger.info("Attempting to connect to WiFi: " + ssid);
    WiFi.begin(ssid.c_str(), psk.c_str());

    int attempts = 0;
    while (!isWiFiConnected() && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();

    if (isWiFiConnected()) {
        logger.info("WiFi connected! IP: " + WiFi.localIP().toString());
        return true;
    }
    else {
        logger.warning("Failed to connect to WiFi after " + String(attempts) + " attempts. SSID: " +
                            ssid);
        return false;
    }
}

bool NetworkManager::wifiSTADisconnect() {
    WiFi.disconnect(true);
    WiFi.mode(_wifiAPmode ? WIFI_AP : WIFI_OFF);

    _wifiSTAmode = false;
    return true;
}

// Process DNS requests
void NetworkManager::processDNS()
{
    dnsServer.processNextRequest();
}

void NetworkManager::stopDNS()
{
    dnsServer.stop();
    logger.info("DNS server stopped");
}

String NetworkManager::getWiFiSSID() const
{
    return WiFi.SSID();
}

String NetworkManager::getWiFiAPSSID() const
{
    return WiFi.softAPSSID();
}

IPAddress NetworkManager::getWiFiIP() const
{
    return WiFi.localIP();
}

IPAddress NetworkManager::getWiFiAPIP() const
{
    return WiFi.softAPIP();
}

wifi_mode_t NetworkManager::getWiFiMode() const
{
    return WiFi.getMode();
}

uint8_t* NetworkManager::getWiFimacAddress(uint8_t* mac)
{
    return WiFi.macAddress(mac);
}

String NetworkManager::getWiFimacAddress(void) const
{
    return WiFi.macAddress();
}

// Check if network is available
bool NetworkManager::isConnected() const
{
    return isWiFiConnected() || isEthernetConnected() || isModemConnected();
}

// Check if WiFi is connected
bool NetworkManager::isWiFiConnected() const
{
    return _wifiSTAmode ? WiFi.status() == WL_CONNECTED : false;
}

bool NetworkManager::isWifiAPActive() const
{
    return _wifiAPmode;
}

// Check if Ethernet is connected
bool NetworkManager::isEthernetConnected() const
{
    // Implement Ethernet connection check logic here
    return false;
}

// Check if Modem is connected
bool NetworkManager::isModemConnected() const
{
    // Implement Modem connection check logic here
    return false;
}