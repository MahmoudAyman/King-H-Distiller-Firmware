// #include <Arduino.h>
// #include <Ticker.h>
// #include <RotaryEncoder.h>

// #include <BLEDevice.h>
// #include <BLEServer.h>
// #include <BLEUtils.h>
// #include <BLE2902.h>
// #include "ArduinoJson.h"

// #include "Config.h"
// #include "SystemManager.h"
// #include "LedManager.h"
// #include "AudioManager.h"

// // Globals
// Ticker encTicker;
// SystemManager systemMgr;
// LedManager ledMgr;
// AudioManager audioMgr;

// uint8_t activeMode; 
// RotaryEncoder encoder(ENCODER_CLK, ENCODER_DT, RotaryEncoder::LatchMode::TWO03);

// // BLE Globals (To be refactored next)
// BLEServer* pServer = NULL;
// BLECharacteristic* pCharacteristic = NULL;
// BLECharacteristic* pCharacteristic_2 = NULL;
// bool deviceConnected = false;

// // Shared Logic: Link Audio Level to LEDs
// void handleAudioLevel(int level) {
//     ledMgr.runMusicSync(level);
// }

// // Forward Declarations
// void sendDataBle(String message);
// void sendLightMessage(float value);
// void sendSpeedMessage(float value);
// void sendEffectMessage(int effect);

// CommandType parseCommandType(const String& cmd) {
//     if (cmd == "LIGHT") return CMD_LIGHT;
//     if (cmd == "SPEED") return CMD_SPEED;
//     if (cmd == "EFFECT") return CMD_EFFECT;
//     return CMD_UNKNOWN;
// }

// EffectType parseEffectType(const String& value) {
//     if (value == "STATIC") return EFFECT_STATIC;
//     if (value == "RAINBOW") return EFFECT_RAINBOW;
//     if (value == "BREATHING") return EFFECT_BREATHING;
//     if (value == "SPARKLE") return EFFECT_SPARKLE;
//     if (value == "COLOR_CYCLE") return EFFECT_COLOR_CYCLE;
//     if (value == "STROBING") return EFFECT_STROBING;
//     if (value == "STARRY_NIGHT") return EFFECT_STARRY_NIGHT;
//     if (value == "MUSIC") return EFFECT_MUSIC;
//     return EFFECT_INVALID;
// }

// class MyServerCallbacks : public BLEServerCallbacks {
//     void onConnect(BLEServer* pServer) {
//         deviceConnected = true;
//         sendEffectMessage((int)ledMgr.getEffect());
//         sendLightMessage(ledMgr.getSpeed());
//     }
//     void onDisconnect(BLEServer* pServer) override {
//         deviceConnected = false;
//         pServer->startAdvertising();
//     }
// };

// class CharacteristicCallBack : public BLECharacteristicCallbacks {
//     void onWrite(BLECharacteristic* pChar) override {
//         JsonDocument doc;
//         deserializeJson(doc, pChar->getValue().c_str());
//         String cmdStr = doc["cmd"] | "";
//         CommandType cmd = parseCommandType(cmdStr);
//         switch (cmd) {
//             case CMD_LIGHT: ledMgr.setBrightness((float)doc["value"] * 100); break;
//             case CMD_SPEED: ledMgr.setSpeed((float)doc["value"]); break;
//             case CMD_EFFECT: ledMgr.setEffect(parseEffectType(doc["value"] | "")); break;
//             default: break;
//         }
//     }
// };

// void tickEncoder() { encoder.tick(); }

// void handleEncoderRotation() {
//     static int lastPos = encoder.getPosition();
//     int pos = encoder.getPosition();
//     if (pos != lastPos) {
//         int diff = pos - lastPos;
//         float currentVol = audioMgr.getVolume();
//         audioMgr.setVolume(currentVol - (diff / 100.0f));
//         lastPos = pos;
//     }
// }

// void handleEncoderButton() {
//     static bool lastEncBtn = HIGH;
//     static uint32_t lastPressTime = 0;
//     static bool waitingForDouble = false;
    
