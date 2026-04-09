/*********************************************************************
 *  ESP32  “Super Music Player”  —  SD  +  Bluetooth (toggle)
 *  ---------------------------------------------------------------
 *  • SD-card MP3 player using AudioTools  (default on first boot)
 *  • Bluetooth A2DP sink  (FastLED visualiser etc.)
 *  • BTN 22  double-click  ->  store new mode in NVS, ESP.restart()
 *
 *  Libraries you already use:
 *    - pschatzmann/arduino-audio-tools   (≥2.3)
 *    - pschatzmann/ESP32-A2DP           (≥1.7)
 *    - FastLED, RotaryEncoder
 *********************************************************************/
#include "esp_system.h"
#include <Ticker.h>
#include <Arduino.h>
#include <Preferences.h>
#include "AudioTools.h"
#include "AudioTools/Disk/AudioSourceSDMMC.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include <BluetoothA2DPSink.h>
#include <FastLED.h>
#include <RotaryEncoder.h>
#include <SD_MMC.h>
#include "esp_system.h"  // esp_reset_reason()

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "ArduinoJson.h"
#include <vector>

/////////////////////////////////////////////////////////////////
//  Collect all MP3 file paths into a vector<String>
/////////////////////////////////////////////////////////////////
enum CommandType {
  CMD_LIGHT,
  CMD_SPEED,
  CMD_EFFECT,
  CMD_UNKNOWN
};

enum EffectType {
  EFFECT_STATIC,
  EFFECT_RAINBOW,
  EFFECT_BREATHING,
  EFFECT_SPARKLE,
  EFFECT_COLOR_CYCLE,
  EFFECT_STROBING,
  EFFECT_STARRY_NIGHT,
  EFFECT_MUSIC,
  EFFECT_INVALID
};

// =====================   PIN MAP  =====================
#define ENCODER_CLK 32
#define ENCODER_DT 33
#define ENCODER_SW 35
#define BTN_LED_MODE 5
#define BTN_LED_SPEED 22
#define BTN_TOGGLE 21  // double-click => switch mode + reboot
#define LED_PIN 17
#define NUM_LEDS 144

// I²S pins
#define I2S_BCLK 19
#define I2S_LRCK 25
#define I2S_DOUT 26

// =====================  MODE STORAGE  =================
#define MODE_SD 0
#define MODE_BT 1

// =====================  BLE ID  =================
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR1_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHAR2_UUID "e3223119-9445-4e96-a4a1-85358c4046a2"

std::vector<String> mp3Files;

Ticker encTicker;
Ticker animationTicker;

Preferences prefs;
uint8_t activeMode;  // loaded from flash

// =====================  SHARED OBJECTS  ================
I2SStream i2s;  // one driver for both modes
RotaryEncoder encoder(ENCODER_CLK, ENCODER_DT, RotaryEncoder::LatchMode::TWO03);


CRGB leds[NUM_LEDS];
int brightness = 150;
uint8_t gHue = 0;             // rainbow base hue
CRGB baseColor = CRGB::Blue;  // color for breathing mode
int LEDCounter = 0;

// volatile float rmsVolume = 0;
float smoothedVolume = 0;

int currentSongIndex = 1;
String lastFile = "";
// breathing helper
int breatheBrightness = 0;
int breatheDelta = 2;
//CHANGE
float animationSpeed = 0.5f;
// ------------- Bluetooth path -------------
BluetoothA2DPSink a2dp_sink(i2s);
volatile int btLevel = 0;  // for music bar
bool btPlaying = true;     // play/pause state
bool plused = false;
bool sdPreviouslyAvailable = true;
unsigned long sdLastRetryTime = 0;
const unsigned long sdRetryCooldown = 2000;
// ------------- SD path -------------------
MP3DecoderHelix mp3dec;
AudioSourceSDMMC sdSource("/", "mp3");
AudioPlayer sdPlayer(sdSource, i2s, mp3dec);
bool sdOK = false;
bool sdPaused = false;
float GVolume = 0.2;
// =================  TIMING / DBL-CLICK  ================
uint32_t lastClick = 0;
bool waiting2nd = false;
const uint32_t DC_DELAY = 400;  // ms
uint32_t lastLedTime;

bool waitingForDouble = false;
unsigned long lastPressTime = 0;
const unsigned long dblDelay = 400;  // ms

