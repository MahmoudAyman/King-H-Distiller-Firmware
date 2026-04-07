#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <Arduino.h>

enum AudioMode {
    MODE_SD_CARD,
    MODE_BLUETOOTH
};

/**
 * Base Interface for Audio Management
 * This allows the main code to control audio without knowing
 * if it's running on an ESP32 with SD/BT or an Uno with a buzzer.
 */
class AudioManager {
public:
    virtual ~AudioManager() {}
    
    virtual void begin() = 0;
    virtual void update() = 0;
    
    virtual void play() = 0;
    virtual void pause() = 0;
    virtual void next() = 0;
    
    virtual void setVolume(float volume) = 0;
    virtual float getVolume() = 0;
    
    virtual int getAudioLevel() = 0;
    virtual void setMode(AudioMode mode) = 0;
    virtual AudioMode getMode() = 0;

    virtual bool isPlaying() = 0;
};

#endif