#include "AudioManager.h"

void (*AudioManager::audioLevelCallback)(int) = nullptr;

class RMSStream : public AudioStream {
public:
    RMSStream(AudioStream& output, float& vol, void (*cb)(int)) : out(output), vol(vol), cb(cb) {}
    size_t write(const uint8_t* buffer, size_t size) override {
        const int16_t* samples = (const int16_t*)buffer;
        int32_t sum = 0;
        size_t count = size / 2;
        for (size_t i = 0; i < count; i++) sum += abs(samples[i]);
        if (count > 0 && cb) {
            int level = sum / (count * (vol + 0.01f));
            cb(level);
        }
        return out.write(buffer, size);
    }
private:
    AudioStream& out;
    float& vol;
    void (*cb)(int);
};

static RMSStream* rmsWrapper = nullptr;

AudioManager::AudioManager() : sdSource("/", "mp3") {
    sdPlayer = new AudioPlayer(sdSource, i2s, mp3dec);
    a2dp_sink = new BluetoothA2DPSink(i2s);
}

void AudioManager::begin(uint8_t mode) {
    currentMode = mode;
    auto cfg = i2s.defaultConfig(TX_MODE);
    cfg.pin_bck = I2S_BCLK;
    cfg.pin_ws = I2S_LRCK;
    cfg.pin_data = I2S_DOUT;
    cfg.sample_rate = 44100;
    cfg.channels = 2;
    cfg.bits_per_sample = 16;
    cfg.i2s_format = I2S_STD_FORMAT; 
    i2s.begin(cfg);

    rmsWrapper = new RMSStream(i2s, globalVolume, audioLevelCallback);

    if (mode == MODE_SD) {
        if (USE_SD_CARD && SD_MMC.begin("/sdcard", true)) {
            if (SD_MMC.cardType() != CARD_NONE) {
                sdPlayer->setOutput(*rmsWrapper);
                setVolume(globalVolume); 
                if (sdPlayer->begin()) {
                    mp3Files.clear();
                    walkDir("/", true);
                    sdPlayer->setIndex(0);
                    sdPlayer->play();
                }
            }
        }
    } else {
        a2dp_sink->set_auto_reconnect(true);
        a2dp_sink->set_stream_reader(btDataCallback, true);
        a2dp_sink->start("KingH Music Player");
        setVolume(globalVolume);
    }
}

void AudioManager::update() {
    if (currentMode == MODE_SD && !sdPaused) {
        bool playing = sdPlayer->copy();
        if (!playing && !isPlayingNext && !mp3Files.empty()) {
            currentSongIndex = (currentSongIndex + 1) % mp3Files.size();
            sdPlayer->setIndex(currentSongIndex);
            sdPlayer->play();
            isPlayingNext = true;
        } else if (playing) {
            isPlayingNext = false;
        }
    }
}

void AudioManager::setVolume(float vol) {
    globalVolume = constrain(vol, 0.0f, MAX_VOLUME_LIMIT);
    if (currentMode == MODE_SD) sdPlayer->setVolume(globalVolume);
    else a2dp_sink->set_volume(globalVolume * 127);
}

float AudioManager::getVolume() { return globalVolume; }

void AudioManager::next() {
    if (currentMode == MODE_SD && !mp3Files.empty()) {
        currentSongIndex = (currentSongIndex + 1) % mp3Files.size();
        sdPlayer->setIndex(currentSongIndex);
        sdPlayer->play();
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
        if (f.isDirectory()) { if (recurse) found += walkDir(f.path(), true); }
        else {
            String name = f.name(); name.toLowerCase();
            if (name.endsWith(".mp3")) { mp3Files.push_back(String(f.path())); found++; }
        }
        f = dir.openNextFile();
    }
    return found;
}

void AudioManager::btDataCallback(const uint8_t* data, uint32_t len) {
    const int16_t* samples = (const int16_t*)data;
    int32_t sum = 0;
    size_t count = len / 2;
    for (size_t i = 0; i < count; i++) sum += abs(samples[i]);
    if (count > 0 && audioLevelCallback) audioLevelCallback(sum / count);
}