bool waiting2ndSpeed = false;
uint32_t lastSpeedClick = 0;
const uint32_t DC_DELAY_SPEED = 400;
// =================  LED EFFECTS (same as before) =======
int breatheLvl = 0, breatheDir = 2;
int currentMode = 1, animDelay = 10;

bool lastEncBtn = HIGH;
bool lastBtnMode = HIGH;
bool lastBtnSpeed = HIGH;
bool lastBtnOn = HIGH;

// =================  BLE global variables =======
BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
BLECharacteristic* pCharacteristic_2 = NULL;
BLEDescriptor* pDescr;
BLE2902* pBLE2902;

bool deviceConnected = false;
bool oldDeviceConnected = false;
uint32_t value = 0;

void sendDataBle(String message);
void sendLightMessage(float value);
void sendSpeedMessage(float value);
void sendEffectMessage(int effect);


//Function prtotypes
// Add these prototypes near the top of main.cpp
void updateAnimationSpeed(float newSpeed);
void runCurrentMode();
void sendDataBle(String message);
void sendLightMessage(float value);
void sendSpeedMessage(float value);
void sendEffectMessage(int effect);
// =================  BLE ENUM variables =======
size_t walkDir(const char* folder,
               std::vector<String>& list,
               bool recurse = true)  // set false to stay in 1 dir
{
  File dir = SD_MMC.open(folder);
  if (!dir) {
    Serial.printf("❌  Can't open dir \"%s\"\n", folder);
    return 0;
  }

  size_t found = 0;
  File f = dir.openNextFile();
  while (f) {
    if (f.isDirectory()) {
      if (recurse) {
        found += walkDir(f.path(), list, true);  // dive deeper
      }
    } else {
      String name = f.name();  // "MUSIC/track01.MP3"
      name.toLowerCase();
      if (name.endsWith(".mp3")) {         // extension match
        list.push_back(String(f.path()));  // keep full path
        ++found;
      }
    }
    f = dir.openNextFile();
  }
  return found;
}

// convenience wrapper -------------------------------------------------
size_t buildMp3List(std::vector<String>& outList,
                    const char* startFolder = "/",  // where to begin
                    bool recurse = true)            // scan sub-dirs?
{
  outList.clear();
  return walkDir(startFolder, outList, recurse);
}

// int indexOfCurrentTrack(AudioSourceSDMMC &sdSource,
//                         const std::vector<String> &playlist)
// {
//   // 1️⃣  Ask the source for the complete path it is streaming:
//   String nowPlaying = sdSource.currentFilePath();     // e.g.  "/MUSIC/Track07.mp3"
//   if (nowPlaying.isEmpty()) return -1;                // not initialised?

//   // 2️⃣  Case-insensitive search inside the vector:
//   nowPlaying.toLowerCase();
//   for (size_t i = 0; i < playlist.size(); ++i) {
//     String candidate = playlist[i];
//     candidate.toLowerCase();
//     if (candidate == nowPlaying) return static_cast<int>(i);
//   }
//   return -1;  // not found
// }

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

// =================  SD Classes =======
class RMSStream : public AudioStream {
public:
  RMSStream(I2SStream& i2s)
    : i2s(i2s) {}

  size_t write(const uint8_t* buffer, size_t size) override {
    calculateRMS(buffer, size);
    return i2s.write(buffer, size);
  }

private:
  I2SStream& i2s;

  void calculateRMS(const uint8_t* d, uint32_t len) {
    const int16_t* s = (int16_t*)d;
    int32_t acc = 0;
    for (uint32_t i = 0; i < len / 2; i++) acc += abs(s[i]);
    smoothedVolume = acc / ((len / 2) * sqrt(GVolume + 0.01));
  }
};

RMSStream rmsStream(i2s);  // Our custom stream wrapper

// =================  BLE Classes =======
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("a new device is cannected!");
    uint16_t mtu = pServer->getPeerMTU(pServer->getConnId());
    Serial.print("Negotiated MTU from client: ");
    Serial.println(mtu);
    delay(100);  // give BLE stack time to settle

    sendEffectMessage(currentMode);
    //CHANGE
    sendLightMessage(animationSpeed);
  };

  void onDisconnect(BLEServer* pServer) override {
    deviceConnected = false;
    Serial.println("Client disconnected. Restarting advertising...");
    delay(100);  // give BLE stack time to settle
    pServer->startAdvertising();
  }
};