//     bool reading = digitalRead(ENCODER_SW);
//     if (reading == LOW && lastEncBtn == HIGH) {
//         if (DEBUG_INPUT) Serial.println("Main: Encoder SW pressed");
//         if (waitingForDouble && (millis() - lastPressTime < DBL_DELAY)) {
//             waitingForDouble = false;
//             audioMgr.next();
//         } else {
//             waitingForDouble = true;
//             lastPressTime = millis();
//         }
//     }
//     if (waitingForDouble && (millis() - lastPressTime > DBL_DELAY)) {
//         waitingForDouble = false;
//         audioMgr.togglePause();
//     }
//     lastEncBtn = reading;
// }

// void handleExtraButtons() {
//     static bool lastBtnMode = HIGH;
//     static bool lastBtnSpeed = HIGH;
    
//     bool btnMode = digitalRead(BTN_LED_MODE);
//     bool btnSpeed = digitalRead(BTN_LED_SPEED);

//     if (btnMode == LOW && lastBtnMode == HIGH) {
//         int nextMode = ((int)ledMgr.getEffect() + 1) % 8;
//         ledMgr.setEffect((EffectType)nextMode);
//         sendEffectMessage(nextMode);
//     }
//     if (btnSpeed == LOW && lastBtnSpeed == HIGH) {
//         float currentSpd = ledMgr.getSpeed();
//         currentSpd -= 0.3f; if (currentSpd <= 0) currentSpd = 1.0f;
//         ledMgr.setSpeed(currentSpd);
//         sendSpeedMessage(currentSpd);
//     }
//     lastBtnMode = btnMode;
//     lastBtnSpeed = btnSpeed;
// }

// void handleToggleBtn() {
//     static bool lastBtnOn = HIGH;
//     static uint32_t lastClick = 0;
//     static bool waiting2nd = false;
    
//     bool reading = digitalRead(BTN_TOGGLE);
//     if (reading == LOW && lastBtnOn == HIGH) {
//         if (waiting2nd && (millis() - lastClick < DC_DELAY)) {
//             waiting2nd = false;
//             systemMgr.storeModeAndReboot((activeMode == MODE_SD) ? MODE_BT : MODE_SD);
//         } else {
//             waiting2nd = true;
//             lastClick = millis();
//         }
//     }
//     if (waiting2nd && (millis() - lastClick > DC_DELAY)) {
//         waiting2nd = false;
//         ledMgr.togglePower();
//     }
//     lastBtnOn = reading;
// }

// void setup() {
//     Serial.begin(115200);
//     delay(1000);
//     Serial.println("\n[Main] Phase 3 Integration Complete.");

//     systemMgr.begin();
//     activeMode = systemMgr.getActiveMode();

//     pinMode(ENCODER_CLK, INPUT_PULLUP);
//     pinMode(ENCODER_DT, INPUT_PULLUP);
//     pinMode(ENCODER_SW, INPUT_PULLUP); 
//     pinMode(BTN_LED_MODE, INPUT_PULLUP);
//     pinMode(BTN_LED_SPEED, INPUT_PULLUP);
//     pinMode(BTN_TOGGLE, INPUT_PULLUP);

//     encTicker.attach_ms(1, tickEncoder);
    
//     ledMgr.begin();
//     audioMgr.setAudioLevelCallback(handleAudioLevel);
//     audioMgr.begin(activeMode);

//     // Initial BLE Setup (Refactor in Phase 4)
//     BLEDevice::init("KingH Music Player");
//     pServer = BLEDevice::createServer();
//     pServer->setCallbacks(new MyServerCallbacks());
//     BLEService* pService = pServer->createService(SERVICE_UUID);
//     pCharacteristic = pService->createCharacteristic(CHAR1_UUID, BLECharacteristic::PROPERTY_NOTIFY);
//     pCharacteristic_2 = pService->createCharacteristic(CHAR2_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
//     pCharacteristic->addDescriptor(new BLE2902());
//     pCharacteristic_2->setCallbacks(new CharacteristicCallBack());
//     pService->start();
//     BLEDevice::getAdvertising()->start();

//     Serial.println("[Main] System Initialized.");
// }

