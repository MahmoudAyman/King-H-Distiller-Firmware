#ifndef SYSTEM_MANAGER_H
#define SYSTEM_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include "Config.h"

class SystemManager {
private:
    Preferences prefs;
    uint8_t activeMode;

public:
    SystemManager() : activeMode(MODE_SD) {}

    void begin() {
        Serial.println("[SystemManager] Initializing NVS...");
        prefs.begin("player", false);
        activeMode = prefs.getUChar("mode", MODE_SD);
        Serial.printf("[SystemManager] Last stored mode: %s\n", (activeMode == MODE_BT ? "Bluetooth" : "SD Card"));
    }

    uint8_t getActiveMode() {
        return activeMode;
    }

    void setActiveMode(uint8_t mode) {
        activeMode = mode;
    }

    void storeModeAndReboot(uint8_t newMode) {
        Serial.printf("[SystemManager] Saving new mode (%d) and rebooting...\n", newMode);
        prefs.putUChar("mode", newMode);
        prefs.end();
        delay(100);
        ESP.restart();
    }

    void logDiagnostics() {
        Serial.println("--- King-H System Diagnostics ---");
        Serial.printf("Reset Reason: %d\n", (int)esp_reset_reason());
        Serial.printf("Heap Free: %u bytes\n", ESP.getFreeHeap());
        Serial.println("---------------------------------");
    }
};

#endif