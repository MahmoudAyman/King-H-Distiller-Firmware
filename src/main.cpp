#include <Arduino.h>
#include <Ticker.h>
#include <FastLED.h>
#include <RotaryEncoder.h>
#include <SD_MMC.h>
#include <BluetoothA2DPSink.h>

#include "AudioTools.h"
#include "AudioTools/Disk/AudioSourceSDMMC.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "ArduinoJson.h"
#include <vector>

#include "Config.h"
#include "SystemManager.h"

std::vector<String> mp3Files;
Ticker encTicker;
Ticker animationTicker;

SystemManager systemMgr;
uint8_t activeMode; 

I2SStream i2s;  
RotaryEncoder encoder(ENCODER_CLK, ENCODER_DT, RotaryEncoder::LatchMode::TWO03);

CRGB leds[NUM_LEDS];
int brightness = 150;
uint8_t gHue = 0;             
CRGB baseColor = CRGB::Blue;  
float smoothedVolume = 0;
int currentSongIndex = 1;
float animationSpeed = 0.5f;

BluetoothA2DPSink a2dp_sink(i2s);
volatile int btLevel = 0;  
bool btPlaying = true;     
bool plused = false;

MP3DecoderHelix mp3dec;
AudioSourceSDMMC sdSource("/", "mp3");
AudioPlayer sdPlayer(sdSource, i2s, mp3dec);
bool sdOK = false;
bool sdPaused = false;
float GVolume = 0.2;

uint32_t lastClick = 0;
bool waiting2nd = false;
bool waitingForDouble = false;
unsigned long lastPressTime = 0;

int currentMode = 1;
int breatheBrightness = 0;
int breatheDelta = 2;

bool lastEncBtn = HIGH;
bool lastBtnMode = HIGH;
bool lastBtnSpeed = HIGH;
bool lastBtnOn = HIGH;

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
BLECharacteristic* pCharacteristic_2 = NULL;
bool deviceConnected = false;

// Forward Declarations
void updateAnimationSpeed(float newSpeed);
void runCurrentMode();
void sendDataBle(String message);
void sendLightMessage(float value);
void sendSpeedMessage(float value);
void sendEffectMessage(int effect);
void initBTmode();
void initSDmode();

size_t walkDir(const char* folder, std::vector<String>& list, bool recurse = true) {
  if (!USE_SD_CARD) return 0;
  File dir = SD_MMC.open(folder);
  if (!dir) return 0;
  size_t found = 0;
  File f = dir.openNextFile();
  while (f) {
    if (f.isDirectory()) {
      if (recurse) found += walkDir(f.path(), list, true);
    } else {
      String name = f.name();
      name.toLowerCase();
      if (name.endsWith(".mp3")) {
        list.push_back(String(f.path()));
        ++found;
      }
    }
    f = dir.openNextFile();
  }
  return found;
}

size_t buildMp3List(std::vector<String>& outList, const char* startFolder = "/", bool recurse = true) {
  outList.clear();
  return walkDir(startFolder, outList, recurse);
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

class RMSStream : public AudioStream {
public:
  RMSStream(I2SStream& i2s) : i2s(i2s) {}
  size_t write(const uint8_t* buffer, size_t size) override {
    const int16_t* s = (int16_t*)buffer;
    int32_t acc = 0;
    for (uint32_t i = 0; i < size / 2; i++) acc += abs(s[i]);
    smoothedVolume = acc / ((size / 2) * sqrt(GVolume + 0.01));
    return i2s.write(buffer, size);
  }
private:
  I2SStream& i2s;
};
RMSStream rmsStream(i2s);

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    sendEffectMessage(currentMode);
    sendLightMessage(animationSpeed);
  };
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
      case CMD_LIGHT:
        FastLED.setBrightness((float)doc["value"] * 100);
        break;
      case CMD_SPEED:
        updateAnimationSpeed((float)doc["value"]);
        break;
      case CMD_EFFECT:
        currentMode = parseEffectType(doc["value"] | "");
        break;
      default: break;
    }
  }
};

void tickEncoder() { encoder.tick(); }