// void loop() {
//     audioMgr.update();
//     handleToggleBtn();
//     handleEncoderRotation();
//     handleEncoderButton();
//     handleExtraButtons();
// }

// // BLE Stubs
// void sendDataBle(String message) { if (deviceConnected) { pCharacteristic->setValue(message.c_str()); pCharacteristic->notify(); } }
// void sendLightMessage(float v) { JsonDocument d; d["cmd"]="LIGHT"; d["value"]=v; String s; serializeJson(d,s); sendDataBle(s); }
// void sendSpeedMessage(float v) { JsonDocument d; d["cmd"]="SPEED"; d["value"]=v; String s; serializeJson(d,s); sendDataBle(s); }
// void sendEffectMessage(int e) { JsonDocument d; d["cmd"]="EFFECT"; d["value"]=e; String s; serializeJson(d,s); sendDataBle(s); }


#include <Arduino.h>
#include <Ticker.h>
#include <RotaryEncoder.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "ArduinoJson.h"

#include "Config.h"
#include "SystemManager.h"
#include "LedManager.h"
#include "AudioManager.h"

Ticker encTicker;
SystemManager systemMgr;
LedManager ledMgr;
AudioManager audioMgr;

uint8_t activeMode; 
RotaryEncoder encoder(ENCODER_CLK, ENCODER_DT, RotaryEncoder::LatchMode::TWO03);

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
BLECharacteristic* pCharacteristic_2 = NULL;
bool deviceConnected = false;

// Forward declarations
void sendEffectMessage(int effect);
void sendLightMessage(float value);
void sendSpeedMessage(float value);
void sendDataBle(String message);

void handleAudioLevel(int level) {
    ledMgr.runMusicSync(level);
}

CommandType parseCommandType(const String& cmd) {
    if (cmd == "LIGHT") return CMD_LIGHT;
    if (cmd == "SPEED") return CMD_SPEED;
    if (cmd == "EFFECT") return CMD_EFFECT;
    return CMD_UNKNOWN;
}

EffectType parseEffectType(const String& value) {
    if (value == "STATIC") return EFFECT_STATIC;
    if (value == "RAINBOW") return EFFECT_RAINBOW;
    if (value == "BREATHING") return EFFECT_BREATHING;
    if (value == "SPARKLE") return EFFECT_SPARKLE;
    if (value == "COLOR_CYCLE") return EFFECT_COLOR_CYCLE;
    if (value == "STROBING") return EFFECT_STROBING;
    if (value == "STARRY_NIGHT") return EFFECT_STARRY_NIGHT;
    if (value == "MUSIC") return EFFECT_MUSIC;
    return EFFECT_INVALID;
}

class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        sendEffectMessage((int)ledMgr.getEffect());
    }
    void onDisconnect(BLEServer* pServer) override {
        deviceConnected = false;
        pServer->startAdvertising();
    }
};

class CharacteristicCallBack : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pChar) override {
        JsonDocument doc;
        deserializeJson(doc, pChar->getValue().c_str());
        String cmdStr = doc["cmd"] | "";
        CommandType cmd = parseCommandType(cmdStr);
        switch (cmd) {
            case CMD_LIGHT: ledMgr.setBrightness((float)doc["value"] * 100); break;
            case CMD_SPEED: ledMgr.setSpeed((float)doc["value"]); break;
            case CMD_EFFECT: ledMgr.setEffect(parseEffectType(doc["value"] | "")); break;
            default: break;
        }
    }
};

void tickEncoder() { encoder.tick(); }

void handleEncoderRotation() {
    static int lastPos = encoder.getPosition();
    int pos = encoder.getPosition();
    if (pos != lastPos) {
        int diff = pos - lastPos;
        float currentVol = audioMgr.getVolume();
        audioMgr.setVolume(currentVol - (diff / 100.0f));
        lastPos = pos;
    }
}

