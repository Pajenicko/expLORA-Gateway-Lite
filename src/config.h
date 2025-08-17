/**
 * expLORA Gateway Lite
 *
 * Main configuration file for the expLORA Gateway
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

// Firmware version
#define FIRMWARE_VERSION "1.0.9"

// LoRa registers
#define REG_FIFO 0x00
#define REG_OP_MODE 0x01
#define REG_FRF_MSB 0x06
#define REG_FRF_MID 0x07
#define REG_FRF_LSB 0x08
#define REG_PA_CONFIG 0x09
#define REG_OCP 0x0B
#define REG_LNA 0x0C
#define REG_FIFO_ADDR_PTR 0x0D
#define REG_FIFO_TX_BASE_ADDR 0x0E
#define REG_FIFO_RX_BASE_ADDR 0x0F
#define REG_FIFO_RX_CURRENT_ADDR 0x10
#define REG_IRQ_FLAGS 0x12
#define REG_RX_NB_BYTES 0x13
#define REG_PKT_SNR_VALUE 0x19
#define REG_PKT_RSSI_VALUE 0x1A
#define REG_MODEM_CONFIG_1 0x1D
#define REG_MODEM_CONFIG_2 0x1E
#define REG_SYMB_TIMEOUT_LSB 0x1F
#define REG_PREAMBLE_MSB 0x20
#define REG_PREAMBLE_LSB 0x21
#define REG_PAYLOAD_LENGTH 0x22
#define REG_MODEM_CONFIG_3 0x26
#define REG_FREQ_ERROR_MSB 0x28
#define REG_FREQ_ERROR_MID 0x29
#define REG_FREQ_ERROR_LSB 0x2A
#define REG_RSSI_WIDEBAND 0x2C
#define REG_DETECTION_OPTIMIZE 0x31
#define REG_INVERTIQ 0x33
#define REG_DETECTION_THRESHOLD 0x37
#define REG_SYNC_WORD 0x39
#define REG_INVERTIQ2 0x3B
#define REG_DIO_MAPPING_1 0x40
#define REG_VERSION 0x42
#define REG_PA_DAC 0x4D

// LoRa modes
#define MODE_LONG_RANGE_MODE 0x80
#define MODE_SLEEP 0x00
#define MODE_STDBY 0x01
#define MODE_TX 0x03
#define MODE_RX_CONTINUOUS 0x05
#define MODE_RX_SINGLE 0x06
#define MODE_CAD 0x07

// Application configuration
#define MAX_SENSORS 20                // Maximum number of sensors
#define LOG_BUFFER_SIZE 200           // Size of log buffer
#define AP_TIMEOUT 300000             // AP mode timeout (5 minutes in milliseconds)
#define WIFI_RECONNECT_INTERVAL 60000 // WiFi reconnect attempt interval (1 minute in milliseconds)
#define WDT_TIMEOUT 10                // Watchdog timeout in seconds

// File system configuration
#define CONFIG_FILE "/config.json"   // Configuration file
#define SENSORS_FILE "/sensors.json" // Sensors file

// NTP configuration
#define NTP_SERVER "pool.ntp.org"
#define DEFAULT_TIMEZONE "CET-1CEST,M3.5.0,M10.5.0/3" // Central European Time with auto DST

// MQTT Configuration
#define MQTT_DEFAULT_HOST "homeassistant.local"
#define MQTT_DEFAULT_PORT 1883
#define MQTT_DEFAULT_USER ""
#define MQTT_DEFAULT_PASS ""
#define MQTT_DEFAULT_ENABLED false
#define MQTT_DEFAULT_TLS false
#define MQTT_RECONNECT_INTERVAL 30000 // 30 seconds in milliseconds
#define MQTT_DEFAULT_PREFIX "explora"
#define HA_DISCOVERY_DEFAULT_ENABLED true
#define HA_DISCOVERY_DEFAULT_PREFIX "homeassistant"

// CORS headers for API
#define CORS_HEADER_NAME "Access-Control-Allow-Origin"
#define CORS_HEADER_VALUE "*"

// Webserver ports
#define HTTP_PORT 80 // HTTP server port
#define DNS_PORT 53  // DNS server port

// Sensor type definitions - easily extendable for additional types
#define SENSOR_TYPE_UNKNOWN  0x00
#define SENSOR_TYPE_BME280   0x01  // Temperature, humidity, pressure
#define SENSOR_TYPE_SCD40    0x02  // Temperature, humidity, CO2
#define SENSOR_TYPE_METEO    0x03  // Meteorological station
#define SENSOR_TYPE_VEML7700 0x04  // Light sensor (LUX)
#define SENSOR_TYPE_DIY_TEMP 0x51  // Temperature
// Add additional sensor types here...

// PSRAM configuration for web server
#define WEB_BUFFER_SIZE 16384 // Reduced to 16KB

// OTA update settings
#define OTA_PASSWORD "admin" // OTA update password
#define OTA_PORT 3232        // OTA update port