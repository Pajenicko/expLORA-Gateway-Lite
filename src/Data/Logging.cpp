/**
 * expLORA Gateway Lite
 *
 * Logging system implementation file
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

#include "Logging.h"
#include <time.h>
#include <esp_heap_caps.h>
#include <Arduino.h>

// Initialization of static variables
LogLevel Logger::currentLevel = LogLevel::INFO;
LogEntry *Logger::logBuffer = nullptr;
size_t Logger::logBufferSize = LOG_BUFFER_SIZE;
size_t Logger::logIndex = 0;
size_t Logger::logCount = 0;
std::mutex Logger::logMutex;
bool Logger::initialized = false;

bool Logger::timeInitialized = false;

void Logger::setTimeInitialized(bool initialized)
{
    timeInitialized = initialized;
}

// Logger initialization
bool Logger::init(size_t bufferSize)
{
    // If already initialized, first free the memory
    if (initialized)
    {
        deinit();
    }

    logBufferSize = bufferSize;

    logBuffer = new LogEntry[logBufferSize];
    Serial.printf("Logger: Allocated %u bytes in RAM for logs\n", sizeof(LogEntry) * logBufferSize);

    // Check if memory allocation was successful
    if (logBuffer == nullptr)
    {
        Serial.println("Logger: Failed to allocate memory for logs");
        return false;
    }

    // Reset indexes and log count
    logIndex = 0;
    logCount = 0;
    initialized = true;

    // Pre-reserve capacity for messages to keep heap usage stable over time
    for (size_t i = 0; i < logBufferSize; ++i)
    {
        logBuffer[i].message.reserve(LOG_MESSAGE_CAPACITY);
        logBuffer[i].formattedTime.reserve(LOG_TIME_CAPACITY);
    }

    // First log after initialization - use char[] instead of String concat
    char initMsg[64];
    snprintf(initMsg, sizeof(initMsg), "Logging system initialized with %u entries",
             logBufferSize);
    info(initMsg);

    return true;
}

// Free logger memory
void Logger::deinit()
{
    if (!initialized)
        return;

    std::lock_guard<std::mutex> lock(logMutex);

    if (logBuffer != nullptr)
    {
        delete[] logBuffer;
        logBuffer = nullptr;
    }

    initialized = false;
}

// Set logging level
void Logger::setLogLevel(LogLevel level)
{
    currentLevel = level;
    info("Log level set to: " + levelToString(level));
}

// Get current logging level
LogLevel Logger::getLogLevel()
{
    return currentLevel;
}

// Get timestamp
String Logger::getTimeStamp()
{
    static char timeBuffer[32];

    if (timeInitialized)
    {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo))
        {
            strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
            return String(timeBuffer);
        }
    }

    // Time not set - use simple static string
    return String("[Time not set]");
}

// Add log with specified level
void Logger::log(LogLevel level, const String &message)
{
    if (!initialized || level > currentLevel)
    {
        return;
    }

    // Get timestamp only once
    String timeStamp = getTimeStamp();
    // String timeStamp = "[Time not set]";

    // Print to serial port
    Serial.print(timeStamp);
    Serial.print(F(" ["));
    Serial.print(levelToString(level));
    Serial.print(F("] "));
    Serial.println(message);

    std::lock_guard<std::mutex> lock(logMutex);

    if (logBuffer != nullptr)
    {
        logBuffer[logIndex].timestamp = millis();
        logBuffer[logIndex].level = level;
        // Store a capped version to ensure we never exceed reserved capacity
        if ((size_t)message.length() > LOG_MESSAGE_CAPACITY)
        {
            // Keep the last 3 chars for ellipsis
            String tmp = message.substring(0, LOG_MESSAGE_CAPACITY - 3);
            tmp += F("...");
            logBuffer[logIndex].message = tmp;
        }
        else
        {
            logBuffer[logIndex].message = message;
        }
        // Formatted time is small; still ensure it won't exceed reserved capacity
        if ((size_t)timeStamp.length() > LOG_TIME_CAPACITY)
        {
            logBuffer[logIndex].formattedTime = timeStamp.substring(0, LOG_TIME_CAPACITY);
        }
        else
        {
            logBuffer[logIndex].formattedTime = timeStamp;
        }

        logIndex = (logIndex + 1) % logBufferSize;
        if (logCount < logBufferSize)
        {
            logCount++;
        }
    }
}

// Helper methods for different log levels
void Logger::error(const String &message)
{
    log(LogLevel::ERROR, message);
}

void Logger::warning(const String &message)
{
    log(LogLevel::WARNING, message);
}

void Logger::info(const String &message)
{
    log(LogLevel::INFO, message);
}

void Logger::debug(const String &message)
{
    log(LogLevel::DEBUG, message);
}

void Logger::verbose(const String &message)
{
    log(LogLevel::VERBOSE, message);
}

// Get all logs (for web interface)
const LogEntry *Logger::getLogs(size_t &count)
{
    std::lock_guard<std::mutex> lock(logMutex);
    count = logCount;
    return logBuffer;
}

// Clear all logs
void Logger::clearLogs()
{
    std::lock_guard<std::mutex> lock(logMutex);

    // Explicitly clear all String objects
    for (size_t i = 0; i < logBufferSize; i++)
    {
        // Force String deallocation by assigning empty strings
        logBuffer[i].message = String();
        logBuffer[i].formattedTime = String();
    }

    logIndex = 0;
    logCount = 0;
}

// Convert log level from string
LogLevel Logger::levelFromString(const String &levelStr)
{
    if (levelStr.equalsIgnoreCase("ERROR"))
        return LogLevel::ERROR;
    if (levelStr.equalsIgnoreCase("WARNING"))
        return LogLevel::WARNING;
    if (levelStr.equalsIgnoreCase("INFO"))
        return LogLevel::INFO;
    if (levelStr.equalsIgnoreCase("DEBUG"))
        return LogLevel::DEBUG;
    if (levelStr.equalsIgnoreCase("VERBOSE"))
        return LogLevel::VERBOSE;

    return LogLevel::INFO;
}

// Convert log level to string
String Logger::levelToString(LogLevel level)
{
    switch (level)
    {
    case LogLevel::ERROR:
        return "ERROR";
    case LogLevel::WARNING:
        return "WARNING";
    case LogLevel::INFO:
        return "INFO";
    case LogLevel::DEBUG:
        return "DEBUG";
    case LogLevel::VERBOSE:
        return "VERBOSE";
    default:
        return "UNKNOWN";
    }
}
