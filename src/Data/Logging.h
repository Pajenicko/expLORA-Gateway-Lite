/**
 * expLORA Gateway Lite
 *
 * Logging system header file
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
#include <mutex>
#include "../config.h"

// Preallocated capacities for in-memory log strings
// Capping stored message length prevents unbounded growth and fragmentation
#ifndef LOG_MESSAGE_CAPACITY
#define LOG_MESSAGE_CAPACITY 300   // bytes reserved per log message in buffer
#endif
#ifndef LOG_TIME_CAPACITY
#define LOG_TIME_CAPACITY 32      // bytes reserved for formatted time string
#endif

/**
 * Definition of a logging system with support for various levels
 *
 * The system supports storing logs in PSRAM (if available) and allows
 * setting the logging level via the web interface. Logs are stored in a circular
 * buffer for efficient memory usage.
 */

// Logging levels
enum class LogLevel
{
    ERROR = 0,   // Only critical errors
    WARNING = 1, // Errors and warnings
    INFO = 2,    // Basic activity information
    DEBUG = 3,   // Detailed information for debugging
    VERBOSE = 4  // All available information
};

// Structure for storing log entries
struct LogEntry
{
    unsigned long timestamp; // Timestamp in milliseconds since start
    LogLevel level;          // Log level
    String message;          // Text message
    String formattedTime;    // Formatted time (if available)

    // Convert log level to text representation
    String getLevelString() const
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

    // Get color for the given log level (for web interface)
    String getLevelColor() const
    {
        switch (level)
        {
        case LogLevel::ERROR:
            return "#ff5555"; // Red
        case LogLevel::WARNING:
            return "#ffaa00"; // Orange
        case LogLevel::INFO:
            return "#2196F3"; // Blue
        case LogLevel::DEBUG:
            return "#4CAF50"; // Green
        case LogLevel::VERBOSE:
            return "#9E9E9E"; // Gray
        default:
            return "#000000"; // Black
        }
    }

    // Format log for UI display
    String getFormattedLog() const
    {
        return formattedTime + " [" + getLevelString() + "] " + message;
    }
};

class Logger
{
private:
    static LogLevel currentLevel;
    static LogEntry *logBuffer;
    static size_t logBufferSize;
    static size_t logIndex;
    static size_t logCount;
    static bool usePSRAM;
    static std::mutex logMutex;
    static bool initialized;
    static bool timeInitialized;

    // Get timestamp (if available)
    static String getTimeStamp();

public:
    // Logger initialization
    static bool init(size_t bufferSize = LOG_BUFFER_SIZE);

    // Free memory on termination
    static void deinit();

    // Set logging level
    static void setLogLevel(LogLevel level);

    // Get current logging level
    static LogLevel getLogLevel();

    // Add log with specified level
    static void log(LogLevel level, const String &message);

    // Helper methods for different log levels
    static void error(const String &message);
    static void warning(const String &message);
    static void info(const String &message);
    static void debug(const String &message);
    static void verbose(const String &message);

    // Get all logs (for web interface)
    static const LogEntry *getLogs(size_t &count);

    // Clear all logs
    static void clearLogs();

    // Convert log level from string
    static LogLevel levelFromString(const String &levelStr);

    // Convert log level to string
    static String levelToString(LogLevel level);

    // Set time initialization state
    static void setTimeInitialized(bool initialized);

    static bool isTimeInitialized() { return timeInitialized; }

    static size_t getLogIndex() { return logIndex; }
    static size_t getLogBufferSize() { return logBufferSize; }
};