class CharacteristicCallBack : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pChar) override {
    Serial.println("onWrite is called!!");

    String jsonStr = pChar->getValue().c_str();
    Serial.println("Received string: " + jsonStr);

    StaticJsonDocument<128> doc;
    DeserializationError err = deserializeJson(doc, jsonStr);

    if (err) {
      Serial.print("JSON parse error: ");
      Serial.println(err.c_str());
      return;
    }

    String cmdStr = doc["cmd"];
    CommandType cmd = parseCommandType(cmdStr);

    Serial.print("Command: ");
    Serial.println(cmd);

    switch (cmd) {
      case CMD_LIGHT:
        {
          float brightness_cmd = doc["value"];
          Serial.printf("💡 Set LIGHT to %.2f\n", brightness_cmd);
          if (brightness_cmd < 0) brightness_cmd = 0;
          FastLED.setBrightness(brightness_cmd * 100);
          // apply brightness
          break;
        }
      case CMD_SPEED:
        {
          float speed = doc["value"];
          Serial.printf("⚡ Set SPEED to %.2f\n", speed);
          if (speed < 0) speed = 0;
          //CHANGE
          updateAnimationSpeed(speed);
          Serial.printf("⚡ Set animationSpeed to %f\n", animationSpeed);
          break;
        }
      case CMD_EFFECT:
        {
          String effectStr = doc["value"];
          EffectType effect = parseEffectType(effectStr);
          Serial.printf("✨ Set EFFECT to %s\n", effectStr.c_str());
          currentMode = effect;
          break;
        }
      default:
        Serial.println("❗Unknown command");
    }
  }
};

void tickEncoder() {
  encoder.tick();
}

void solidPulse(CRGB c = CRGB::Blue) {
  fill_solid(leds, NUM_LEDS, c);
  FastLED.show();
}

void rainbowDance() {
  fill_rainbow(leds, NUM_LEDS, gHue, 10);
  gHue += 5;
  FastLED.show();
}

void breathingEffect(CRGB c, uint8_t speed = 10) {
  breatheBrightness += breatheDelta;
  if (breatheBrightness == 0 || breatheBrightness == 255) breatheDelta = -breatheDelta;
  CRGB cur = c;
  cur.nscale8_video(breatheBrightness);
  fill_solid(leds, NUM_LEDS, cur);
  FastLED.show();
}

void sparkleEffect(CRGB sparkle = CRGB::White, uint8_t chance = 60, uint8_t fadeAmt = 20, uint8_t delayT = 10) {
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
  static uint32_t last = 0;
  uint32_t now = millis();

  // Adjust timing based on animationSpeed (0 to 1)
  float speedFactor = (1.0f - animationSpeed) * 9.0f + 1.0f;  // → 1 to 10
  uint16_t baseTime = 50;                                     // base speed when animationSpeed = 1.0
  uint16_t onMs = baseTime * speedFactor;
  uint16_t offMs = baseTime * speedFactor;

  uint16_t dur = on ? onMs : offMs;
  if (now - last >= dur) {
    on = !on;
    last = now;
  }

  fill_solid(leds, NUM_LEDS, on ? color : CRGB::Black);
  FastLED.show();
}

void strayNightEffect(uint8_t chance = 60, uint8_t fadeAmt = 20) {
  if (random8() < chance) leds[random(NUM_LEDS)] = CHSV(random8(), 200, 255);
  fadeToBlackBy(leds, NUM_LEDS, fadeAmt);
  FastLED.show();
}

void musicSync() {
  int raw = (activeMode == MODE_SD) ? smoothedVolume : btLevel;
  int ledLevel = map(raw, 0, 3000, 0, NUM_LEDS);
  ledLevel = constrain(ledLevel, 0, NUM_LEDS);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  for (int i = 0; i < ledLevel; ++i) leds[i] = CHSV(gHue + i * 10, 255, 255);
  ++gHue;
  FastLED.show();
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
    default: solidPulse(); break;
  }
}

// ============================================================
//                  INPUT HANDLERS (encoder & buttons)
// ============================================================
void handleEncoderRotation() {
  static int lastPos = encoder.getPosition();
  int pos = encoder.getPosition();
  if (pos != lastPos) {
    int diff = pos - lastPos;
    if (activeMode == MODE_SD) {
      float vol = GVolume * 100;
      vol = constrain(vol - diff, 0, 100);
      GVolume = vol / 100;
      sdPlayer.setVolume(GVolume);
      Serial.printf("SdVolume: %f%%\n", vol);
    } else {
      int vol = a2dp_sink.get_volume();
      vol = constrain(vol - diff, 0, 100);
      a2dp_sink.set_volume(vol);
      Serial.printf("BTVolume: %d%%\n", vol);
    }
    lastPos = pos;
  }
}

