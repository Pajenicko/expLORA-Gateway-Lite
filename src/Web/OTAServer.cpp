#include <ElegantOTA.h>
#include <AsyncTCP.h>
#include "OTAServer.h"

// Constructor
OTAServer::OTAServer(Logger &log, AsyncWebServer &srv) : logger(log), server(srv) { }

void OTAServer::init() {
    ElegantOTA.begin(&server);

    ElegantOTA.setAutoReboot(true);
    ElegantOTA.onStart([this]() {this->onOTAStart();});
    ElegantOTA.onProgress([this](size_t current, size_t final) {this->onOTAProgress(current, final);});
    ElegantOTA.onEnd([this](bool success) {this->onOTAEnd(success);});
}

void OTAServer::process() {
    ElegantOTA.loop();
}

void OTAServer::onOTAStart() {
    logger.info("OTA update started!");
}

void OTAServer::onOTAProgress(size_t current, size_t final) {
    if (millis() - ota_progress_millis > 1000) {
        ota_progress_millis = millis();
        logger.debug("OTA Progress Current: " + String(current) + " bytes, Final: " + String(final) + " bytes");
    }
}

void OTAServer::onOTAEnd(bool success) {
    if (success) {
        logger.info("OTA update finished successfully!");
    } else {
        logger.error("There was an error during OTA update!");
    }
}
