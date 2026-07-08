#ifndef LOGGER_H
#define LOGGER_H

#include <FS.h>
#include <SPIFFS.h>
#include "config.h"

class SystemLogger {
public:
    static bool init() {
        if (!SPIFFS.begin(true)) {
            Serial.println("[CRITICAL] SPIFFS Mount Failed.");
            return false;
        }
        return true;
    }

    static void logEvent(const char* eventType, const char* details) {
        // Formulate standard string format
        String logLine = "[" + String(millis()) + "] " + String(eventType) + ": " + String(details) + "\n";
        Serial.print("LOG -> " + logLine);

        // Check for size-based log rotation
        File checkFile = SPIFFS.open(LOG_FILE_PATH, FILE_READ);
        if (checkFile && checkFile.size() > MAX_LOG_SIZE_BYTES) {
            checkFile.close();
            Serial.println("[SYSTEM] Log reached threshold limit. Executing rotation.");
            // Wipe or rotate log
            SPIFFS.remove(LOG_FILE_PATH);
        } else if (checkFile) {
            checkFile.close();
        }

        // Append line to file
        File logFile = SPIFFS.open(LOG_FILE_PATH, FILE_APPEND);
        if (logFile) {
            logFile.print(logLine);
            logFile.close();
        }
    }
};

#endif