void handleEncoderButton() {
    static bool lastEncBtn = HIGH;
    static uint32_t lastPressTime = 0;
    static bool waitingForDouble = false;
    
    bool reading = digitalRead(ENCODER_SW);
    if (reading == LOW && lastEncBtn == HIGH) {
        if (waitingForDouble && (millis() - lastPressTime < DBL_DELAY)) {
            waitingForDouble = false;
            audioMgr.next();
        } else {
            waitingForDouble = true;
            lastPressTime = millis();
        }
    }
    if (waitingForDouble && (millis() - lastPressTime > DBL_DELAY)) {
        waitingForDouble = false;
        audioMgr.togglePause();
    }
    lastEncBtn = reading;
}

void handleExtraButtons() {
    static bool lastBtnMode = HIGH;
    static bool lastBtnSpeed = HIGH;
    
    bool btnMode = digitalRead(BTN_LED_MODE);
    bool btnSpeed = digitalRead(BTN_LED_SPEED);

    if (btnMode == LOW && lastBtnMode == HIGH) {
        int nextMode = ((int)ledMgr.getEffect() + 1) % 8;
        ledMgr.setEffect((EffectType)nextMode);
    }
    if (btnSpeed == LOW && lastBtnSpeed == HIGH) {
        float currentSpd = ledMgr.getSpeed();
        currentSpd -= 0.3f; if (currentSpd <= 0) currentSpd = 1.0f;
        ledMgr.setSpeed(currentSpd);
    }
    lastBtnMode = btnMode;
    lastBtnSpeed = btnSpeed;
}

void handleToggleBtn() {
    static bool lastBtnOn = HIGH;
    static uint32_t lastClick = 0;
    static bool waiting2nd = false;
    
    bool reading = digitalRead(BTN_TOGGLE);
    if (reading == LOW && lastBtnOn == HIGH) {
        if (waiting2nd && (millis() - lastClick < DC_DELAY)) {
            waiting2nd = false;
            systemMgr.storeModeAndReboot((activeMode == MODE_SD) ? MODE_BT : MODE_SD);
        } else {
            waiting2nd = true;
            lastClick = millis();
        }
    }
    if (waiting2nd && (millis() - lastClick > DC_DELAY)) {
        waiting2nd = false;
        ledMgr.togglePower();
    }
    lastBtnOn = reading;
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    systemMgr.begin();
    activeMode = systemMgr.getActiveMode();

    pinMode(ENCODER_CLK, INPUT_PULLUP);
    pinMode(ENCODER_DT, INPUT_PULLUP);
    pinMode(ENCODER_SW, INPUT_PULLUP); 
    pinMode(BTN_LED_MODE, INPUT_PULLUP);
    pinMode(BTN_LED_SPEED, INPUT_PULLUP);
    pinMode(BTN_TOGGLE, INPUT_PULLUP);

    encTicker.attach_ms(1, tickEncoder);
    
    ledMgr.begin();
    audioMgr.setAudioLevelCallback(handleAudioLevel);
    audioMgr.begin(activeMode);

    BLEDevice::init("KingH Music Player");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    BLEService* pService = pServer->createService(SERVICE_UUID);
    pCharacteristic = pService->createCharacteristic(CHAR1_UUID, BLECharacteristic::PROPERTY_NOTIFY);
    pCharacteristic_2 = pService->createCharacteristic(CHAR2_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
    pCharacteristic->addDescriptor(new BLE2902());
    pCharacteristic_2->setCallbacks(new CharacteristicCallBack());
    pService->start();
    BLEDevice::getAdvertising()->start();

    Serial.println("[Main] Ready.");
}

void loop() {
    audioMgr.update();
    handleToggleBtn();
    handleEncoderRotation();
    handleEncoderButton();
    handleExtraButtons();
}

void sendDataBle(String message) { if (deviceConnected) { pCharacteristic->setValue(message.c_str()); pCharacteristic->notify(); } }
void sendLightMessage(float v) { JsonDocument d; d["cmd"]="LIGHT"; d["value"]=v; String s; serializeJson(d,s); sendDataBle(s); }
void sendSpeedMessage(float v) { JsonDocument d; d["cmd"]="SPEED"; d["value"]=v; String s; serializeJson(d,s); sendDataBle(s); }
void sendEffectMessage(int e) { JsonDocument d; d["cmd"]="EFFECT"; d["value"]=e; String s; serializeJson(d,s); sendDataBle(s); }