void handleEncoderButton() {
  bool reading = digitalRead(ENCODER_SW);
  if (reading != lastEncBtn) {
    delay(10);
    reading = digitalRead(ENCODER_SW);
  }

  if (reading == LOW && lastEncBtn == HIGH) {
    uint32_t now = millis();
    if (waitingForDouble && (now - lastPressTime < dblDelay)) {
      waitingForDouble = false;
      if (activeMode == MODE_SD) {
        int total = buildMp3List(mp3Files);
        if (currentSongIndex < total) {
          sdPlayer.next();
          currentSongIndex++;  // <─── NEW
          Serial.println("Next Button:");
          Serial.println(currentSongIndex);
        } else {
          // At end — wrap to first song
          currentSongIndex = 1;
          sdPlayer.setIndex(0);
          sdPlayer.play();  // always reset decoder cleanly
        }
      } else {
        a2dp_sink.next();
      }
      Serial.println("⏭️  Next track (double press)");
    } else {
      waitingForDouble = true;
      lastPressTime = now;
    }
  }

  if (waitingForDouble && (millis() - lastPressTime > dblDelay)) {
    waitingForDouble = false;
    if (activeMode == MODE_SD) {
      sdPaused = !sdPaused;
    } else {
      if (btPlaying) {
        a2dp_sink.pause();
        Serial.println("⏸️  Pause");
      } else {
        a2dp_sink.play();
        Serial.println("▶️  Play");
      }
      btPlaying = !btPlaying;
    }
  }

  lastEncBtn = reading;
}

void handleExtraButtons() {
  bool btnMode = digitalRead(BTN_LED_MODE);
  bool btnSpeed = digitalRead(BTN_LED_SPEED);
  bool btnOn = digitalRead(BTN_TOGGLE);

  if (btnMode == LOW && lastBtnMode == HIGH) {
    currentMode = (currentMode + 1) % 8;
    Serial.printf("LED mode → %d\n", currentMode);
    sendEffectMessage(currentMode);
  }

  if (btnSpeed == LOW && lastBtnSpeed == HIGH) {
    //CHANGE

    animationSpeed -= 0.3;
    if (animationSpeed <= 0) animationSpeed = 1;
    updateAnimationSpeed(animationSpeed);
    Serial.printf("Anim speed → %f ms\n", animationSpeed);
    sendSpeedMessage(animationSpeed);
  }

  if (btnOn == LOW && lastBtnOn == HIGH) {
    bool newState = FastLED.getBrightness() == 0;
    FastLED.setBrightness(newState ? brightness : 0);
    FastLED.show();
    Serial.println(newState ? "LEDs ON" : "LEDs OFF");
  }

  lastBtnMode = btnMode;
  lastBtnSpeed = btnSpeed;
  lastBtnOn = btnOn;
}

// =============  BT ANALYSER (music bar) ================
void volTap(const uint8_t* d, uint32_t len) {
  const int16_t* s = (int16_t*)d;
  int32_t acc = 0;
  for (uint32_t i = 0; i < len / 2; i++) acc += abs(s[i]);
  btLevel = acc / ((len / 2) * sqrt(GVolume + 0.01));
}

// =============  MODE-SWAP + REBOOT  ====================
void storeModeAndReboot(uint8_t newMode) {
  prefs.begin("player", true);
  prefs.putUChar("mode", newMode);
  prefs.end();
  delay(50);
  ESP.restart();
}

// =============  HARDWARE INIT  SHARED  =================
void commonHardware() {
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT);
  pinMode(BTN_LED_MODE, INPUT_PULLUP);
  pinMode(BTN_LED_SPEED, INPUT_PULLUP);
  pinMode(BTN_TOGGLE, INPUT_PULLUP);

  FastLED.addLeds<WS2812B, LED_PIN, RGB>(leds, NUM_LEDS);
  FastLED.setBrightness(brightness);
  FastLED.clear(true);
  FastLED.show();

  auto cfg = i2s.defaultConfig(TX_MODE);
  cfg.pin_bck = I2S_BCLK;
  cfg.pin_ws = I2S_LRCK;
  cfg.pin_data = I2S_DOUT;
  cfg.buffer_size = 512;
  i2s.begin(cfg);
}

