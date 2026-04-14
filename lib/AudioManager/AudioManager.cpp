// #include "AudioManager.h"

// void (*AudioManager::audioLevelCallback)(int) = nullptr;

// class RMSStream : public AudioStream {
// public:
//     RMSStream(I2SStream& i2s, float& vol, void (*cb)(int)) : i2s(i2s), vol(vol), cb(cb) {}
//     size_t write(const uint8_t* buffer, size_t size) override {
//         const int16_t* s = (int16_t*)buffer;
//         int32_t acc = 0;
//         for (uint32_t i = 0; i < size / 2; i++) acc += abs(s[i]);
//         int level = acc / ((size / 2) * sqrt(vol + 0.01));
//         if (cb) cb(level);
//         return i2s.write(buffer, size);
//     }
// private:
//     I2SStream& i2s;
//     float& vol;
//     void (*cb)(int);
// };

// static RMSStream* rms;

// AudioManager::AudioManager() : sdSource("/", "mp3") {
//     sdPlayer = new AudioPlayer(sdSource, i2s, mp3dec);
//     a2dp_sink = new BluetoothA2DPSink(i2s);
// }

// void AudioManager::begin(uint8_t mode) {
//     currentMode = mode;
//     Serial.printf("AudioManager: Initializing in %s mode...\n", (mode == MODE_BT ? "Bluetooth" : "SD"));

//     auto cfg = i2s.defaultConfig(TX_MODE);
//     cfg.pin_bck = I2S_BCLK;
//     cfg.pin_ws = I2S_LRCK;
//     cfg.pin_data = I2S_DOUT;
//     i2s.begin(cfg);

//     rms = new RMSStream(i2s, globalVolume, audioLevelCallback);

//     if (mode == MODE_SD) {
//         if (USE_SD_CARD && SD_MMC.begin()) {
//             if (SD_MMC.cardType() != CARD_NONE) {
//                 sdPlayer->setOutput(*rms);
//                 setVolume(globalVolume); // Ensure safety cap is applied
//                 if (sdPlayer->begin()) {
//                     size_t count = walkDir("/", true);
//                     Serial.printf("AudioManager: SD Found %d MP3 files.\n", (int)count);
//                     sdPlayer->setIndex(0);
//                     sdPlayer->play();
//                 }
//             }
//         }
//     } else {
//         a2dp_sink->set_auto_reconnect(true);
//         a2dp_sink->set_stream_reader(btDataCallback, true);
//         a2dp_sink->start("KingH Music Player", true);
//         setVolume(globalVolume); // Ensure safety cap is applied
//     }
// }

// void AudioManager::update() {
//     if (currentMode == MODE_SD && !sdPaused) {
//         bool playing = sdPlayer->copy();
//         if (!playing && !isPlayingNext) {
//             if (mp3Files.size() > 0) {
//                 if (++currentSongIndex >= mp3Files.size()) currentSongIndex = 0;
//                 sdPlayer->setIndex(currentSongIndex);
//                 sdPlayer->play();
//                 isPlayingNext = true;
//             }
//         } else if (playing) {
//             isPlayingNext = false;
//         }
//     }
// }

// void AudioManager::setVolume(float vol) {
//     // Apply safety cap defined in Config.h
//     globalVolume = constrain(vol, 0.0f, MAX_VOLUME_LIMIT);
    
//     if (currentMode == MODE_SD) {
//         sdPlayer->setVolume(globalVolume);
//     } else {
//         a2dp_sink->set_volume(globalVolume * 100);
//     }
//     Serial.printf("AudioManager: Volume set to %.2f (Cap: %.2f)\n", globalVolume, MAX_VOLUME_LIMIT);
// }

// float AudioManager::getVolume() {
//     return globalVolume;
// }

// void AudioManager::next() {
//     Serial.println("AudioManager: Next Track");
//     if (currentMode == MODE_SD) {
//         if (mp3Files.size() > 0) {
//             if (++currentSongIndex >= mp3Files.size()) currentSongIndex = 0;
//             sdPlayer->setIndex(currentSongIndex);
//             sdPlayer->play();
//         }
//     } else {
//         a2dp_sink->next();
//     }
// }

// void AudioManager::togglePause() {
//     if (currentMode == MODE_SD) {
//         sdPaused = !sdPaused;
//         Serial.printf("AudioManager: SD %s\n", sdPaused ? "Paused" : "Resumed");
//     } else {
//         if (a2dp_sink->get_audio_state() == ESP_A2D_AUDIO_STATE_STARTED) {
//             a2dp_sink->pause();
//         } else {
//             a2dp_sink->play();
//         }
//     }
// }

// size_t AudioManager::walkDir(const char* folder, bool recurse) {
//     File dir = SD_MMC.open(folder);
//     if (!dir) return 0;
//     size_t found = 0;
//     File f = dir.openNextFile();
//     while (f) {
//         if (f.isDirectory()) {
//             if (recurse) found += walkDir(f.path(), true);
//         } else {
//             String name = f.name();
//             name.toLowerCase();
//             if (name.endsWith(".mp3")) {
//                 mp3Files.push_back(String(f.path()));
//                 ++found;
//             }
//         }
//         f = dir.openNextFile();
//     }
//     return found;
// }

