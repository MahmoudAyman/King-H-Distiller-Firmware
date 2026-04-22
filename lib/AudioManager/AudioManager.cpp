#include "AudioManager.h"

void (*AudioManager::audioLevelCallback)(int) = nullptr;

// RMS Stream that wraps the final output to calculate levels for LEDs
class RMSStream : public AudioStream {
public:
    RMSStream(AudioStream& output, float& vol, void (*cb)(int)) : out(output), vol(vol), cb(cb) {}
    size_t write(const uint8_t* buffer, size_t size) override {
        const int16_t* s = (int16_t*)buffer;
        int32_t acc = 0;
        for (uint32_t i = 0; i < size / 2; i++) acc += abs(s[i]);
        // Normalize level for visualization
        int level = acc / ((size / 2) * sqrt(vol + 0.01));
        if (cb) cb(level);
        return out.write(buffer, size);
    }
private:
    AudioStream& out;
    float& vol;
    void (*cb)(int);
};

static RMSStream* rms;
// Use AnalogAudioStream for Internal DAC output
static AnalogAudioStream analogOut;

AudioManager::AudioManager() : sdSource("/", "mp3") {
    sdPlayer = new AudioPlayer(sdSource, analogOut, mp3dec);
    a2dp_sink = new BluetoothA2DPSink(analogOut);
}

void AudioManager::begin(uint8_t mode) {
    currentMode = mode;
    Serial.printf("AudioManager: Starting in %s mode (Internal DAC)\n", (mode == MODE_BT ? "BT" : "SD"));

    // --- SOFT START RAMP ---
    // Charges your 10uF filter capacitors to midpoint quietly
    for(int i=0; i<128; i++) {
        dacWrite(25, i);
        dacWrite(26, i);
        delay(2);
    }

    // --- CONFIGURE ANALOG OUTPUT ---
    auto cfg = analogOut.defaultConfig(TX_MODE);
    cfg.sample_rate = 44100;
    cfg.channels = 2;
    cfg.bits_per_sample = 16;
    analogOut.begin(cfg);

    // Wrap the analog output with the RMS visualizer
    rms = new RMSStream(analogOut, globalVolume, audioLevelCallback);

    if (mode == MODE_SD) {
        // SD Card in 1-bit mode for hardware reliability (GPIO2/12 conflicts)
        if (USE_SD_CARD && SD_MMC.begin("/sdcard", true)) {
            if (SD_MMC.cardType() != CARD_NONE) {
                sdPlayer->setOutput(*rms);
                setVolume(globalVolume); 
                if (sdPlayer->begin()) {
                    size_t count = walkDir("/", true);
                    Serial.printf("AudioManager: Found %d MP3s\n", (int)count);
                    sdPlayer->setIndex(0);
                    sdPlayer->play();
                }
            }
        } else {
            Serial.println("AudioManager: SD_MMC Fail");
        }
    } else {
        // Bluetooth Setup
        a2dp_sink->set_auto_reconnect(true);
        a2dp_sink->set_stream_reader(btDataCallback, true);
        // BluetoothA2DPSink will output to analogOut defined in constructor
        a2dp_sink->start("KingH Music Player");
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
        a2dp_sink->set_volume(globalVolume * 100);
    }
}

float AudioManager::getVolume() { return globalVolume; }

void AudioManager::next() {
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
    } else {
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