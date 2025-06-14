/**
 * expLORA Gateway Lite
 *
 * Web portal manager header file
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

#include <ESPAsyncWebServer.h>
#include <Arduino.h>
#include "../Data/Logging.h"


class OTAServer {
    private:
        Logger &logger;             // Reference to logger
        AsyncWebServer &server;     // Reference to web server
        unsigned long ota_progress_millis = 0;

        void onOTAStart();
        void onOTAProgress(size_t current, size_t final);
        void onOTAEnd(bool success);

    public:
        // Constructor
        OTAServer(Logger &log, AsyncWebServer &srv);

        // Destructor
        ~OTAServer();

        void init();
        void process();
};