// void AudioManager::btDataCallback(const uint8_t* data, uint32_t len) {
//     const int16_t* s = (int16_t*)data;
//     int32_t acc = 0;
//     for (uint32_t i = 0; i < len / 2; i++) acc += abs(s[i]);
//     int level = acc / (len / 2); 
//     if (audioLevelCallback) audioLevelCallback(level);
// }

/* WORKING */

#include "AudioManager.h"

void (*AudioManager::audioLevelCallback)(int) = nullptr;

/**
 * RMSStream Proxy
 * Intercepts audio data to calculate volume levels for LED synchronization
 * before passing it to the internal DAC.
 */
class RMSStream : public AudioStream {
public:
    RMSStream(AnalogAudioStream& out, float& vol, void (*cb)(int)) : out(out), vol(vol), cb(cb) {}
    size_t write(const uint8_t* buffer, size_t size) override {
        const int16_t* s = (int16_t*)buffer;
        int32_t acc = 0;
        // Simple RMS-like calculation for LED visualization
        for (uint32_t i = 0; i < size / 2; i++) acc += abs(s[i]);
        int level = acc / ((size / 2) * sqrt(vol + 0.01));
        if (cb) cb(level);
        return out.write(buffer, size);
    }
private:
    AnalogAudioStream& out;
    float& vol;
    void (*cb)(int);
};

static RMSStream* rms;

AudioManager::AudioManager() : sdSource("/", "mp3") {
    // Initialize player with the analog output
    sdPlayer = new AudioPlayer(sdSource, out, mp3dec);
    // Note: A2DP Sink will be configured to use the internal DAC in begin()
    a2dp_sink = new BluetoothA2DPSink(); 
}

void AudioManager::begin(uint8_t mode) {
    currentMode = mode;
    Serial.printf("AudioManager: Initializing in %s mode (Internal DAC Plan B)...\n", (mode == MODE_BT ? "Bluetooth" : "SD"));

    // Configure Analog Output for Internal DAC
    auto cfg = out.defaultConfig(TX_MODE);
    cfg.sample_rate = 44100;
    cfg.channels = 2;
    cfg.bits_per_sample = 16;
    out.begin(cfg);

    // Setup the visualization proxy
    rms = new RMSStream(out, globalVolume, audioLevelCallback);

    if (mode == MODE_SD) {
        if (USE_SD_CARD && SD_MMC.begin()) {
            if (SD_MMC.cardType() != CARD_NONE) {
                // Route SD Player through RMS visualization to Analog out
                sdPlayer->setOutput(*rms);
                setVolume(globalVolume); 
                if (sdPlayer->begin()) {
                    size_t count = walkDir("/", true);
                    Serial.printf("AudioManager: SD Found %d MP3 files.\n", (int)count);
                    sdPlayer->setIndex(0);
                    sdPlayer->play();
                }
            }
        }
    } else {
        // Configure A2DP to use Internal DAC
        // ESP32-A2DP supports internal DAC by passing 'true' to start or setting i2s_config
        a2dp_sink->set_auto_reconnect(true);
        a2dp_sink->set_stream_reader(btDataCallback, true);
        
        // Start with internal DAC enabled
        a2dp_sink->start("KingH Music Player", true); 
        setVolume(globalVolume);
    }
}

void AudioManager::update() {
    if (currentMode == MODE_SD && !sdPaused) {
        bool playing = sdPlayer->copy();
        if (!playing && !isPlayingNext) {
            if (mp3Files.size() > 0) {
                if (++currentSongIndex >= (int)mp3Files.size()) currentSongIndex = 0;
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
        // Bluetooth volume is 0-127
        a2dp_sink->set_volume(globalVolume * 100);
    }
    Serial.printf("AudioManager: Volume set to %.2f (Internal DAC)\n", globalVolume);
}

float AudioManager::getVolume() {
    return globalVolume;
}

void AudioManager::next() {
    Serial.println("AudioManager: Next Track");
    if (currentMode == MODE_SD) {
        if (mp3Files.size() > 0) {
            if (++currentSongIndex >= (int)mp3Files.size()) currentSongIndex = 0;
            sdPlayer->setIndex(currentSongIndex);
            sdPlayer->play();
        }
    } else {
        a2dp_sink->next();
    }
}

void AudioManager::togglePause() {
    if (currentMode == MODE_SD) {
        sdPaused = !sdPaused;
        Serial.printf("AudioManager: SD %s\n", sdPaused ? "Paused" : "Resumed");
    } else {
        if (a2dp_sink->get_audio_state() == ESP_A2D_AUDIO_STATE_STARTED) {
            a2dp_sink->pause();
        } else {
            a2dp_sink->play();
        }
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
    // Process audio data for visualization during Bluetooth streaming
    const int16_t* s = (int16_t*)data;
    int32_t acc = 0;
    for (uint32_t i = 0; i < len / 2; i++) acc += abs(s[i]);
    int level = acc / (len / 2); 
    if (audioLevelCallback) audioLevelCallback(level);
}