void solidPulse(CRGB c = CRGB::Blue) { fill_solid(leds, NUM_LEDS, c); FastLED.show(); }
void rainbowDance() { fill_rainbow(leds, NUM_LEDS, gHue, 10); gHue += 5; FastLED.show(); }
void breathingEffect(CRGB c, uint8_t speed = 10) {
  breatheBrightness += breatheDelta;
  if (breatheBrightness <= 0 || breatheBrightness >= 255) breatheDelta = -breatheDelta;
  CRGB cur = c; cur.nscale8_video(breatheBrightness);
  fill_solid(leds, NUM_LEDS, cur); FastLED.show();
}
void sparkleEffect() { if (random8() < 60) leds[random16(NUM_LEDS)] = CRGB::White; fadeToBlackBy(leds, NUM_LEDS, 20); FastLED.show(); }
void colorCycleEffect() { static uint8_t hue = 0; fill_solid(leds, NUM_LEDS, CHSV(hue++, 255, 255)); FastLED.show(); }
void strobeEffect(CRGB color) {
  static bool on = false; static uint32_t last = 0;
  uint16_t dur = (1.0f - animationSpeed) * 900 + 50;
  if (millis() - last >= dur) { on = !on; last = millis(); }
  fill_solid(leds, NUM_LEDS, on ? color : CRGB::Black); FastLED.show();
}
void strayNightEffect() { if (random8() < 60) leds[random(NUM_LEDS)] = CHSV(random8(), 200, 255); fadeToBlackBy(leds, NUM_LEDS, 20); FastLED.show(); }
void musicSync() {
  int raw = (activeMode == MODE_SD) ? smoothedVolume : btLevel;
  int ledLevel = constrain(map(raw, 0, 3000, 0, NUM_LEDS), 0, NUM_LEDS);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  for (int i = 0; i < ledLevel; ++i) leds[i] = CHSV(gHue + i * 10, 255, 255);
  ++gHue; FastLED.show();
}

void runCurrentMode() {
  switch (currentMode) {
    case 0: solidPulse(CRGB::Blue); break;
    case 1: rainbowDance(); break;
    case 2: breathingEffect(baseColor, 5); break;
    case 3: sparkleEffect(); break;
    case 4: colorCycleEffect(); break;
    case 5: strobeEffect(CRGB::Yellow); break;
    case 6: strayNightEffect(); break;
    case 7: musicSync(); break;
  }
}

void handleEncoderRotation() {
  static int lastPos = encoder.getPosition();
  int pos = encoder.getPosition();
  if (pos != lastPos) {
    int diff = pos - lastPos;
    if (activeMode == MODE_SD) {
      GVolume = constrain(GVolume - (diff / 100.0f), 0, 1.0f);
      sdPlayer.setVolume(GVolume);
    } else {
      int vol = constrain(a2dp_sink.get_volume() - diff, 0, 100);
      a2dp_sink.set_volume(vol);
    }
    lastPos = pos;
  }
}

void handleEncoderButton() {
  bool reading = digitalRead(ENCODER_SW);
  if (reading == LOW && lastEncBtn == HIGH) {
    if (DEBUG_INPUT) Serial.println("[Input] Encoder Button Pressed");
    if (waitingForDouble && (millis() - lastPressTime < DBL_DELAY)) {
      waitingForDouble = false;
      if (activeMode == MODE_SD && USE_SD_CARD) {
         if (currentSongIndex < buildMp3List(mp3Files)) { sdPlayer.next(); currentSongIndex++; }
         else { currentSongIndex = 1; sdPlayer.setIndex(0); sdPlayer.play(); }
      } else a2dp_sink.next();
    } else { waitingForDouble = true; lastPressTime = millis(); }
  }
  if (waitingForDouble && (millis() - lastPressTime > DBL_DELAY)) {
    waitingForDouble = false;
    if (activeMode == MODE_SD) sdPaused = !sdPaused;
    else { if (btPlaying) a2dp_sink.pause(); else a2dp_sink.play(); btPlaying = !btPlaying; }
  }
  lastEncBtn = reading;
}

void handleExtraButtons() {
  bool btnMode = digitalRead(BTN_LED_MODE);
  bool btnSpeed = digitalRead(BTN_LED_SPEED);
  bool btnOn = digitalRead(BTN_TOGGLE);

  if (btnMode == LOW && lastBtnMode == HIGH) {
    if (DEBUG_INPUT) Serial.println("[Input] Mode Button Pressed");
    currentMode = (currentMode + 1) % 8;
    sendEffectMessage(currentMode);
  }
  if (btnSpeed == LOW && lastBtnSpeed == HIGH) {
    if (DEBUG_INPUT) Serial.println("[Input] Speed Button Pressed");
    animationSpeed -= 0.3; if (animationSpeed <= 0) animationSpeed = 1;
    updateAnimationSpeed(animationSpeed);
    sendSpeedMessage(animationSpeed);
  }
  
  lastBtnMode = btnMode;
  lastBtnSpeed = btnSpeed;
  lastBtnOn = btnOn;
}

void volTap(const uint8_t* d, uint32_t len) {
  const int16_t* s = (int16_t*)d;
  int32_t acc = 0;
  for (uint32_t i = 0; i < len / 2; i++) acc += abs(s[i]);
  btLevel = acc / ((len / 2) * sqrt(GVolume + 0.01));
}

void commonHardware() {
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP); 
  pinMode(BTN_LED_MODE, INPUT_PULLUP);
  pinMode(BTN_LED_SPEED, INPUT_PULLUP);
  pinMode(BTN_TOGGLE, INPUT_PULLUP);

  FastLED.addLeds<WS2812B, LED_PIN, RGB>(leds, NUM_LEDS);
  FastLED.setBrightness(brightness);
  FastLED.clear(true); FastLED.show();

  auto cfg = i2s.defaultConfig(TX_MODE);
  cfg.pin_bck = I2S_BCLK; cfg.pin_ws = I2S_LRCK; cfg.pin_data = I2S_DOUT;
  i2s.begin(cfg);
}

