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

BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;
BLECharacteristic* pCharacteristic_2 = nullptr;
bool deviceConnected = false;

// Forward declarations
void sendEffectMessage(int effect);
void sendLightMessage(float value);
void sendSpeedMessage(float value);
void sendDataBle(String message);

void handleAudioLevel(int level) { ledMgr.runMusicSync(level); }

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
        // Re-synced only existing getters from LedManager
        sendEffectMessage((int)ledMgr.getEffect());
        sendSpeedMessage(ledMgr.getSpeed());
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
            case CMD_LIGHT: ledMgr.setBrightness((uint8_t)(doc["value"].as<float>() * 255.0f)); break;
            case CMD_SPEED: ledMgr.setSpeed(doc["value"].as<float>()); break;
            case CMD_EFFECT: ledMgr.setEffect(parseEffectType(doc["value"] | "")); break;
            default: break;
        }
    }
};

struct DebounceButton {
    uint8_t pin;
    bool lastRawState;
    bool stableState;
    uint32_t lastDebounceTime;
    uint32_t debounceDelay;

    DebounceButton(uint8_t p, uint32_t delayMs = 50) {
        pin = p;
        lastRawState = HIGH;
        stableState = HIGH;
        lastDebounceTime = 0;
        debounceDelay = delayMs;
    }

    bool update() {
        bool reading = digitalRead(pin);
        bool pressed = false;

        if (reading != lastRawState) {
            lastDebounceTime = millis();
        }

        if ((millis() - lastDebounceTime) > debounceDelay) {
            if (reading != stableState) {
                stableState = reading;
                if (stableState == LOW) {
                    pressed = true;
                }
            }
        }

        lastRawState = reading;
        return pressed;
    }
};

void tickEncoder() { encoder.tick(); }

void handleInputs() {
    // Volume Control via Encoder
    static int lastPos = 0;
    int pos = encoder.getPosition();
    if (pos != lastPos) {
        float vol = audioMgr.getVolume();
        vol += (pos > lastPos) ? -0.02f : 0.02f;
        audioMgr.setVolume(vol);
        lastPos = pos;
    }

    // Debounced Buttons
    static DebounceButton dbEncoderBtn(ENCODER_SW, 50);
    static DebounceButton dbBtnMode(BTN_LED_MODE, 50);
    static DebounceButton dbBtnSpeed(BTN_LED_SPEED, 50);
    static DebounceButton dbBtnToggle(BTN_TOGGLE, 50);

    // Encoder Button Logic (Play/Pause & Next)
    static uint32_t lastPressTime = 0;
    static bool waitingForDouble = false;

    if (dbEncoderBtn.update()) {
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

    // Mode & Speed Buttons
    if (dbBtnMode.update()) {
        int nextMode = ((int)ledMgr.getEffect() + 1) % 8;
        ledMgr.setEffect((EffectType)nextMode);
        sendEffectMessage(nextMode);
    }
    if (dbBtnSpeed.update()) {
        float currentSpd = ledMgr.getSpeed();
        currentSpd -= 0.2f; if (currentSpd <= 0) currentSpd = 1.0f;
        ledMgr.setSpeed(currentSpd);
        sendSpeedMessage(currentSpd);
    }

    // Toggle Button Logic (Double Click to switch mode, Single Click to toggle power)
    static uint32_t lastClick = 0;
    static bool waiting2nd = false;

    if (dbBtnToggle.update()) {
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
}

void setup() {
    Serial.begin(115200);
    systemMgr.begin();
    activeMode = systemMgr.getActiveMode();
    pinMode(ENCODER_CLK, INPUT_PULLUP);
    pinMode(ENCODER_DT, INPUT_PULLUP);
    pinMode(ENCODER_SW, INPUT_PULLUP); 
    pinMode(BTN_LED_MODE, INPUT_PULLUP);
    pinMode(BTN_LED_SPEED, INPUT_PULLUP);
    pinMode(BTN_TOGGLE, INPUT_PULLUP);
    encTicker.attach_ms(1, tickEncoder);
    delay(1000);
    audioMgr.setAudioLevelCallback(handleAudioLevel);
    audioMgr.begin(activeMode);
    BLEDevice::init("KingH Distiller");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    BLEService* pService = pServer->createService(SERVICE_UUID);
    pCharacteristic = pService->createCharacteristic(CHAR1_UUID, BLECharacteristic::PROPERTY_NOTIFY);
    pCharacteristic->addDescriptor(new BLE2902());
    pCharacteristic_2 = pService->createCharacteristic(CHAR2_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
    pCharacteristic_2->setCallbacks(new CharacteristicCallBack());
    pService->start();
    BLEDevice::getAdvertising()->start();
    systemMgr.logDiagnostics();

    delay(2000);
    ledMgr.begin();
    delay(1000);
}

void loop() { audioMgr.update(); handleInputs(); }

void sendDataBle(String message) { if (deviceConnected && pCharacteristic) { pCharacteristic->setValue(message.c_str()); pCharacteristic->notify(); } }
void sendLightMessage(float value) { JsonDocument d; d["cmd"] = "LIGHT"; d["value"] = value; String s; serializeJson(d, s); sendDataBle(s); }
void sendSpeedMessage(float value) { JsonDocument d; d["cmd"] = "SPEED"; d["value"] = value; String s; serializeJson(d, s); sendDataBle(s); }
void sendEffectMessage(int effect) { JsonDocument d; d["cmd"] = "EFFECT"; d["value"] = effect; String s; serializeJson(d, s); sendDataBle(s); }