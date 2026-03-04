#include <FastLED.h>

/**
 * WS2812B 3x3 Module Matrix Test (144 LEDs)
 * -----------------------------------------
 * Pin 6 -> DIN
 */

// Configuration
#define LED_PIN     6          
#define NUM_LEDS    144        
#define BRIGHTNESS  100        
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB

#define MAX_BRIGHTNESS 20      // Set very low
#define VOLTS          5       
#define MAX_AMPS       400     // Limit to 400mA 


CRGB leds[NUM_LEDS];

// Global state variables
uint8_t gHue = 0;
CRGB baseColor = CRGB::Blue;
int currentMode = 1;          
float animationSpeed = 0.5f;  
int breatheBrightness = 0;
int breatheDelta = 1;
unsigned long lastLedTime = 0;

// ============================================================
//                  VISUAL EFFECTS (FROM ORIGINAL FIRMWARE)
// ============================================================

void solidPulse(CRGB c = CRGB::Blue) {
  fill_solid(leds, NUM_LEDS, c);
  FastLED.show();
}

void rainbowDance() {
  fill_rainbow(leds, NUM_LEDS, gHue, 10);
  gHue += 5;
  FastLED.show();
}

void breathingEffect(CRGB c) {
  breatheBrightness += breatheDelta;
  if (breatheBrightness <= 0 || breatheBrightness >= 255) {
    breatheDelta = -breatheDelta;
  }
  CRGB cur = c;
  cur.nscale8_video(breatheBrightness);
  fill_solid(leds, NUM_LEDS, cur);
  FastLED.show();
}

void sparkleEffect(CRGB sparkle = CRGB::White, uint8_t chance = 60, uint8_t fadeAmt = 20) {
  if (random8() < chance) leds[random16(NUM_LEDS)] = sparkle;
  fadeToBlackBy(leds, NUM_LEDS, fadeAmt);
  FastLED.show();
}

void colorCycleEffect(uint8_t step = 1) {
  static uint8_t hue = 0;
  fill_solid(leds, NUM_LEDS, CHSV(hue, 255, 255));
  hue += step;
  FastLED.show();
}

void strobeEffect(CRGB color = CRGB::White) {
  static bool on = false;
  on = !on;
  fill_solid(leds, NUM_LEDS, on ? color : CRGB::Black);
  FastLED.show();
}

void strayNightEffect(uint8_t chance = 60, uint8_t fadeAmt = 20) {
  if (random8() < chance) leds[random(NUM_LEDS)] = CHSV(random8(), 200, 255);
  fadeToBlackBy(leds, NUM_LEDS, fadeAmt);
  FastLED.show();
}

void runCurrentMode() {
  switch (currentMode) {
    case 0: solidPulse(CRGB::Blue); break;
    case 1: rainbowDance(); break;
    case 2: breathingEffect(baseColor); break;
    case 3: sparkleEffect(); break;
    case 4: colorCycleEffect(); break;
    case 5: strobeEffect(CRGB::Yellow); break;
    case 6: strayNightEffect(); break;
    default: solidPulse(CRGB::Red); break;
  }
}

// ============================================================
//                  MAIN SETUP & LOOP
// ============================================================

void setup() {
  Serial.begin(9600);
  
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setMaxPowerInVoltsAndMilliamps(VOLTS, MAX_AMPS);
  FastLED.setBrightness(MAX_BRIGHTNESS);
  FastLED.clear();
  FastLED.show();
  
  Serial.println("--- LED Matrix Test Initialized ---");
}

void loop() {
  static unsigned long lastModeSwitch = 0;
  if (millis() - lastModeSwitch > 10000) {
    currentMode = (currentMode + 1) % 7; 
    lastModeSwitch = millis();
    Serial.print("Current Mode: ");
    Serial.println(currentMode);
  }

  unsigned long currentTime = millis();
  unsigned int minTimeDif = ((1.0f - animationSpeed) + 0.1f) * 60;
  
  if ((currentTime - lastLedTime) > minTimeDif) {
    lastLedTime = currentTime;
    runCurrentMode();
  }
}