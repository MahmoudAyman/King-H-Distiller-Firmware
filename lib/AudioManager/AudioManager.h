// #ifndef AUDIO_MANAGER_H
// #define AUDIO_MANAGER_H

// #include <Arduino.h>
// #include <vector>
// #include <SD_MMC.h>
// #include <BluetoothA2DPSink.h>
// #include "AudioTools.h"
// #include "AudioTools/Disk/AudioSourceSDMMC.h"
// #include "AudioTools/AudioCodecs/CodecMP3Helix.h"
// #include "../../include/Config.h"

// // Forward declaration to avoid circular dependency if needed later
// class LedManager;

// class AudioManager {
// private:
//     I2SStream i2s;
//     MP3DecoderHelix mp3dec;
//     AudioSourceSDMMC sdSource;
//     AudioPlayer* sdPlayer;
//     BluetoothA2DPSink* a2dp_sink;
    
//     std::vector<String> mp3Files;
//     float globalVolume = 0.2f;
//     uint8_t currentMode = MODE_SD;
//     int currentSongIndex = 0;
//     bool sdPaused = false;
//     bool isPlayingNext = false;

//     // Helper for SD file listing
//     size_t walkDir(const char* folder, bool recurse);
    
//     // Callback for LED music sync
//     static void (*audioLevelCallback)(int);

// public:
//     AudioManager();
//     void begin(uint8_t mode);
//     void update(); // Called in main loop
    
//     void setVolume(float vol);
//     float getVolume();
    
//     void next();
//     void togglePause();
    
//     void setAudioLevelCallback(void (*cb)(int)) { audioLevelCallback = cb; }
    
//     static void btDataCallback(const uint8_t* data, uint32_t len);
// };

// #endif


#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <Arduino.h>
#include <vector>
#include <SD_MMC.h>
#include <BluetoothA2DPSink.h>
#include "AudioTools.h"
#include "AudioTools/Disk/AudioSourceSDMMC.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "../../include/Config.h"

// Forward declaration to avoid circular dependency
class LedManager;

class AudioManager {
private:
    // Using AnalogAudioStream for ESP32 Internal DAC (Pins 25 & 26)
    AnalogAudioStream out; 
    MP3DecoderHelix mp3dec;
    AudioSourceSDMMC sdSource;
    AudioPlayer* sdPlayer;
    BluetoothA2DPSink* a2dp_sink;
    
    std::vector<String> mp3Files;
    float globalVolume = 0.2f;
    uint8_t currentMode = MODE_SD;
    int currentSongIndex = 0;
    bool sdPaused = false;
    bool isPlayingNext = false;

    // Helper for SD file listing
    size_t walkDir(const char* folder, bool recurse);
    
    // Callback for LED music sync
    static void (*audioLevelCallback)(int);

public:
    AudioManager();
    void begin(uint8_t mode);
    void update(); // Called in main loop
    
    void setVolume(float vol);
    float getVolume();
    
    void next();
    void togglePause();
    
    void setAudioLevelCallback(void (*cb)(int)) { audioLevelCallback = cb; }
    
    static void btDataCallback(const uint8_t* data, uint32_t len);
};

#endif