// =============  INIT  SD  ===============================
void initSDmode() {
  Serial.println("→ Initialising SD mode");

  if (SD_MMC.begin() && SD_MMC.cardType() != CARD_NONE) {
    sdPlayer.setOutput(rmsStream);  // for LED volume sync
    sdPlayer.setVolume(GVolume);    // set initial volume

    sdOK = sdPlayer.begin();  // scan mp3 files, set up decoder
    if (!sdOK) {
      Serial.println("No MP3 files found.");
      return;
    }

    // ✅ Auto-play first song
    sdPlayer.setIndex(0);  // start from the first song
    sdPlayer.play();

    Serial.println("✅ Auto-playing first song on SD card...");
  } else {
    Serial.println("SD mount failed – falling back to Bluetooth");
    storeModeAndReboot(MODE_BT);
  }
}
// =============  INIT  BLE  ===============================
void bleInitStart() {
  // Create the BLE Device
  BLEDevice::init("KingH Music Player");
  BLEDevice::setMTU(256);
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService* pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
    CHAR1_UUID,
    BLECharacteristic::PROPERTY_NOTIFY);

  pCharacteristic_2 = pService->createCharacteristic(
    CHAR2_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);

  // Create a BLE Descriptor

  pDescr = new BLEDescriptor((uint16_t)0x2901);
  pDescr->setValue("A very interesting variable");
  pCharacteristic->addDescriptor(pDescr);

  pBLE2902 = new BLE2902();
  pBLE2902->setNotifications(true);

  // Add all Descriptors here
  pCharacteristic->addDescriptor(pBLE2902);
  pCharacteristic_2->addDescriptor(new BLE2902());

  // After defining the desriptors, set the callback functions
  pCharacteristic_2->setCallbacks(new CharacteristicCallBack());

  pService->start();
}

// =============  Start Advertise  BLE  ===============================
void bleStartAdvertise() {
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
  Serial.println("Waiting a client connection to notify...");
}

// =============  INIT  BT  ===============================
void initBTmode() {
  Serial.println("→ Initialising Bluetooth mode");
  a2dp_sink.set_auto_reconnect(true);
  a2dp_sink.set_stream_reader(volTap, true);  // analyser
  a2dp_sink.start("KingH Music Player", true);
  a2dp_sink.set_volume(GVolume * 100);


  bleInitStart();
  bleStartAdvertise();
}

// ===================  BUTTON HANDLER  ===================
void handleToggleBtn() {
  bool st = digitalRead(BTN_TOGGLE);
  static bool last = HIGH;
  if (st != last) {
    delay(10);
    st = digitalRead(BTN_TOGGLE);
  }
  if (st == LOW && last == HIGH) {
    uint32_t now = millis();
    if (waiting2nd && (now - lastClick < DC_DELAY)) {
      waiting2nd = false;
      uint8_t newMode = (activeMode == MODE_SD) ? MODE_BT : MODE_SD;
      Serial.printf("Double-click: switching to %s and rebooting\n", newMode == MODE_BT ? "BT" : "SD");
      storeModeAndReboot(newMode);
      // storeModeAndReboot(newMode, currentMode, GVolume, brightness);
    } else {
      waiting2nd = true;
      lastClick = now;
    }
  }
  if (waiting2nd && (millis() - lastClick > DC_DELAY)) waiting2nd = false;
  last = st;
}

void sendDataBle(String message) {
  if (deviceConnected) {
    pCharacteristic->setValue(message.c_str());
    pCharacteristic->notify();
  }
}

void sendLightMessage(float value) {
  StaticJsonDocument<128> doc;
  doc["cmd"] = "LIGHT";
  doc["value"] = value;
  String json_string;
  serializeJson(doc, json_string);
  sendDataBle(json_string);
}

void sendSpeedMessage(float value) {
  StaticJsonDocument<128> doc;
  doc["cmd"] = "SPEED";
  doc["value"] = value;
  String json_string;
  serializeJson(doc, json_string);
  sendDataBle(json_string);
}

void sendEffectMessage(int effect) {
  StaticJsonDocument<128> doc;
  doc["cmd"] = "EFFECT";
  doc["value"] = effect;
  String json_string;
  serializeJson(doc, json_string);
  sendDataBle(json_string);
}

