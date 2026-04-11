/**
 * File: include/AudioManager.h
 * Project: King-H Distiller Firmware
 * Description: Header for AudioManager with corrected Diagnostic Tone types
 */

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

class AudioManager {
private:
    I2SStream i2s;
    MP3DecoderHelix mp3dec;
    AudioSourceSDMMC sdSource;
    AudioPlayer* sdPlayer;
    BluetoothA2DPSink* a2dp_sink;
    
    // Diagnostic Tone Generator
    SineWaveGenerator<int16_t> sineWave;
    StreamCopy copier; // Replaces GeneratedAudioStream for copying data
    bool isTestMode = false;

    std::vector<String> mp3Files;
    float globalVolume = 0.03f; 
    uint8_t currentMode = MODE_SD;
    int currentSongIndex = 0;
    bool sdPaused = false;
    bool isPlayingNext = false;

    size_t walkDir(const char* folder, bool recurse);
    static void (*audioLevelCallback)(int);

public:
    AudioManager();
    void begin(uint8_t mode);
    void update(); 
    
    // Diagnostic Controls
    void startBeepTest();
    void stopBeepTest();
    
    void setVolume(float vol);
    float getVolume();
    void next();
    void togglePause();
    void setAudioLevelCallback(void (*cb)(int)) { audioLevelCallback = cb; }
    
    static void btDataCallback(const uint8_t* data, uint32_t len);
};

#endif