void initSDmode() {
  Serial.println("[Main] Initializing SD_MMC...");
  if (USE_SD_CARD && SD_MMC.begin()) {
    uint8_t cardType = SD_MMC.cardType();
    if(cardType == CARD_NONE){
        Serial.println("[Main] Error: No SD card attached");
        activeMode = MODE_BT;
        initBTmode();
        return;
    }
    Serial.printf("[Main] SD Card Type: %d\n", cardType);
    uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
    Serial.printf("[Main] SD Card Size: %lluMB\n", cardSize);

    sdPlayer.setOutput(rmsStream);
    sdPlayer.setVolume(GVolume);
    sdOK = sdPlayer.begin();
    if (sdOK) { 
        size_t count = buildMp3List(mp3Files);
        Serial.printf("[Main] SD Player initialized. Found %d MP3 files.\n", count);
        sdPlayer.setIndex(0); 
        sdPlayer.play(); 
    } else {
        Serial.println("[Main] Failed to start SD Player logic.");
    }
  } else {
    Serial.println("[Main] SD_MMC mount failed. Fallback to BT.");
    activeMode = MODE_BT;
    initBTmode();
  }
}

void initBTmode() {
  Serial.println("[Main] Initializing Bluetooth A2DP...");
  a2dp_sink.set_auto_reconnect(true);
  a2dp_sink.set_stream_reader(volTap, true);
  a2dp_sink.start("KingH Music Player", true);
  a2dp_sink.set_volume(GVolume * 100);

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
}

void handleToggleBtn() {
  bool reading = digitalRead(BTN_TOGGLE);
  
  if (reading == LOW && lastBtnOn == HIGH) {
    if (DEBUG_INPUT) Serial.println("[Input] Toggle Button (Pin 21) Clicked");
    
    if (waiting2nd && (millis() - lastClick < DC_DELAY)) {
      waiting2nd = false;
      Serial.println("[Input] Double Click Detected - Switching Mode & Rebooting...");
      systemMgr.storeModeAndReboot((activeMode == MODE_SD) ? MODE_BT : MODE_SD);
    } else {
      waiting2nd = true;
      lastClick = millis();
    }
  }

  if (waiting2nd && (millis() - lastClick > DC_DELAY)) {
    waiting2nd = false;
    bool newState = FastLED.getBrightness() == 0;
    FastLED.setBrightness(newState ? brightness : 0);
    FastLED.show();
    if (DEBUG_INPUT) Serial.printf("[Input] Single Click - LED Brightness: %d\n", FastLED.getBrightness());
  }
  
  lastBtnOn = reading;
}

void sendDataBle(String message) { if (deviceConnected) { pCharacteristic->setValue(message.c_str()); pCharacteristic->notify(); } }

void sendLightMessage(float v) { 
  JsonDocument d; 
  d["cmd"] = "LIGHT"; 
  d["value"] = v; 
  String s; 
  serializeJson(d, s); 
  sendDataBle(s); 
}

void sendSpeedMessage(float v) { 
  JsonDocument d; 
  d["cmd"] = "SPEED"; 
  d["value"] = v; 
  String s; 
  serializeJson(d, s); 
  sendDataBle(s); 
}

void sendEffectMessage(int e) { 
  JsonDocument d; 
  d["cmd"] = "EFFECT"; 
  d["value"] = e; 
  String s; 
  serializeJson(d, s); 
  sendDataBle(s); 
}

void updateAnimationSpeed(float newSpeed) {
  animationSpeed = newSpeed;
  float intervalMs = ((1.0f - animationSpeed) + 0.1f) * 25.0f;
  animationTicker.detach();
  animationTicker.attach_ms(intervalMs, runCurrentMode);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n[Main] TEST Starting...");

  systemMgr.begin();
  activeMode = systemMgr.getActiveMode();

  encTicker.attach_ms(1, tickEncoder);
  commonHardware();

  if (activeMode == MODE_BT) initBTmode();
  else initSDmode();

  updateAnimationSpeed(0.5f);
  Serial.println("[Main] System Initialized.");
}

void loop() {
  handleToggleBtn();
  handleEncoderRotation();
  handleEncoderButton();
  handleExtraButtons();

  if (USE_SD_CARD && activeMode == MODE_SD && sdOK && !sdPaused) {
    bool playing = sdPlayer.copy();
    if (!playing && !plused) {
        if (++currentSongIndex >= buildMp3List(mp3Files)) { currentSongIndex = 1; sdPlayer.setIndex(0); }
        sdPlayer.play(); plused = true;
    } else if (playing) plused = false;
  }
}