void updateAnimationSpeed(float newSpeed) {
  animationSpeed = newSpeed;

  // Avoid 0ms interval
  float intervalMs = ((1.0f - animationSpeed) + 0.1f) * 25.0f;

  animationTicker.detach();                               // 🧹 stop old ticker
  animationTicker.attach_ms(intervalMs, runCurrentMode);  // ✅ reattach with new speed
}


// =======================  SETUP  ========================
// void setup() {
//   Serial.begin(115200);
//   Serial.println("TESTSTSTTSTS");
//   delay(1000);
//   Serial.println("sdsdsdssdsd");
//   if (esp_reset_reason() == ESP_RST_POWERON) {
//     pinMode(LED_PIN, OUTPUT);  // drive DIN LOW early
//     digitalWrite(LED_PIN, LOW);
//     delay(400);  // 200-400 ms lets even slow USB supplies settle
//   }

  
//   encTicker.attach_ms(1, tickEncoder);
//   prefs.begin("player", false);
//   activeMode = prefs.getUChar("mode", MODE_SD);

//   commonHardware();

//   if (activeMode == MODE_BT) initBTmode();
//   else initSDmode();


//   Serial.println("Double-click BTN22 to switch source.");
//   lastLedTime = millis();
//   updateAnimationSpeed(0.5f);
// }

void setup() {
  Serial.begin(115200);
  Serial.println("--- System Initializing ---");
  delay(1000); 
  Serial.println("--- System Initializing ---");

  encTicker.attach_ms(1, tickEncoder);
  prefs.begin("player", false);
  activeMode = prefs.getUChar("mode", MODE_SD);

  commonHardware(); // Initialize I2S first

  // Add a safety check: if SD is missing, don't just reboot, 
  // maybe switch to BT mode automatically without a hard restart
  if (activeMode == MODE_SD) {
      Serial.println("Checking SD Card...");
      if (!SD_MMC.begin() || SD_MMC.cardType() == CARD_NONE) {
          Serial.println("No SD Card found! Switching to BT Mode...");
          activeMode = MODE_BT; // Override for this session
      }
  }

  if (activeMode == MODE_BT) {
      initBTmode();
  } else {
      initSDmode();
  }

  updateAnimationSpeed(0.5f);
  Serial.println("System Ready.");
}

void runModeSpeed() {
  uint32_t currentTime = millis();
  uint32_t minTimeDif = ((1 - animationSpeed) + 0.1) * 60;
  Serial.printf("delta is: %d,  minDif is: %d", currentTime - lastLedTime, minTimeDif);
  if ((currentTime - lastLedTime) > minTimeDif) {
    lastLedTime = currentTime;
    runCurrentMode();
  }
}

// =======================  LOOP  =========================
void loop() {
  Serial.println("test");
  delay(500);
  handleToggleBtn();
  handleEncoderRotation();
  handleEncoderButton();
  handleExtraButtons();

  // ===== Safe SD playback =====
  if (activeMode == MODE_SD && sdOK && !sdPaused) {
    if (SD_MMC.cardType() != CARD_NONE) {
      sdPlayer.copy();  // Safe copy only if card present
    } else {
      Serial.println("⚠️ SD card disappeared during playback!");
      sdPlayer.end();   // Stop player safely
      sdOK = false;     // Mark SD unavailable
      sdPaused = true;  // Prevent further attempts
    }
  }

  // run source-specific processing
  if (activeMode == MODE_SD && sdOK && !sdPaused) {
    bool playing = sdPlayer.copy();  // true ⇒ still copying samples
    if (!sdPaused && !playing && !plused) {
      int total = buildMp3List(mp3Files);
      currentSongIndex++;  // 👈 bump counter
      if (currentSongIndex >= total) {
        currentSongIndex = 1;
        sdPlayer.setIndex(0);
        sdPlayer.play();  // always reset decoder cleanly
      }
      plused = true;
      Serial.println("Self Next:");
      Serial.println(currentSongIndex);
    } else if (playing) {
      plused = false;
    }
  }
  // BT path is interrupt-driven by the sink


  // BT test send Data
  // if ((value % 100) == 0) sendDataBle();
  if (deviceConnected && !oldDeviceConnected) {
    // do stuff here on connecting
    oldDeviceConnected = deviceConnected;
  }
}
