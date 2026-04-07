#ifndef MOCK_AUDIO_MANAGER_H
#define MOCK_AUDIO_MANAGER_H

#include "AudioManager.h"

class MockAudioManager : public AudioManager {
private:
    int _pinL;
    int _pinR;
    float _volume;
    bool _playing;
    AudioMode _mode;
    unsigned long _lastUpdate;

public:
    MockAudioManager(int pinL, int pinR);

    void begin() override;
    void update() override;
    void play() override;
    void pause() override;
    void next() override;
    void setVolume(float volume) override;
    float getVolume() override;
    int getAudioLevel() override;
    void setMode(AudioMode mode) override;
    AudioMode getMode() override;
    bool isPlaying() override;
};

#endif