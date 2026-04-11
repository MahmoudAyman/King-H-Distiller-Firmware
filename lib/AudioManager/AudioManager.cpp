/**
 * File: src/AudioManager.cpp
 * Project: King-H Distiller Firmware
 * Description: Implementation for AudioManager with corrected copier logic
 */

#include "AudioManager.h"

void (*AudioManager::audioLevelCallback)(int) = nullptr;

class RMSStream : public AudioStream {
public:
    RMSStream(I2SStream& i2s, float& vol, void (*cb)(int)) : i2s(i2s), vol(vol), cb(cb) {}
    size_t write(const uint8_t* buffer, size_t size) override {
        const int16_t* s = (int16_t*)buffer;
        int32_t acc = 0;
        for (uint32_t i = 0; i < size / 2; i++) acc += abs(s[i]);
        int level = acc / ((size / 2) * sqrt(vol + 0.01));
        if (cb) cb(level);
        return i2s.write(buffer, size);
    }
private:
    I2SStream& i2s;
    float& vol;
    void (*cb)(int);
};

static RMSStream* rms;

AudioManager::AudioManager() : sdSource("/", "mp3") {
    sdPlayer = new AudioPlayer(sdSource, i2s, mp3dec);
    a2dp_sink = new BluetoothA2DPSink(i2s);
}

void AudioManager::begin(uint8_t mode) {
    currentMode = mode;
    Serial.println("AudioManager: Initializing I2S for UDA1334...");

    auto cfg = i2s.defaultConfig(TX_MODE);
    cfg.pin_bck = I2S_BCLK;
    cfg.pin_ws = I2S_LRCK;
    cfg.pin_data = I2S_DOUT;
    cfg.sample_rate = 44100;
    cfg.bits_per_sample = 16;
    cfg.i2s_format = I2S_STD_FORMAT; 
    cfg.buffer_size = 512;

    if (!i2s.begin(cfg)) {
        Serial.println("AudioManager: I2S Start Failed");
        return;
    }

    rms = new RMSStream(i2s, globalVolume, audioLevelCallback);
    setVolume(0.03f);

    if (mode == MODE_SD) {
        if (USE_SD_CARD && SD_MMC.begin()) {
            if (SD_MMC.cardType() != CARD_NONE) {
                sdPlayer->setOutput(*rms);
                sdPlayer->setVolume(globalVolume);
                if (sdPlayer->begin()) {
                    walkDir("/", true);
                    sdPlayer->setIndex(0);
                    sdPlayer->play();
                }
            }
        }
    } else {
        a2dp_sink->set_auto_reconnect(true);
        a2dp_sink->set_stream_reader(btDataCallback, true);
        a2dp_sink->start("KingH Music Player", true);
    }
}

void AudioManager::startBeepTest() {
    Serial.println("AudioManager: Beep Test Starting...");
    isTestMode = true;
    
    // Correct SineWave Setup for recent AudioTools
    auto info = i2s.info();
    sineWave.begin(info, 440); // 440Hz
    copier.begin(i2s, sineWave);
}

void AudioManager::stopBeepTest() {
    isTestMode = false;
}

void AudioManager::update() {
    if (isTestMode) {
        copier.copy();
        return;
    }

    if (currentMode == MODE_SD && !sdPaused) {
        bool playing = sdPlayer->copy();
        if (!playing && !isPlayingNext) {
            if (mp3Files.size() > 0) {
                if (++currentSongIndex >= mp3Files.size()) currentSongIndex = 0;
                sdPlayer->setIndex(currentSongIndex);
                sdPlayer->play();
                isPlayingNext = true;
            }
        } else if (playing) {
            isPlayingNext = false;
        }
    }
}

void AudioManager::setVolume(float vol) {
    globalVolume = constrain(vol, 0.0f, MAX_VOLUME_LIMIT);
    if (currentMode == MODE_SD) {
        sdPlayer->setVolume(globalVolume);
    } else {
        a2dp_sink->set_volume(globalVolume * 100);
    }
}

float AudioManager::getVolume() { return globalVolume; }

void AudioManager::next() {
    if (currentMode == MODE_SD) {
        if (mp3Files.size() > 0) {
            if (++currentSongIndex >= mp3Files.size()) currentSongIndex = 0;
            sdPlayer->setIndex(currentSongIndex);
            sdPlayer->play();
        }
    } else a2dp_sink->next();
}

void AudioManager::togglePause() {
    if (currentMode == MODE_SD) sdPaused = !sdPaused;
    else {
        if (a2dp_sink->get_audio_state() == ESP_A2D_AUDIO_STATE_STARTED) a2dp_sink->pause();
        else a2dp_sink->play();
    }
}

size_t AudioManager::walkDir(const char* folder, bool recurse) {
    File dir = SD_MMC.open(folder);
    if (!dir) return 0;
    size_t found = 0;
    File f = dir.openNextFile();
    while (f) {
        if (f.isDirectory()) {
            if (recurse) found += walkDir(f.path(), true);
        } else {
            String name = f.name();
            name.toLowerCase();
            if (name.endsWith(".mp3")) {
                mp3Files.push_back(String(f.path()));
                ++found;
            }
        }
        f = dir.openNextFile();
    }
    return found;
}

void AudioManager::btDataCallback(const uint8_t* data, uint32_t len) {
    const int16_t* s = (int16_t*)data;
    int32_t acc = 0;
    for (uint32_t i = 0; i < len / 2; i++) acc += abs(s[i]);
    int level = acc / (len / 2); 
    if (audioLevelCallback) audioLevelCallback(level);
}