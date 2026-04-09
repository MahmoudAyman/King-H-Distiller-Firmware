#include "LedManager.h"

LedManager* LedManager::instance = nullptr;

LedManager::LedManager() {
    instance = this;
}

void LedManager::begin() {
    Serial.println("LedManager: Initializing FastLED (144 LEDs)...");
    FastLED.addLeds<WS2812B, LED_PIN, RGB>(leds, NUM_LEDS);
    
    FastLED.setBrightness(25); 
    Serial.println("LedManager: Safety Brightness set to 10%");
    
    FastLED.clear(true);
    FastLED.show();
    updateTicker();
}

void LedManager::setEffect(EffectType effect) {
    Serial.printf("LedManager: Mode Change: %d\n", (int)effect);
    currentEffect = effect;
    updateTicker();
}

EffectType LedManager::getEffect() { 
    return currentEffect; 
}

void LedManager::setSpeed(float speed) {
    animationSpeed = speed;
    updateTicker();
}

float LedManager::getSpeed() { 
    return animationSpeed; 
}

void LedManager::setBrightness(uint8_t b) {
    // b is expected 0-100 from BLE, map it or use directly if 0-255
    FastLED.setBrightness(b);
    FastLED.show();
}

void LedManager::togglePower() {
    bool isOn = FastLED.getBrightness() > 0;
    // Toggle between 0 and 10% (25)
    setBrightness(isOn ? 0 : 25);
    Serial.printf("LedManager: Power: %s (at 10%%)\n", isOn ? "OFF" : "ON");
}

void LedManager::updateTicker() {
    animationTicker.detach();
    float intervalMs = ((1.0f - animationSpeed) + 0.1f) * 25.0f;
    animationTicker.attach_ms(intervalMs, LedManager::updateCallback);
}

void LedManager::updateCallback() {
    if (instance) instance->runCurrentMode();
}

void LedManager::runMusicSync(int rawAudioLevel) {
    if (currentEffect != EFFECT_MUSIC) return;
    int ledLevel = constrain(map(rawAudioLevel, 0, 3000, 0, NUM_LEDS), 0, NUM_LEDS);
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    for (int i = 0; i < ledLevel; ++i) leds[i] = CHSV(gHue + i * 10, 255, 255);
    ++gHue; 
    FastLED.show();
}

void LedManager::runCurrentMode() {
    switch (currentEffect) {
        case EFFECT_STATIC:      solidPulse(); break;
        case EFFECT_RAINBOW:     rainbowDance(); break;
        case EFFECT_BREATHING:   breathingEffect(); break;
        case EFFECT_SPARKLE:     sparkleEffect(); break;
        case EFFECT_COLOR_CYCLE: colorCycleEffect(); break;
        case EFFECT_STROBING:    strobeEffect(); break;
        case EFFECT_STARRY_NIGHT: starryNightEffect(); break;
        case EFFECT_MUSIC:       break; 
        default: break;
    }
}

void LedManager::solidPulse() { 
    fill_solid(leds, NUM_LEDS, baseColor); 
    FastLED.show(); 
}

void LedManager::rainbowDance() { 
    fill_rainbow(leds, NUM_LEDS, gHue, 10); 
    gHue += 5; 
    FastLED.show(); 
}

void LedManager::breathingEffect() {
    breatheBrightness += breatheDelta;
    if (breatheBrightness <= 0 || breatheBrightness >= 255) breatheDelta = -breatheDelta;
    CRGB cur = baseColor; 
    cur.nscale8_video(breatheBrightness);
    fill_solid(leds, NUM_LEDS, cur); 
    FastLED.show();
}

void LedManager::sparkleEffect() { 
    if (random8() < 60) leds[random16(NUM_LEDS)] = CRGB::White; 
    fadeToBlackBy(leds, NUM_LEDS, 20); 
    FastLED.show(); 
}

void LedManager::colorCycleEffect() { 
    static uint8_t hue = 0; 
    fill_solid(leds, NUM_LEDS, CHSV(hue++, 255, 255)); 
    FastLED.show(); 
}

void LedManager::strobeEffect() {
    static bool on = false; 
    static uint32_t last = 0;
    uint16_t dur = (1.0f - animationSpeed) * 900 + 50;
    if (millis() - last >= dur) { on = !on; last = millis(); }
    fill_solid(leds, NUM_LEDS, on ? CRGB::Yellow : CRGB::Black); 
    FastLED.show();
}

void LedManager::starryNightEffect() { 
    if (random8() < 60) leds[random(NUM_LEDS)] = CHSV(random8(), 200, 255); 
    fadeToBlackBy(leds, NUM_LEDS, 20); 
    FastLED.show(); 
}