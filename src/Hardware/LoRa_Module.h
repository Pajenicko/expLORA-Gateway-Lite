/**
 * expLORA Gateway Lite
 *
 * LoRa module manager header file
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
#include <SPI.h>
#include "../config.h"
#include "../Data/Logging.h"
#include "SPI_Manager.h"
#include "board_config.h"

/**
 * Class for managing RFM95W LoRa module
 *
 * Handles initialization, configuration, and communication with RFM95W LoRa module.
 * Abstracts SPI communication and interrupt handling.
 */
class LoRaModule
{
private:
    int csPin;              // Chip select pin
    int rstPin;             // Reset pin
    int dio0Pin;            // DIO0 pin for interrupt
    SPIManager *spiManager; // SPI communication manager

    Logger &logger; // Reference to logger

    static volatile bool interruptOccurred;  // Interrupt flag (static for use in ISR)
    static void IRAM_ATTR handleInterrupt(); // Interrupt handler

    // Setup pins and SPI
    bool setupPins();
    // Reset module
    void resetModule();

public:
    // Constructor
    LoRaModule(Logger &log, SPIManager *spiMgr = nullptr, int cs = LORA_CS, int rst = LORA_RST, int dio0 = LORA_DIO0);

    // Destructor
    ~LoRaModule();

    // Initialize LoRa module
    bool init();

    // Reset module
    bool reset();

    // Write to register
    void writeRegister(uint8_t reg, uint8_t value);

    // Read from register
    uint8_t readRegister(uint8_t reg);

    // Receive packet
    bool receivePacket(uint8_t *buffer, uint8_t *length);

    // Get RSSI
    int getRSSI();

    // Return SNR of last packet
    float getSNR();

    // Check if interrupt occurred
    static bool hasInterrupt();

    // Clear interrupt flag
    static void clearInterrupt();

    // Check if module is connected
    bool isConnected();

    // Get chip version
    uint8_t getVersion();
};
