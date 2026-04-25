#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <Arduino.h>
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "AudioTools/Disk/AudioSourceSDMMC.h"
#include "BluetoothA2DPSink.h"
#include "SD_MMC.h"
#include "../../include/Config.h"
#include <vector>

class AudioManager {
public:
    AudioManager();
    void begin(uint8_t mode);
    void update();
    void setVolume(float vol);
    float getVolume();
    void next();
    void togglePause();
    void setAudioLevelCallback(void (*cb)(int)) { audioLevelCallback = cb; }

private:
    I2SStream i2s;
    MP3DecoderHelix mp3dec;
    AudioSourceSDMMC sdSource;
    AudioPlayer* sdPlayer;
    BluetoothA2DPSink* a2dp_sink;

    uint8_t currentMode;
    float globalVolume = 0.3f;
    bool sdPaused = false;
    bool isPlayingNext = false;
    int currentSongIndex = 0;
    std::vector<String> mp3Files;

    static void (*audioLevelCallback)(int);
    static void btDataCallback(const uint8_t* data, uint32_t len);
    size_t walkDir(const char* folder, bool recurse);
};

#endif