#ifndef LED_MANAGER_H
#define LED_MANAGER_H

#include <Arduino.h>
#include <FastLED.h>
#include <Ticker.h>
#include "../../include/Config.h"

class LedManager {
private:
    CRGB leds[NUM_LEDS];
    Ticker animationTicker;
    
    EffectType currentEffect = EFFECT_RAINBOW;
    float animationSpeed = 0.5f;
    uint8_t gHue = 0;
    CRGB baseColor = CRGB::Blue;
    
    int breatheBrightness = 0;
    int breatheDelta = 2;
    
    static LedManager* instance;

    // Animation Methods
    void solidPulse();
    void rainbowDance();
    void breathingEffect();
    void sparkleEffect();
    void colorCycleEffect();
    void strobeEffect();
    void starryNightEffect();

    static void updateCallback();

public:
    LedManager();
    void begin();
    void setEffect(EffectType effect);
    EffectType getEffect();
    void setSpeed(float speed);
    float getSpeed();
    void setBrightness(uint8_t b);
    void togglePower();
    void updateTicker();
    void runMusicSync(int rawAudioLevel);
    void runCurrentMode();
};

#endif